#include "AppleHostFactsSupport.h"
#include "PlatformHostFactsSupport.h"

namespace gigante
{
namespace
{

HostFacts readMacOsHostFacts(
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    return platformDetail::readAppleHostFacts(
        "macOS", realtimeScheduling);
}

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return platformDetail::createFunctionHostFactsSource(readMacOsHostFacts);
}

} // namespace gigante
