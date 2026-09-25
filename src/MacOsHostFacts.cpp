#include "gigante/HostFacts.h"
#include "gigante/Revision.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <sys/sysctl.h>

namespace gigante
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

class MacOsHostFactsSource final : public HostFactsSource
{
public:
    [[nodiscard]] HostFacts read(
        const RealtimeSchedulingOutcome& realtimeScheduling) const override
    {
        HostFacts facts;
        facts.platform = PlatformKind::macOsHost;
        facts.platformName = platformKindName(facts.platform);
        facts.logicalCoreCount = readSysctlValue<std::uint32_t>("hw.logicalcpu");
        facts.physicalMemoryBytes = readSysctlValue<std::uint64_t>("hw.memsize");
        facts.architecture = readSysctlString("hw.machine");
        facts.buildFingerprint = readSysctlString("kern.osversion");
        facts.sourceRevision = revision;
        facts.realtimeScheduling = realtimeScheduling;
        return facts;
    }
};

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return std::make_unique<MacOsHostFactsSource>();
}

} // namespace gigante
