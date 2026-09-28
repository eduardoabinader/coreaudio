#pragma once

#include "gigante/EndpointPolicy.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace juce
{
class AudioIODeviceType;
}

namespace gigante::detail
{

struct EndpointMetadata
{
    std::optional<std::string> platformId;
    std::optional<std::string> productName;
    AudioBus bus{AudioBus::unknown};
};

using MetadataKey = std::pair<EndpointDirection, std::string>;
using MetadataMap = std::map<MetadataKey, EndpointMetadata>;

[[nodiscard]] std::vector<AudioEndpoint> readJuceEndpoints(
    const MetadataMap& metadata);

[[nodiscard]] std::vector<AudioEndpoint> readJuceEndpoints(
    juce::AudioIODeviceType& type,
    const MetadataMap& metadata);

[[nodiscard]] bool endpointListsEqual(
    const std::vector<AudioEndpoint>& left,
    const std::vector<AudioEndpoint>& right);

} // namespace gigante::detail
