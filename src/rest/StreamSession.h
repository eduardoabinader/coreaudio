#pragma once

#include "CapacityGate.h"
#include "DeliveryPolicy.h"
#include "RestMetrics.h"

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
    StreamSession(DeliveryPolicy policy, std::size_t maxQueuedBytes, CapacityLease lease, RestMetrics& metrics);

    OfferOutcome offer(const StreamChunk& chunk);
    bool hasRoomFor(std::size_t size) const;

    // Returns null when nothing arrived in time, or when the session is closed.
    StreamChunk next(int waitMilliseconds);
    void cancel();
    bool isClosed() const;
    bool wasEvicted() const;

    // Called once the server has finished with this session.
    void releaseLease();

private:
    DeliveryPolicy policy;
    std::size_t maxQueuedBytes;
    RestMetrics& metrics;
    mutable std::mutex mutex;
    std::condition_variable available;
    std::deque<StreamChunk> queue;
    std::size_t queuedBytes = 0;
    bool closed = false;
    bool evicted = false;
    CapacityLease lease;
};

} // namespace gigante
