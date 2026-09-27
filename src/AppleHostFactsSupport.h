#pragma once

#include "gigante/HostFacts.h"

namespace gigante::platformDetail
{

[[nodiscard]] HostFacts readAppleHostFacts(
    PlatformKind platform,
    const RealtimeSchedulingOutcome& realtimeScheduling);

} // namespace gigante::platformDetail
