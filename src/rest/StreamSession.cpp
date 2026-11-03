#include "StreamSession.h"

#include <chrono>
#include <utility>

namespace gigante {

StreamSession::StreamSession(
    RestDeliveryPolicy deliveryPolicy, std::size_t queueLimit, CapacityLease capacityLease, RestMetrics& restMetrics)
    : policy(deliveryPolicy), maxQueuedBytes(queueLimit), metrics(restMetrics), lease(std::move(capacityLease)) {}

OfferOutcome StreamSession::offer(const StreamChunk& chunk) {
    OfferOutcome outcome = OfferOutcome::queued;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (closed || ending) {
            return OfferOutcome::closed;
        }
        if (queuedBytes + chunk->size() > maxQueuedBytes) {
            if (policy == RestDeliveryPolicy::reliable) {
                metrics.refusedChunks.fetch_add(1);
                return OfferOutcome::refused;
            }
            if (policy == RestDeliveryPolicy::broadcast) {
                closed = true;
                evicted = true;
                queue.clear();
                queuedBytes = 0;
                metrics.evictedSubscribers.fetch_add(1);
                available.notify_all();
                return OfferOutcome::evicted;
            }
            while (!queue.empty() && queuedBytes + chunk->size() > maxQueuedBytes) {
                queuedBytes -= queue.front()->size();
                queue.pop_front();
                metrics.droppedChunks.fetch_add(1);
            }
            outcome = OfferOutcome::droppedOldest;
            if (chunk->size() > maxQueuedBytes) {
                metrics.droppedChunks.fetch_add(1);
                return outcome;
            }
        }
        queue.push_back(chunk);
        queuedBytes += chunk->size();
    }
    available.notify_one();
    return outcome;
}

bool StreamSession::hasRoomFor(std::size_t size) const {
    std::lock_guard<std::mutex> lock(mutex);
    return !closed && !ending && queuedBytes + size <= maxQueuedBytes;
}

StreamChunk StreamSession::next(int waitMilliseconds) {
    std::unique_lock<std::mutex> lock(mutex);
    available.wait_for(lock, std::chrono::milliseconds(waitMilliseconds), [this] {
        return closed || ending || !queue.empty();
    });
    if (closed || queue.empty()) {
        return nullptr;
    }
    StreamChunk chunk = std::move(queue.front());
    queue.pop_front();
    queuedBytes -= chunk->size();
    return chunk;
}

void StreamSession::cancel() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        closed = true;
        queue.clear();
        queuedBytes = 0;
    }
    available.notify_all();
}

void StreamSession::endAfterDrain() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        ending = true;
    }
    available.notify_all();
}

bool StreamSession::isClosed() const {
    std::lock_guard<std::mutex> lock(mutex);
    return closed;
}

bool StreamSession::isEnding() const {
    std::lock_guard<std::mutex> lock(mutex);
    return ending && queue.empty();
}

bool StreamSession::wasEvicted() const {
    std::lock_guard<std::mutex> lock(mutex);
    return evicted;
}

void StreamSession::releaseLease() {
    std::lock_guard<std::mutex> lock(mutex);
    closed = true;
    lease.release();
}

} // namespace gigante
