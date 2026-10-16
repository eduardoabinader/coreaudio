#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gigante
{

enum class RealtimeSchedulingState
{
    notRequested,
    applied,
    rejected,
    unavailable
};

// Generic, extensible platform capability detection: a capability domain
// (rendering is the first) is an enum whose values are detected at read
// time by one probe per platform family, never assumed from platform
// identity alone. A VM or container may run macOS or Linux and still lack
// video; an undetectable case is absent from the vector, never guessed.
enum class RenderCapability
{
    audio,
    video
};

[[nodiscard]] const char* renderCapabilityName(
    RenderCapability capability) noexcept;

struct RealtimeSchedulingOutcome
{
    RealtimeSchedulingState state{RealtimeSchedulingState::notRequested};
    std::string requestedPolicy;
    int requestedPriority{0};
    std::string observedPolicy;
    int observedPriority{0};
    std::string detail;
};

struct HostFacts
{
    std::string platformName;
    std::optional<std::uint32_t> logicalCoreCount;
    std::optional<std::uint64_t> physicalMemoryBytes;
    std::optional<std::string> architecture;
    std::optional<std::string> buildFingerprint;
    std::string sourceRevision;
    RealtimeSchedulingOutcome realtimeScheduling;
    std::vector<RenderCapability> renderCapabilities;
};

class HostFactsSource
{
public:
    virtual ~HostFactsSource() = default;

    [[nodiscard]] virtual HostFacts read(
        const RealtimeSchedulingOutcome& realtimeScheduling) const = 0;
};

[[nodiscard]] std::unique_ptr<HostFactsSource> createHostFactsSource();
[[nodiscard]] std::string formatHostFacts(const HostFacts& facts);
[[nodiscard]] const char* realtimeSchedulingStateName(
    RealtimeSchedulingState state) noexcept;

} // namespace gigante
