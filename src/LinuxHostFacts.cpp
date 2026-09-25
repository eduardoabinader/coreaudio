#include "gigante/HostFacts.h"
#include "gigante/Revision.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <sys/utsname.h>
#include <unistd.h>

namespace gigante
{
namespace
{

std::optional<std::uint32_t> readLogicalCoreCount()
{
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count <= 0
        || static_cast<unsigned long>(count)
            > std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;

    return static_cast<std::uint32_t>(count);
}

std::optional<std::uint64_t> readPhysicalMemoryBytes()
{
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pages <= 0 || pageSize <= 0)
        return std::nullopt;

    const auto unsignedPages = static_cast<std::uint64_t>(pages);
    const auto unsignedPageSize = static_cast<std::uint64_t>(pageSize);
    if (unsignedPages
        > std::numeric_limits<std::uint64_t>::max() / unsignedPageSize)
        return std::nullopt;

    return unsignedPages * unsignedPageSize;
}

std::optional<struct utsname> readSystemName()
{
    struct utsname systemName{};
    if (uname(&systemName) != 0)
        return std::nullopt;

    return systemName;
}

class LinuxHostFactsSource final : public HostFactsSource
{
public:
    [[nodiscard]] HostFacts read(
        const RealtimeSchedulingOutcome& realtimeScheduling) const override
    {
        HostFacts facts;
        facts.platform = PlatformKind::linuxHost;
        facts.platformName = platformKindName(facts.platform);
        facts.logicalCoreCount = readLogicalCoreCount();
        facts.physicalMemoryBytes = readPhysicalMemoryBytes();
        facts.sourceRevision = revision;
        facts.realtimeScheduling = realtimeScheduling;

        const auto systemName = readSystemName();
        if (systemName.has_value())
        {
            facts.architecture = systemName->machine;
            facts.buildFingerprint = std::string{systemName->release}
                + " " + systemName->version;
        }

        return facts;
    }
};

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return std::make_unique<LinuxHostFactsSource>();
}

} // namespace gigante
