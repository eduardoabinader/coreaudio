#include "gigante/PlatformEndpointSources.h"

namespace gigante
{

PlatformEndpointSources createPlatformEndpointSources()
{
    PlatformEndpointSources result;
#if defined(GIGANTE_PLATFORM_ENDPOINT_BACKEND_NOT_IMPLEMENTED)
    result.status = PlatformEndpointSourceStatus::platformNotImplemented;
    result.context =
        "endpoint sources are not implemented for the selected target platform";
#else
    result.status = PlatformEndpointSourceStatus::backendUnavailable;
    result.context =
        "endpoint sources require an explicit consumer-provided JUCE target";
#endif
    return result;
}

} // namespace gigante
