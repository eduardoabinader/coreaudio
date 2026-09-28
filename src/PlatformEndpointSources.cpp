#include "gigante/PlatformEndpointSources.h"

namespace gigante
{

const char* platformEndpointSourceStatusName(
    const PlatformEndpointSourceStatus status) noexcept
{
    switch (status)
    {
        case PlatformEndpointSourceStatus::available:
            return "available";
        case PlatformEndpointSourceStatus::backendUnavailable:
            return "backendUnavailable";
        case PlatformEndpointSourceStatus::platformNotImplemented:
            return "platformNotImplemented";
        case PlatformEndpointSourceStatus::initializationFailed:
            return "initializationFailed";
    }
    return "unknown";
}

} // namespace gigante
