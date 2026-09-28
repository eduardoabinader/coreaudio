#pragma once

#include "gigante/EndpointMonitor.h"
#include "gigante/EndpointPolicy.h"

#include <memory>
#include <string>

namespace gigante
{

enum class PlatformEndpointSourceStatus
{
    available,
    backendUnavailable,
    platformNotImplemented,
    initializationFailed
};

struct PlatformEndpointSources
{
    PlatformEndpointSourceStatus status{
        PlatformEndpointSourceStatus::initializationFailed};
    std::string context;
    std::unique_ptr<EndpointFactSource> factSource;
    std::unique_ptr<EndpointChangeSource> changeSource;

    [[nodiscard]] bool available() const noexcept
    {
        return status == PlatformEndpointSourceStatus::available
            && factSource != nullptr
            && changeSource != nullptr;
    }
};

// Creates sources for the selected target platform. The returned sources are
// explicitly owned and all discovery/change work is non-realtime. Builds that
// do not receive the consumer's pinned JUCE target return a specific
// backendUnavailable result rather than selecting another backend.
[[nodiscard]] PlatformEndpointSources createPlatformEndpointSources();

[[nodiscard]] const char* platformEndpointSourceStatusName(
    PlatformEndpointSourceStatus status) noexcept;

} // namespace gigante
