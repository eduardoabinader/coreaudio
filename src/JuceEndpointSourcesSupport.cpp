#include "JuceEndpointSourcesSupport.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace gigante::detail
{
namespace
{

std::string endpointIdentity(const std::string& typeName,
                             const EndpointDirection direction,
                             const std::string& deviceName,
                             const std::optional<std::string>& platformId)
{
    std::string result = typeName;
    result += direction == EndpointDirection::input ? "|input|" : "|output|";
    if (platformId)
        result += *platformId;
    else
        result += "unavailable";
    result += "|";
    result += deviceName;
    return result;
}

AudioEndpoint readEndpoint(juce::AudioIODeviceType& type,
                           const juce::String& deviceName,
                           const EndpointDirection direction,
                           const MetadataMap& metadata)
{
    AudioEndpoint endpoint;
    endpoint.deviceName = deviceName.toStdString();
    endpoint.direction = direction;
    const auto metadataPosition = metadata.find(
        {direction, endpoint.deviceName});
    if (metadataPosition != metadata.end())
    {
        endpoint.platformId = metadataPosition->second.platformId;
        endpoint.productName = metadataPosition->second.productName;
        endpoint.bus = metadataPosition->second.bus;
    }

    const juce::String empty;
    std::unique_ptr<juce::AudioIODevice> device{
        direction == EndpointDirection::input
            ? type.createDevice(empty, deviceName)
            : type.createDevice(deviceName, empty)};
    if (device != nullptr)
    {
        const int channels = direction == EndpointDirection::input
            ? device->getInputChannelNames().size()
            : device->getOutputChannelNames().size();
        if (channels > 0)
            endpoint.channelCount = static_cast<std::uint32_t>(channels);

        for (const double rate : device->getAvailableSampleRates())
        {
            if (std::isfinite(rate) && rate > 0.0)
                endpoint.sampleRates.push_back(rate);
        }
        std::sort(endpoint.sampleRates.begin(), endpoint.sampleRates.end());
        endpoint.sampleRates.erase(
            std::unique(endpoint.sampleRates.begin(), endpoint.sampleRates.end()),
            endpoint.sampleRates.end());
    }

    endpoint.endpointId = endpointIdentity(
        type.getTypeName().toStdString(),
        direction,
        endpoint.deviceName,
        endpoint.platformId);
    return endpoint;
}

bool endpointsEqual(const AudioEndpoint& left, const AudioEndpoint& right)
{
    return left.endpointId == right.endpointId
        && left.platformId == right.platformId
        && left.deviceName == right.deviceName
        && left.productName == right.productName
        && left.direction == right.direction
        && left.bus == right.bus
        && left.channelCount == right.channelCount
        && left.sampleRates == right.sampleRates;
}

} // namespace

std::vector<AudioEndpoint> readJuceEndpoints(const MetadataMap& metadata)
{
    juce::AudioDeviceManager manager;
    std::vector<AudioEndpoint> result;
    for (auto* const type : manager.getAvailableDeviceTypes())
    {
        if (type == nullptr)
            continue;
        auto endpoints = readJuceEndpoints(*type, metadata);
        result.insert(result.end(),
                      std::make_move_iterator(endpoints.begin()),
                      std::make_move_iterator(endpoints.end()));
    }
    std::sort(result.begin(), result.end(),
              [](const AudioEndpoint& left, const AudioEndpoint& right)
              {
                  return left.endpointId < right.endpointId;
              });
    return result;
}

std::vector<AudioEndpoint> readJuceEndpoints(
    juce::AudioIODeviceType& type,
    const MetadataMap& metadata)
{
    std::vector<AudioEndpoint> result;
    for (const auto& name : type.getDeviceNames(true))
    {
        result.push_back(readEndpoint(
            type, name, EndpointDirection::input, metadata));
    }
    for (const auto& name : type.getDeviceNames(false))
    {
        result.push_back(readEndpoint(
            type, name, EndpointDirection::output, metadata));
    }
    std::sort(result.begin(), result.end(),
              [](const AudioEndpoint& left, const AudioEndpoint& right)
              {
                  return left.endpointId < right.endpointId;
              });
    return result;
}

bool endpointListsEqual(const std::vector<AudioEndpoint>& left,
                        const std::vector<AudioEndpoint>& right)
{
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(), endpointsEqual);
}

} // namespace gigante::detail
