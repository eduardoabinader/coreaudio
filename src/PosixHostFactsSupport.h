#pragma once

#include "gigante/HostFacts.h"

namespace gigante::platformDetail
{

[[nodiscard]] HostFacts readPosixHostFacts(
    const char* platformName,
    const RealtimeSchedulingOutcome& realtimeScheduling);

} // namespace gigante::platformDetail
