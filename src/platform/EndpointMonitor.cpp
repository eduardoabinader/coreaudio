#include "gigante/EndpointMonitor.h"

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace gigante
{
namespace
{

class MonitorState final
{
public:
    explicit MonitorState(EndpointFactSource& factSourceIn)
        : factSource(factSourceIn)
    {
    }

    void activate(EndpointMonitor::Callback callbackIn)
    {
        std::lock_guard<std::mutex> lock(mutex);
        callback = std::move(callbackIn);
        acceptingCallbacks = true;
    }

    void deactivate() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        acceptingCallbacks = false;
    }

    void waitForCallbacks() noexcept
    {
        std::unique_lock<std::mutex> lock(mutex);
        const auto callingThread = std::this_thread::get_id();
        const auto callbacksOnCallingThread = static_cast<std::size_t>(
            std::count(callbackThreads.begin(),
                       callbackThreads.end(),
                       callingThread));
        callbackFinished.wait(lock,
                              [this, callbacksOnCallingThread]
                              {
                                  return callbacksInFlight
                                      <= callbacksOnCallingThread;
                              });
        callback = {};
    }

    [[nodiscard]] bool running() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        return acceptingCallbacks;
    }

    void publish(const EndpointMonitorEventCause cause)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!acceptingCallbacks)
                return;
        }

        EndpointMonitorEvent event;
        event.cause = cause;
        try
        {
            event.inventory = factSource.snapshot();
            event.status = EndpointMonitorEventStatus::inventory;
        }
        catch (const std::exception& error)
        {
            event.status = EndpointMonitorEventStatus::snapshotFailed;
            event.context = error.what();
        }
        catch (...)
        {
            event.status = EndpointMonitorEventStatus::snapshotFailed;
            event.context = "endpoint fact source threw an unknown exception";
        }

        EndpointMonitor::Callback callbackCopy;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!acceptingCallbacks)
                return;
            callbackCopy = callback;
            ++callbacksInFlight;
            callbackThreads.push_back(std::this_thread::get_id());
        }

        try
        {
            callbackCopy(event);
        }
        catch (...)
        {
            completeCallback();
            throw;
        }
        completeCallback();
    }

private:
    void completeCallback() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto thread = std::this_thread::get_id();
        const auto position = std::find(callbackThreads.begin(),
                                        callbackThreads.end(),
                                        thread);
        if (position != callbackThreads.end())
            callbackThreads.erase(position);
        --callbacksInFlight;
        if (callbacksInFlight == 0U)
            callbackFinished.notify_all();
    }

    EndpointFactSource& factSource;
    mutable std::mutex mutex;
    std::condition_variable callbackFinished;
    EndpointMonitor::Callback callback;
    std::vector<std::thread::id> callbackThreads;
    std::size_t callbacksInFlight{0U};
    bool acceptingCallbacks{false};
};

EndpointMonitorStartStatus mapStartStatus(
    const EndpointChangeStartStatus status) noexcept
{
    switch (status)
    {
        case EndpointChangeStartStatus::started:
            return EndpointMonitorStartStatus::started;
        case EndpointChangeStartStatus::unavailable:
            return EndpointMonitorStartStatus::changeSourceUnavailable;
        case EndpointChangeStartStatus::permissionDenied:
            return EndpointMonitorStartStatus::changeSourcePermissionDenied;
        case EndpointChangeStartStatus::failed:
            return EndpointMonitorStartStatus::changeSourceFailed;
    }
    return EndpointMonitorStartStatus::changeSourceFailed;
}

} // namespace

struct EndpointMonitor::Implementation
{
    Implementation(EndpointFactSource& factSource,
                   EndpointChangeSource& changeSourceIn)
        : state(std::make_shared<MonitorState>(factSource)),
          changeSource(changeSourceIn)
    {
    }

    std::shared_ptr<MonitorState> state;
    EndpointChangeSource& changeSource;
    std::mutex lifecycleMutex;
};

EndpointMonitor::EndpointMonitor(EndpointFactSource& factSource,
                                 EndpointChangeSource& changeSource)
    : implementation(
          std::make_unique<Implementation>(factSource, changeSource))
{
}

EndpointMonitor::~EndpointMonitor()
{
    stop();
}

EndpointMonitorStartResult EndpointMonitor::start(Callback callback)
{
    EndpointMonitorStartResult result;
    if (!callback)
    {
        result.status = EndpointMonitorStartStatus::invalidCallback;
        result.context = "endpoint monitor callback is empty";
        return result;
    }

    std::unique_lock<std::mutex> lifecycleLock(
        implementation->lifecycleMutex);
    if (implementation->state->running())
    {
        result.effectiveRunning = true;
        result.status = EndpointMonitorStartStatus::alreadyRunning;
        result.context = "endpoint monitor is already running";
        return result;
    }

    const std::weak_ptr<MonitorState> weakState{implementation->state};
    EndpointChangeStartResult sourceResult;
    try
    {
        sourceResult = implementation->changeSource.start(
            [weakState]
            {
                if (const auto state = weakState.lock())
                    state->publish(EndpointMonitorEventCause::changeSignal);
            });
    }
    catch (const std::exception& error)
    {
        sourceResult.status = EndpointChangeStartStatus::failed;
        sourceResult.context = error.what();
    }
    catch (...)
    {
        sourceResult.status = EndpointChangeStartStatus::failed;
        sourceResult.context =
            "endpoint change source threw an unknown exception";
    }

    result.status = mapStartStatus(sourceResult.status);
    result.context = std::move(sourceResult.context);
    if (sourceResult.status != EndpointChangeStartStatus::started)
    {
        implementation->changeSource.stop();
        return result;
    }

    implementation->state->activate(std::move(callback));
    result.effectiveRunning = true;
    lifecycleLock.unlock();

    try
    {
        implementation->state->publish(
            EndpointMonitorEventCause::initialSnapshot);
    }
    catch (...)
    {
        stop();
        throw;
    }
    return result;
}

void EndpointMonitor::stop() noexcept
{
    std::lock_guard<std::mutex> lifecycleLock(
        implementation->lifecycleMutex);
    if (!implementation->state->running())
        return;

    implementation->state->deactivate();
    implementation->changeSource.stop();
    implementation->state->waitForCallbacks();
}

bool EndpointMonitor::running() const noexcept
{
    return implementation->state->running();
}

const char* endpointChangeStartStatusName(
    const EndpointChangeStartStatus status) noexcept
{
    switch (status)
    {
        case EndpointChangeStartStatus::started:
            return "started";
        case EndpointChangeStartStatus::unavailable:
            return "unavailable";
        case EndpointChangeStartStatus::permissionDenied:
            return "permissionDenied";
        case EndpointChangeStartStatus::failed:
            return "failed";
    }
    return "unknown";
}

const char* endpointMonitorEventCauseName(
    const EndpointMonitorEventCause cause) noexcept
{
    switch (cause)
    {
        case EndpointMonitorEventCause::initialSnapshot:
            return "initialSnapshot";
        case EndpointMonitorEventCause::changeSignal:
            return "changeSignal";
    }
    return "unknown";
}

const char* endpointMonitorEventStatusName(
    const EndpointMonitorEventStatus status) noexcept
{
    switch (status)
    {
        case EndpointMonitorEventStatus::inventory:
            return "inventory";
        case EndpointMonitorEventStatus::snapshotFailed:
            return "snapshotFailed";
    }
    return "unknown";
}

const char* endpointMonitorStartStatusName(
    const EndpointMonitorStartStatus status) noexcept
{
    switch (status)
    {
        case EndpointMonitorStartStatus::started:
            return "started";
        case EndpointMonitorStartStatus::alreadyRunning:
            return "alreadyRunning";
        case EndpointMonitorStartStatus::invalidCallback:
            return "invalidCallback";
        case EndpointMonitorStartStatus::changeSourceUnavailable:
            return "changeSourceUnavailable";
        case EndpointMonitorStartStatus::changeSourcePermissionDenied:
            return "changeSourcePermissionDenied";
        case EndpointMonitorStartStatus::changeSourceFailed:
            return "changeSourceFailed";
    }
    return "unknown";
}

} // namespace gigante
