#pragma once

#include "gigante/HostFacts.h"

namespace gigante::platformDetail
{

[[nodiscard]] HostFacts readPosixHostFacts(
    PlatformKind platform,
    const RealtimeSchedulingOutcome& realtimeScheduling);

} // namespace gigante::platformDetail
