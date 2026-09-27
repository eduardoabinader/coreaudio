#include "PlatformHostFactsSupport.h"
#include "PosixHostFactsSupport.h"

namespace gigante
{
namespace
{

HostFacts readLinuxHostFacts(
    const RealtimeSchedulingOutcome& realtimeScheduling)
{
    return platformDetail::readPosixHostFacts(
        PlatformKind::linuxHost, realtimeScheduling);
}

} // namespace

std::unique_ptr<HostFactsSource> createHostFactsSource()
{
    return platformDetail::createFunctionHostFactsSource(readLinuxHostFacts);
}

} // namespace gigante
