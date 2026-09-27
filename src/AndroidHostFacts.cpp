#include "PlatformHostFactsSupport.h"
#include "PosixHostFactsSupport.h"

#include <cstddef>
#include <optional>
#include <string>
#include <sys/system_properties.h>

namespace gigante
{
namespace
{

std::optional<std::string> readAndroidBuildFingerprint()
{
    char value[PROP_VALUE_MAX]{};
    const int length = __system_property_get("ro.build.fingerprint", value);
    if (length <= 0)
        return std::nullopt;

    return std::string{value, static_cast<std::size_t>(length)};
}

HostFacts readAndroidHostFacts(
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    auto facts = platformDetail::readPosixHostFacts(
        PlatformKind::androidHost, realtimeScheduling);
    facts.buildFingerprint = readAndroidBuildFingerprint();
    return facts;
}

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return platformDetail::createFunctionHostFactsSource(readAndroidHostFacts);
}

} // namespace gigante
