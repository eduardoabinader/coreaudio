#include "AppleHostFactsSupport.h"

#include "gigante/Revision.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <sys/sysctl.h>

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gigante::platformDetail
{
namespace
{

#if TARGET_OS_OSX
// kCGSessionOnConsoleKey is true only for a real, logged-in console/GUI
// session; a headless macOS VM, CI runner, or a user switched out via Fast
// User Switching reports false here, which is the actual distinction this
// capability exists to make (not "is this macOS").
bool hasConsoleSession()
{
    CFDictionaryRef sessionInfo = CGSessionCopyCurrentDictionary();
    if (sessionInfo == nullptr)
        return false;

    const void* onConsoleValue =
        CFDictionaryGetValue(sessionInfo, kCGSessionOnConsoleKey);
    const bool onConsole = onConsoleValue != nullptr
        && CFBooleanGetValue(static_cast<CFBooleanRef>(onConsoleValue));
    CFRelease(sessionInfo);
    return onConsole;
}
#endif

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

std::vector<RenderCapability> detectAppleRenderCapabilities()
{
#if TARGET_OS_OSX
    std::vector<RenderCapability> capabilities{RenderCapability::audio};
    if (hasConsoleSession())
        capabilities.push_back(RenderCapability::video);
    return capabilities;
#else
    return {RenderCapability::audio, RenderCapability::video};
#endif
}

HostFacts readAppleHostFacts(
    const char* const platformName,
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    HostFacts facts;
    facts.platformName = platformName;
    facts.logicalCoreCount = readSysctlValue<std::uint32_t>("hw.logicalcpu");
    facts.physicalMemoryBytes = readSysctlValue<std::uint64_t>("hw.memsize");
    facts.architecture = readSysctlString("hw.machine");
    facts.buildFingerprint = readSysctlString("kern.osversion");
    facts.sourceRevision = revision;
    facts.realtimeScheduling = realtimeScheduling;
    facts.renderCapabilities = detectAppleRenderCapabilities();
    return facts;
}

} // namespace gigante::platformDetail
