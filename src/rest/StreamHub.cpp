#include "StreamHub.h"

#include <utility>

namespace gigante {

StreamHub::StreamHub(RestMetrics& restMetrics) : metrics(restMetrics) {}

bool StreamHub::declare(const StreamDescriptor& descriptor) {
    std::lock_guard<std::mutex> lock(mutex);
    return streams.emplace(descriptor.id, StreamEntry{descriptor, {}}).second;
}

std::shared_ptr<StreamSession> StreamHub::subscribe(const std::string& streamId, CapacityLease lease) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = streams.find(streamId);
    if (cancelling || entry == streams.end()) {
        return nullptr;
    }
    auto session = std::make_shared<StreamSession>(
        entry->second.descriptor.policy,
        entry->second.descriptor.maxQueuedBytesPerSubscriber,
        std::move(lease),
        metrics);
    entry->second.subscribers.insert(session);
    return session;
}

void StreamHub::unsubscribe(const std::shared_ptr<StreamSession>& session) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& stream : streams) {
        stream.second.subscribers.erase(session);
    }
}

PublishResult StreamHub::publish(const std::string& streamId, std::string data) {
    PublishResult result;
    const auto chunk = std::make_shared<const std::string>(std::move(data));
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = streams.find(streamId);
    if (entry == streams.end()) {
        return result;
    }
    result.streamKnown = true;
    if (entry->second.descriptor.policy == DeliveryPolicy::reliable) {
        for (const auto& session : entry->second.subscribers) {
            if (!session->isClosed() && !session->hasRoomFor(chunk->size())) {
                metrics.refusedChunks.fetch_add(1);
                result.refused = true;
                return result;
            }
        }
    }
    for (const auto& session : entry->second.subscribers) {
        switch (session->offer(chunk)) {
            case OfferOutcome::queued:
                ++result.delivered;
                break;
            case OfferOutcome::droppedOldest:
                ++result.delivered;
                ++result.dropped;
                break;
            case OfferOutcome::evicted:
                ++result.evicted;
                break;
            case OfferOutcome::refused:
                result.refused = true;
                break;
            case OfferOutcome::closed:
                break;
        }
    }
    return result;
}

void StreamHub::cancelAll() {
    std::lock_guard<std::mutex> lock(mutex);
    cancelling = true;
    for (auto& stream : streams) {
        for (const auto& session : stream.second.subscribers) {
            session->cancel();
        }
    }
}

std::size_t StreamHub::subscriberCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::size_t count = 0;
    for (const auto& stream : streams) {
        count += stream.second.subscribers.size();
    }
    return count;
}

TransportResponse StreamHub::makeResponse(const std::string& streamId, const std::shared_ptr<StreamSession>& session) {
    TransportResponse response;
    {
        std::lock_guard<std::mutex> lock(mutex);
        response.contentType = streams.at(streamId).descriptor.contentType;
    }
    response.streamProducer = [session](TransportWriter& writer) {
        const StreamChunk chunk = session->next(100);
        if (!chunk) {
            return !session->isClosed();
        }
        return writer.isWritable() && writer.write(chunk->data(), chunk->size());
    };
    response.release = [this, session](bool) {
        session->releaseLease();
        unsubscribe(session);
    };
    return response;
}

} // namespace gigante
