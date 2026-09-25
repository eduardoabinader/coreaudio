#include "gigante/AudioBufferView.h"
#include "gigante/EffectsChain.h"
#include "gigante/EffectsStageObserver.h"
#include "gigante/RealtimeMetrics.h"
#include "gigante/Revision.h"

namespace
{

static_assert(gigante::revision != nullptr, "revision identity must be available");
static_assert(sizeof(gigante::AudioBufferView) > 0, "public buffer view must be complete");
static_assert(sizeof(gigante::EffectsChain) > 0, "public processor must be complete");
static_assert(sizeof(gigante::RealtimeMetricsSnapshot) > 0, "metrics must be complete");

} // namespace
