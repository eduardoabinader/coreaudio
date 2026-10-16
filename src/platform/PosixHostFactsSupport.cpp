#include "PosixHostFactsSupport.h"

#include "gigante/Revision.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <sys/utsname.h>
#include <unistd.h>

namespace gigante::platformDetail
{
namespace
{

bool pathExists(const std::string& path)
{
    return access(path.c_str(), F_OK) == 0;
}

// A reachable Wayland compositor socket at $XDG_RUNTIME_DIR/$WAYLAND_DISPLAY
// (default name "wayland-0" when WAYLAND_DISPLAY is unset but a runtime
// directory is). This checks the socket file exists, not a full protocol
// handshake, which is enough to distinguish "no compositor at all" from
// "one is running" without linking a Wayland client library.
bool hasWaylandDisplay()
{
    const char* const runtimeDir = std::getenv("XDG_RUNTIME_DIR");
    if (runtimeDir == nullptr || runtimeDir[0] == '\0')
        return false;

    const char* const waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const std::string socketName =
        (waylandDisplay != nullptr && waylandDisplay[0] != '\0')
            ? waylandDisplay
            : "wayland-0";

    return pathExists(std::string{runtimeDir} + "/" + socketName);
}

// A reachable X11 server socket at /tmp/.X11-unix/X<N>, parsed from
// $DISPLAY (":0", ":0.0", or "hostname:0"). Same rationale as Wayland:
// socket presence, not a full X11 handshake, with no libX11 dependency.
bool hasX11Display()
{
    const char* const display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0')
        return false;

    const std::string value{display};
    const auto colon = value.find(':');
    if (colon == std::string::npos || colon + 1U >= value.size())
        return false;

    std::string displayNumber = value.substr(colon + 1U);
    const auto dot = displayNumber.find('.');
    if (dot != std::string::npos)
        displayNumber.resize(dot);
    if (displayNumber.empty())
        return false;

    return pathExists("/tmp/.X11-unix/X" + displayNumber);
}

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

} // namespace

std::vector<RenderCapability> detectPosixRenderCapabilities()
{
    std::vector<RenderCapability> capabilities{RenderCapability::audio};
    if (hasWaylandDisplay() || hasX11Display())
        capabilities.push_back(RenderCapability::video);
    return capabilities;
}

HostFacts readPosixHostFacts(
    const char* const platformName,
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    HostFacts facts;
    facts.platformName = platformName;
    facts.logicalCoreCount = readLogicalCoreCount();
    facts.physicalMemoryBytes = readPhysicalMemoryBytes();
    facts.sourceRevision = revision;
    facts.realtimeScheduling = realtimeScheduling;
    facts.renderCapabilities = detectPosixRenderCapabilities();

    const auto systemName = readSystemName();
    if (systemName.has_value())
    {
        facts.architecture = systemName->machine;
        facts.buildFingerprint = std::string{systemName->release}
            + " " + systemName->version;
    }

    return facts;
}

} // namespace gigante::platformDetail
