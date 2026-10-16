#pragma once

#include "gigante/HostFacts.h"

namespace gigante::platformDetail
{

[[nodiscard]] HostFacts readPosixHostFacts(
    const char* platformName,
    const RealtimeSchedulingOutcome& realtimeScheduling);

// Detected, not assumed: audio is always reported (a POSIX host can always
// render to a null/dummy audio sink even without real hardware). Video is
// reported only when an actual display-server socket (X11 or Wayland) is
// present, so a headless VM/container correctly reports no video capability
// even though "Linux" in general supports it.
[[nodiscard]] std::vector<RenderCapability> detectPosixRenderCapabilities();

} // namespace gigante::platformDetail
