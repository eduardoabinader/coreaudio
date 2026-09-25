#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gigante
{

enum class EndpointDirection
{
    input,
    output
};

enum class EndpointBus
{
    builtIn,
    usb,
    other,
    unknown
};

struct AudioEndpointFacts
{
    std::string endpointId;
    std::optional<std::string> platformId;
    std::string deviceName;
    std::optional<std::string> productName;
    EndpointDirection direction{EndpointDirection::input};
    EndpointBus bus{EndpointBus::unknown};
    std::uint32_t channelCount{0};
    std::vector<double> sampleRates;
};

struct EndpointInventory
{
    std::uint64_t generation{0};
    std::vector<AudioEndpointFacts> endpoints;
};

class EndpointFactSource
{
public:
    virtual ~EndpointFactSource() = default;

    [[nodiscard]] virtual EndpointInventory snapshot() const = 0;
};

struct EndpointRolePolicy
{
    EndpointDirection requiredDirection{EndpointDirection::input};
    std::vector<EndpointBus> allowedBuses;
    std::uint32_t minimumChannelCount{0};
    double minimumSampleRate{0.0};
};

struct EndpointSelectionRequest
{
    std::optional<std::string> requestedEndpointId;
};

enum class EndpointRejectionReason
{
    invalidIdentity,
    duplicateIdentity,
    directionMismatch,
    busNotAllowed,
    unknownBusRequiresExplicitRequest,
    insufficientChannels,
    insufficientSampleRate,
    notRequested
};

enum class EndpointDisposition
{
    eligible,
    selected,
    rejected
};

struct EndpointDecision
{
    std::size_t inventoryIndex{0};
    std::string endpointId;
    EndpointDisposition disposition{EndpointDisposition::rejected};
    std::vector<EndpointRejectionReason> rejectionReasons;
};

enum class EndpointSelectionStatus
{
    invalidPolicy,
    noEligibleEndpoints,
    oneEligibleEndpoint,
    multipleEligibleEndpoints,
    requestedEndpointMissing,
    requestedEndpointRejected,
    selected
};

struct EndpointSelectionResult
{
    std::uint64_t inventoryGeneration{0};
    std::optional<std::string> requestedEndpointId;
    std::optional<std::string> effectiveEndpointId;
    EndpointSelectionStatus status{
        EndpointSelectionStatus::noEligibleEndpoints};
    std::vector<EndpointDecision> decisions;
};

[[nodiscard]] EndpointSelectionResult evaluateEndpointSelection(
    const EndpointInventory& inventory,
    const EndpointRolePolicy& policy,
    const EndpointSelectionRequest& request);

[[nodiscard]] const char* endpointDirectionName(
    EndpointDirection direction) noexcept;
[[nodiscard]] const char* endpointBusName(EndpointBus bus) noexcept;
[[nodiscard]] const char* endpointRejectionReasonName(
    EndpointRejectionReason reason) noexcept;
[[nodiscard]] const char* endpointSelectionStatusName(
    EndpointSelectionStatus status) noexcept;

} // namespace gigante
