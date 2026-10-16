#pragma once

#include "gigante/HostFacts.h"

namespace gigante::platformDetail
{

[[nodiscard]] HostFacts readAppleHostFacts(
    const char* platformName,
    const RealtimeSchedulingOutcome& realtimeScheduling);

// Detected, not assumed. iOS always reports both (a handheld device with a
// built-in screen and speaker). macOS probes the actual console session:
// a VM or CI runner with no logged-in GUI session correctly reports no
// video capability even though "macOS" in general supports it.
[[nodiscard]] std::vector<RenderCapability> detectAppleRenderCapabilities();

} // namespace gigante::platformDetail
