#include "gigante/PlatformEndpointSources.h"

#include "JuceEndpointSourcesSupport.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gigante
{
namespace
{

template <typename Value>
std::optional<Value> readProperty(
    const AudioObjectID object,
    const AudioObjectPropertySelector selector,
    const AudioObjectPropertyScope scope)
{
    const AudioObjectPropertyAddress address{
        selector, scope, kAudioObjectPropertyElementMain};
    Value value{};
    UInt32 size = static_cast<UInt32>(sizeof(value));
    if (AudioObjectGetPropertyData(
            object, &address, 0U, nullptr, &size, &value) != noErr
        || size != sizeof(value))
    {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> readStringProperty(
    const AudioObjectID object,
    const AudioObjectPropertySelector selector)
{
    const auto value = readProperty<CFStringRef>(
        object, selector, kAudioObjectPropertyScopeGlobal);
    if (!value || *value == nullptr)
        return std::nullopt;

    const std::string result = juce::String::fromCFString(*value).toStdString();
    CFRelease(*value);
    if (result.empty())
        return std::nullopt;
    return result;
}

std::uint32_t readChannelCount(const AudioObjectID object,
                               const AudioObjectPropertyScope scope)
{
    const AudioObjectPropertyAddress address{
        kAudioDevicePropertyStreamConfiguration,
        scope,
        kAudioObjectPropertyElementMain};
    UInt32 size = 0U;
    if (AudioObjectGetPropertyDataSize(
            object, &address, 0U, nullptr, &size) != noErr
        || size < sizeof(AudioBufferList))
    {
        return 0U;
    }

    auto storage = std::make_unique<std::byte[]>(size);
    if (AudioObjectGetPropertyData(
            object, &address, 0U, nullptr, &size, storage.get()) != noErr)
    {
        return 0U;
    }

    const auto* const buffers =
        reinterpret_cast<const AudioBufferList*>(storage.get());
    std::uint32_t result = 0U;
    for (UInt32 index = 0U; index < buffers->mNumberBuffers; ++index)
        result += buffers->mBuffers[index].mNumberChannels;
    return result;
}

AudioBus busForTransportType(const std::optional<UInt32> transportType)
{
    if (!transportType)
        return AudioBus::unknown;
    if (*transportType == kAudioDeviceTransportTypeBuiltIn)
        return AudioBus::builtIn;
    if (*transportType == kAudioDeviceTransportTypeUSB)
        return AudioBus::usb;
    return AudioBus::other;
}

std::vector<AudioObjectID> readAudioDeviceIds()
{
    const AudioObjectPropertyAddress address{
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain};
    UInt32 size = 0U;
    if (AudioObjectGetPropertyDataSize(
            kAudioObjectSystemObject,
            &address,
            0U,
            nullptr,
            &size) != noErr
        || size % sizeof(AudioObjectID) != 0U)
    {
        return {};
    }

    std::vector<AudioObjectID> result(size / sizeof(AudioObjectID));
    if (!result.empty()
        && AudioObjectGetPropertyData(
               kAudioObjectSystemObject,
               &address,
               0U,
               nullptr,
               &size,
               result.data()) != noErr)
    {
        return {};
    }
    return result;
}

struct DirectionalMetadata
{
    EndpointDirection direction{EndpointDirection::input};
    std::string name;
    detail::EndpointMetadata metadata;
};

detail::MetadataMap readCoreAudioMetadata()
{
    std::vector<DirectionalMetadata> inputs;
    std::vector<DirectionalMetadata> outputs;
    for (const AudioObjectID deviceId : readAudioDeviceIds())
    {
        const auto name = readStringProperty(
            deviceId, kAudioDevicePropertyDeviceNameCFString);
        if (!name)
            continue;

        detail::EndpointMetadata metadata;
        metadata.platformId = readStringProperty(
            deviceId, kAudioDevicePropertyDeviceUID);
        metadata.productName = name;
        metadata.bus = busForTransportType(readProperty<UInt32>(
            deviceId,
            kAudioDevicePropertyTransportType,
            kAudioObjectPropertyScopeGlobal));

        if (readChannelCount(
                deviceId, kAudioDevicePropertyScopeInput) > 0U)
        {
            inputs.push_back(
                {EndpointDirection::input, *name, metadata});
        }
        if (readChannelCount(
                deviceId, kAudioDevicePropertyScopeOutput) > 0U)
        {
            outputs.push_back(
                {EndpointDirection::output, *name, metadata});
        }
    }

    detail::MetadataMap result;
    const auto addDirection = [&result](
        const std::vector<DirectionalMetadata>& entries)
    {
        juce::StringArray names;
        for (const auto& entry : entries)
            names.add(juce::String::fromUTF8(entry.name.c_str()));
        names.appendNumbersToDuplicates(false, true);

        for (int index = 0; index < names.size(); ++index)
        {
            const auto position = static_cast<std::size_t>(index);
            result.emplace(
                detail::MetadataKey{
                    entries[position].direction,
                    names[index].toStdString()},
                entries[position].metadata);
        }
    };
    addDirection(inputs);
    addDirection(outputs);
    return result;
}

class MacOsEndpointFactSource final : public EndpointFactSource
{
public:
    EndpointInventory snapshot() const override
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto endpoints = detail::readJuceEndpoints(readCoreAudioMetadata());
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

class MacOsEndpointChangeSource final
    : public EndpointChangeSource,
      private juce::AudioIODeviceType::Listener
{
public:
    MacOsEndpointChangeSource()
    {
        for (auto* const type : deviceManager.getAvailableDeviceTypes())
        {
            if (type != nullptr)
                deviceTypes.push_back(type);
        }
    }

    ~MacOsEndpointChangeSource() override
    {
        stop();
    }

    EndpointChangeStartResult start(ChangeSignal signalIn) override
    {
        if (!signalIn)
            return {EndpointChangeStartStatus::failed,
                    "endpoint change signal is empty"};

        std::lock_guard<std::mutex> lock(mutex);
        if (active)
            return {EndpointChangeStartStatus::failed,
                    "macOS endpoint change source is already running"};
        if (deviceTypes.empty())
            return {EndpointChangeStartStatus::unavailable,
                    "JUCE exposed no CoreAudio device type"};

        signal = std::move(signalIn);
        active = true;
        for (auto* const type : deviceTypes)
            type->addListener(this);
        return {EndpointChangeStartStatus::started,
                "JUCE CoreAudio notification on the JUCE message thread"};
    }

    void stop() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!active)
                return;
            active = false;
            signal = {};
        }
        for (auto* const type : deviceTypes)
            type->removeListener(this);
    }

private:
    void audioDeviceListChanged() override
    {
        ChangeSignal signalToCall;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!active)
                return;
            signalToCall = signal;
        }
        signalToCall();
    }

    juce::AudioDeviceManager deviceManager;
    std::vector<juce::AudioIODeviceType*> deviceTypes;
    std::mutex mutex;
    ChangeSignal signal;
    bool active{false};
};

} // namespace

PlatformEndpointSources createPlatformEndpointSources()
{
    PlatformEndpointSources result;
    try
    {
        result.factSource = std::make_unique<MacOsEndpointFactSource>();
        result.changeSource = std::make_unique<MacOsEndpointChangeSource>();
        result.status = PlatformEndpointSourceStatus::available;
        result.context =
            "JUCE CoreAudio inventory/notification with transport-type facts";
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
