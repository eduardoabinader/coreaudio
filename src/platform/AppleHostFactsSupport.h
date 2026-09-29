#pragma once

#include "gigante/HostFacts.h"

namespace gigante::platformDetail
{

[[nodiscard]] HostFacts readAppleHostFacts(
    const char* platformName,
    const RealtimeSchedulingOutcome& realtimeScheduling);

} // namespace gigante::platformDetail
