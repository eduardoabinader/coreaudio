#include "AppleHostFactsSupport.h"
#include "PlatformHostFactsSupport.h"

namespace gigante
{
namespace
{

HostFacts readIosHostFacts(
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    return platformDetail::readAppleHostFacts(
        "iOS", realtimeScheduling);
}

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return platformDetail::createFunctionHostFactsSource(readIosHostFacts);
}

} // namespace gigante
