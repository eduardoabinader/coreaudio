#include "gigante/EndpointPolicy.h"
#include "gigante/HostFacts.h"

namespace
{

static_assert(sizeof(gigante::HostFacts) > 0,
              "public host facts must be complete");
static_assert(sizeof(gigante::RealtimeSchedulingOutcome) > 0,
              "public scheduling outcome must be complete");
static_assert(sizeof(gigante::EndpointSelectionResult) > 0,
              "public endpoint selection result must be complete");

} // namespace
