#pragma once

#include "CapacityGate.h"
#include "RestMetrics.h"

#include "gigante/RestTypes.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace gigante {

using StreamChunk = std::shared_ptr<const std::string>;

enum class OfferOutcome { queued, droppedOldest, refused, evicted, closed };

// One subscriber. The producer side never blocks; only the server thread waits.
class StreamSession {
public:
    StreamSession(RestDeliveryPolicy policy, std::size_t maxQueuedBytes, CapacityLease lease, RestMetrics& metrics);

    OfferOutcome offer(const StreamChunk& chunk);
    bool hasRoomFor(std::size_t size) const;

    // Returns null when nothing arrived in time, or when the session is closed.
    StreamChunk next(int waitMilliseconds);

    // Stops at once and drops what is queued.
    void cancel();
    // Ends after what is already queued has been sent.
    void endAfterDrain();

    bool isClosed() const;
    // True once a graceful end was requested and the queue is empty.
    bool isEnding() const;
    bool wasEvicted() const;

    // Called once the server has finished with this session.
    void releaseLease();

private:
    RestDeliveryPolicy policy;
    std::size_t maxQueuedBytes;
    RestMetrics& metrics;
    mutable std::mutex mutex;
    std::condition_variable available;
    std::deque<StreamChunk> queue;
    std::size_t queuedBytes = 0;
    bool closed = false;
    bool ending = false;
    bool evicted = false;
    CapacityLease lease;
};

} // namespace gigante
