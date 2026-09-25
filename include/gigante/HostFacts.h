#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace gigante
{

enum class PlatformKind
{
    unknownHost,
    androidHost,
    iosHost,
    linuxHost,
    macOsHost
};

enum class RealtimeSchedulingState
{
    notRequested,
    applied,
    rejected,
    unavailable
};

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
    PlatformKind platform{PlatformKind::unknownHost};
    std::string platformName;
    std::optional<std::uint32_t> logicalCoreCount;
    std::optional<std::uint64_t> physicalMemoryBytes;
    std::optional<std::string> architecture;
    std::optional<std::string> buildFingerprint;
    std::string sourceRevision;
    RealtimeSchedulingOutcome realtimeScheduling;
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
[[nodiscard]] const char* platformKindName(PlatformKind platform) noexcept;
[[nodiscard]] const char* realtimeSchedulingStateName(
    RealtimeSchedulingState state) noexcept;

} // namespace gigante
