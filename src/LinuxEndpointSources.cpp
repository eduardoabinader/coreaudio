#include "gigante/PlatformEndpointSources.h"

#include "JuceEndpointSourcesSupport.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <sys/eventfd.h>
#include <sys/inotify.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gigante
{
namespace
{

class LinuxEndpointFactSource final : public EndpointFactSource
{
public:
    EndpointInventory snapshot() const override
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::unique_ptr<juce::AudioIODeviceType> type{
            juce::AudioIODeviceType::createAudioIODeviceType_ALSA()};
        std::vector<AudioEndpoint> endpoints;
        if (type != nullptr)
        {
            type->scanForDevices();
            endpoints = detail::readJuceEndpoints(*type, {});
        }
        if (!initialized
            || !detail::endpointListsEqual(previousEndpoints, endpoints))
        {
            ++generation;
            previousEndpoints = endpoints;
            initialized = true;
        }
        return {generation, std::move(endpoints)};
    }

private:
    mutable std::mutex mutex;
    mutable std::vector<AudioEndpoint> previousEndpoints;
    mutable std::uint64_t generation{0U};
    mutable bool initialized{false};
};

struct InotifyState
{
    ~InotifyState()
    {
        if (inotifyFileDescriptor >= 0)
            ::close(inotifyFileDescriptor);
        if (wakeFileDescriptor >= 0)
            ::close(wakeFileDescriptor);
    }

    EndpointChangeSource::ChangeSignal signal;
    std::atomic<bool> stopping{false};
    int inotifyFileDescriptor{-1};
    int wakeFileDescriptor{-1};
};

void runInotify(const std::shared_ptr<InotifyState>& state) noexcept
{
    std::array<char, 4096> events{};
    while (!state->stopping.load(std::memory_order_acquire))
    {
        std::array<pollfd, 2> descriptors{{
            {state->inotifyFileDescriptor, POLLIN, 0},
            {state->wakeFileDescriptor, POLLIN, 0}
        }};
        const int result = ::poll(
            descriptors.data(),
            static_cast<nfds_t>(descriptors.size()),
            -1);
        if (result < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        if ((descriptors[1].revents & POLLIN) != 0)
            break;
        if ((descriptors[0].revents & POLLIN) == 0)
            continue;

        bool receivedChange = false;
        for (;;)
        {
            const auto bytesRead = ::read(
                state->inotifyFileDescriptor,
                events.data(),
                events.size());
            if (bytesRead > 0)
            {
                receivedChange = true;
                continue;
            }
            if (bytesRead < 0 && errno == EINTR)
                continue;
            break;
        }
        if (!receivedChange
            || state->stopping.load(std::memory_order_acquire))
        {
            continue;
        }
        try
        {
            state->signal();
        }
        catch (...)
        {
            state->stopping.store(true, std::memory_order_release);
        }
    }
}

class LinuxEndpointChangeSource final : public EndpointChangeSource
{
public:
    ~LinuxEndpointChangeSource() override
    {
        stop();
    }

    EndpointChangeStartResult start(ChangeSignal signal) override
    {
        if (!signal)
            return {EndpointChangeStartStatus::failed,
                    "endpoint change signal is empty"};

        std::lock_guard<std::mutex> lock(lifecycleMutex);
        if (worker.joinable())
            return {EndpointChangeStartStatus::failed,
                    "Linux endpoint change source is already running"};

        auto newState = std::make_shared<InotifyState>();
        newState->signal = std::move(signal);
        newState->inotifyFileDescriptor = ::inotify_init1(
            IN_NONBLOCK | IN_CLOEXEC);
        if (newState->inotifyFileDescriptor < 0)
            return startFailure("inotify_init1");

        constexpr std::uint32_t changeMask =
            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB;
        if (::inotify_add_watch(newState->inotifyFileDescriptor,
                                "/dev/snd",
                                changeMask) < 0)
        {
            if (errno == EACCES || errno == EPERM)
            {
                return {EndpointChangeStartStatus::permissionDenied,
                        errorContext("cannot watch /dev/snd")};
            }
            if (errno == ENOENT)
            {
                return {EndpointChangeStartStatus::unavailable,
                        "/dev/snd is unavailable; no Linux endpoint change "
                        "signal exists"};
            }
            return startFailure("cannot watch /dev/snd");
        }

        newState->wakeFileDescriptor = ::eventfd(0U, EFD_CLOEXEC | EFD_NONBLOCK);
        if (newState->wakeFileDescriptor < 0)
            return startFailure("eventfd");

        try
        {
            worker = std::thread(runInotify, newState);
        }
        catch (const std::exception& error)
        {
            return {EndpointChangeStartStatus::failed, error.what()};
        }
        state = std::move(newState);
        return {EndpointChangeStartStatus::started,
                "watching /dev/snd with inotify on a non-realtime thread"};
    }

    void stop() noexcept override
    {
        std::thread workerToJoin;
        {
            std::lock_guard<std::mutex> lock(lifecycleMutex);
            if (!worker.joinable())
                return;
            if (state != nullptr)
            {
                state->stopping.store(true, std::memory_order_release);
                constexpr std::uint64_t wakeValue = 1U;
                static_cast<void>(::write(
                    state->wakeFileDescriptor,
                    &wakeValue,
                    sizeof(wakeValue)));
            }
            if (worker.get_id() == std::this_thread::get_id())
                worker.detach();
            else
                workerToJoin = std::move(worker);
            state.reset();
        }
        if (workerToJoin.joinable())
            workerToJoin.join();
    }

private:
    static std::string errorContext(const char* operation)
    {
        return std::string{operation} + ": " + std::strerror(errno);
    }

    static EndpointChangeStartResult startFailure(const char* operation)
    {
        return {EndpointChangeStartStatus::failed, errorContext(operation)};
    }

    std::mutex lifecycleMutex;
    std::shared_ptr<InotifyState> state;
    std::thread worker;
};

} // namespace

PlatformEndpointSources createPlatformEndpointSources()
{
    PlatformEndpointSources result;
    try
    {
        result.factSource = std::make_unique<LinuxEndpointFactSource>();
        result.changeSource = std::make_unique<LinuxEndpointChangeSource>();
        result.status = PlatformEndpointSourceStatus::available;
        result.context =
            "JUCE endpoint inventory with /dev/snd inotify change signals; "
            "Linux bus metadata is not inferred";
    }
    catch (const std::exception& error)
    {
        result.status = PlatformEndpointSourceStatus::initializationFailed;
        result.context = error.what();
        result.factSource.reset();
        result.changeSource.reset();
    }
    return result;
}

} // namespace gigante
