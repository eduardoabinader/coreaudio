#include "gigante/HostFacts.h"

#include <sstream>

namespace gigante
{

const char* platformKindName(const PlatformKind platform) noexcept
{
    switch (platform)
    {
        case PlatformKind::unknownHost:
            return "Unknown";
        case PlatformKind::androidHost:
            return "Android";
        case PlatformKind::iosHost:
            return "iOS";
        case PlatformKind::linuxHost:
            return "Linux";
        case PlatformKind::macOsHost:
            return "macOS";
    }

    return "Unknown";
}

const char* realtimeSchedulingStateName(
    const RealtimeSchedulingState state) noexcept
{
    switch (state)
    {
        case RealtimeSchedulingState::notRequested:
            return "notRequested";
        case RealtimeSchedulingState::applied:
            return "applied";
        case RealtimeSchedulingState::rejected:
            return "rejected";
        case RealtimeSchedulingState::unavailable:
            return "unavailable";
    }

    return "unavailable";
}

std::string formatHostFacts(const HostFacts& facts)
{
    const auto gibibytes = facts.physicalMemoryBytes.has_value()
        ? std::to_string(*facts.physicalMemoryBytes
                         / (1024ULL * 1024ULL * 1024ULL))
        : std::string{"unavailable"};

    std::ostringstream output;
    output << facts.platformName << '/'
           << facts.architecture.value_or("unavailable")
           << "/cores=";
    if (facts.logicalCoreCount.has_value())
        output << *facts.logicalCoreCount;
    else
        output << "unavailable";
    output << "/ram=" << gibibytes;
    if (facts.physicalMemoryBytes.has_value())
        output << "GiB";
    output << "/build=" << facts.sourceRevision;
    return output.str();
}

} // namespace gigante
