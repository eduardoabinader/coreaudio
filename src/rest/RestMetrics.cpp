#include "RestMetrics.h"

namespace gigante {

RestMetricsSnapshot RestMetrics::snapshot() const {
    RestMetricsSnapshot result;
    result.requests = requests.load();
    result.rejectedStreams = rejectedStreams.load();
    result.rejectedTransfers = rejectedTransfers.load();
    result.resourceExhaustion = resourceExhaustion.load();
    result.transportErrors = transportErrors.load();
    result.droppedChunks = droppedChunks.load();
    result.refusedChunks = refusedChunks.load();
    result.evictedSubscribers = evictedSubscribers.load();
    result.handlerExceptions = handlerExceptions.load();
    result.authenticationFailures = authenticationFailures.load();
    result.dispatcherFull = dispatcherFull.load();
    result.dispatcherDeadlines = dispatcherDeadlines.load();
    return result;
}

} // namespace gigante
