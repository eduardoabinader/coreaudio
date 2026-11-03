#pragma once

#include "gigante/EndpointPolicy.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gigante
{
// Native endpoint transport, independent of the legacy coarse AudioBus policy.
enum class AudioTransport
{
    unknown, builtIn, wired, usb, bluetoothA2dp, bluetoothHfp,
    bluetoothLe, bluetoothHearingAid, other
};

enum class AudioLinkAvailability { unknown, available, unavailable };

struct AudioLink
{
    AudioEndpoint endpoint;
    AudioTransport transport{AudioTransport::unknown};
    AudioLinkAvailability availability{AudioLinkAvailability::unknown};
    // Observed carrying this application's output, never the user's request.
    bool selected{false};
    // Preserve native classification even when the normalized transport is unknown.
    std::string nativeTransport{};
};

// available means an accepted observation, not effective routing or playback eligibility.
enum class OutputRouteStatus { unavailable, available, failed };
// The producer must establish effective routing; the model never infers it.
enum class OutputRouteBasis { unknown, inventory, requested, effective };
enum class OutputRouteUpdate { applied, staleGeneration, invalidObservation };
// Effective-route observation and complete output enumeration are independent.
enum class OutputInventoryStatus { unknown, partial, complete };

// Non-realtime value model. All access must be serialized by its owner. Native
// callbacks must marshal updates to that owner; audio callbacks never read it.
// Updates own their link values and publish a whole generation, not partial edits.
class OutputRoute final
{
public:
    [[nodiscard]] OutputRouteUpdate updateLinks(std::uint64_t generation,
        std::vector<AudioLink> links, OutputRouteBasis basis,
        OutputInventoryStatus inventoryStatus = OutputInventoryStatus::unknown,
        std::string inventoryContext = {});
    [[nodiscard]] OutputRouteUpdate markUnavailable(std::uint64_t generation,
        std::string context);
    [[nodiscard]] OutputRouteUpdate markFailed(std::uint64_t generation,
        std::string context);
    // This records intent only and never changes selected links or observation age.
    void setRequestedEndpointId(std::optional<std::string> endpointId);

    [[nodiscard]] const std::vector<AudioLink>& links() const noexcept;
    [[nodiscard]] OutputRouteStatus status() const noexcept;
    [[nodiscard]] OutputRouteBasis basis() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] const std::string& context() const noexcept;
    [[nodiscard]] OutputInventoryStatus inventoryStatus() const noexcept;
    [[nodiscard]] const std::string& inventoryContext() const noexcept;
    [[nodiscard]] const std::optional<std::string>& requestedEndpointId() const noexcept;

private:
    void invalidate(std::uint64_t generation, OutputRouteStatus status,
        std::string context);

    std::vector<AudioLink> currentLinks;
    OutputRouteStatus currentStatus{OutputRouteStatus::unavailable};
    OutputRouteBasis currentBasis{OutputRouteBasis::unknown};
    std::uint64_t currentGeneration{0};
    std::string currentContext;
    OutputInventoryStatus currentInventoryStatus{OutputInventoryStatus::unknown};
    std::string currentInventoryContext;
    std::optional<std::string> requestedId;
};
} // namespace gigante
