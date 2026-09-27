#include "AppleHostFactsSupport.h"

#include "gigante/Revision.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <sys/sysctl.h>

namespace gigante::platformDetail
{
namespace
{

template <typename Value>
std::optional<Value> readSysctlValue(const char* const name)
{
    Value value{};
    std::size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0
        || size != sizeof(value))
        return std::nullopt;

    return value;
}

std::optional<std::string> readSysctlString(const char* const name)
{
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size <= 1U)
        return std::nullopt;

    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0 || size <= 1U)
        return std::nullopt;

    value.resize(size - 1U);
    return value;
}

} // namespace

HostFacts readAppleHostFacts(
    const PlatformKind platform,
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    HostFacts facts;
    facts.platform = platform;
    facts.platformName = platformKindName(platform);
    facts.logicalCoreCount = readSysctlValue<std::uint32_t>("hw.logicalcpu");
    facts.physicalMemoryBytes = readSysctlValue<std::uint64_t>("hw.memsize");
    facts.architecture = readSysctlString("hw.machine");
    facts.buildFingerprint = readSysctlString("kern.osversion");
    facts.sourceRevision = revision;
    facts.realtimeScheduling = realtimeScheduling;
    return facts;
}

} // namespace gigante::platformDetail
