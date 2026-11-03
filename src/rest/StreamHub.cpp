#include "StreamHub.h"

#include "Router.h"

#include "gigante/RestStream.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <utility>

namespace gigante {

namespace {

std::string sseEvent(std::uint64_t id, const std::string& event, const std::string& data) {
    std::string frame = "id: " + std::to_string(id) + "\n";
    if (!event.empty()) {
        frame += "event: " + event + "\n";
    }
    std::size_t start = 0;
    for (;;) {
        const std::size_t end = data.find('\n', start);
        frame += "data: " + data.substr(start, end == std::string::npos ? std::string::npos : end - start) + "\n";
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return frame + "\n";
}

} // namespace

StreamHub::StreamHub(RestMetrics& restMetrics) : metrics(restMetrics) {}

bool StreamHub::isValidEventName(const std::string& event) {
    if (event.size() > 64 || event == "gap" || event == "end") {
        return false;
    }
    for (const char current : event) {
        if (std::isalnum(static_cast<unsigned char>(current)) == 0 && current != '-' && current != '_') {
            return false;
        }
    }
    return true;
}

bool StreamHub::declare(const RestStreamDescriptor& descriptor) {
    if (!Router::isValidPathParameter(descriptor.id) || descriptor.maxQueuedBytesPerSubscriber == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex);
    return streams.emplace(descriptor.id, StreamEntry{descriptor, {}, 1, false}).second;
}

bool StreamHub::isDeclared(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(mutex);
    return streams.count(streamId) != 0;
}

std::vector<RestStreamDescriptor> StreamHub::descriptors() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<RestStreamDescriptor> result;
    for (const auto& stream : streams) {
        result.push_back(stream.second.descriptor);
    }
    return result;
}

SubscribeResult StreamHub::check(const std::string& streamId) const {
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = streams.find(streamId);
    if (entry == streams.end()) {
        return SubscribeResult::unknownStream;
    }
    if (entry->second.closed) {
        return SubscribeResult::streamEnded;
    }
    return cancelling ? SubscribeResult::stopping : SubscribeResult::subscribed;
}

std::shared_ptr<StreamSession> StreamHub::subscribe(const std::string& streamId, CapacityLease lease, const std::string& lastEventId) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = streams.find(streamId);
    if (cancelling || entry == streams.end() || entry->second.closed) {
        return nullptr;
    }
    const RestStreamDescriptor& descriptor = entry->second.descriptor;
    auto session = std::make_shared<StreamSession>(descriptor.policy, descriptor.maxQueuedBytesPerSubscriber, std::move(lease), metrics);
    if (descriptor.lane == RestStreamLane::sse && !lastEventId.empty()) {
        const std::uint64_t latest = entry->second.nextEventId - 1;
        char* parsedEnd = nullptr;
        const unsigned long long last = std::strtoull(lastEventId.c_str(), &parsedEnd, 10);
        const bool parsed = parsedEnd != nullptr && *parsedEnd == '\0';
        if (!parsed || last > latest) {
            session->offer(std::make_shared<const std::string>("event: gap\ndata: {\"missed\":null}\n\n"));
        } else if (last < latest) {
            session->offer(std::make_shared<const std::string>("event: gap\ndata: {\"missed\":" + std::to_string(latest - last) + "}\n\n"));
        }
    }
    entry->second.subscribers.insert(session);
    return session;
}

void StreamHub::unsubscribe(const std::shared_ptr<StreamSession>& session) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto& stream : streams) {
        stream.second.subscribers.erase(session);
    }
}

PublishResult StreamHub::publish(const std::string& streamId, std::string data, const std::string& event) {
    PublishResult result;
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = streams.find(streamId);
    if (entry == streams.end()) {
        return result;
    }
    result.streamKnown = true;
    if (entry->second.closed) {
        result.closed = true;
        return result;
    }
    const bool sse = entry->second.descriptor.lane == RestStreamLane::sse;
    if (sse && !event.empty() && !isValidEventName(event)) {
        result.invalidEvent = true;
        return result;
    }
    const std::size_t frameSize = sse ? data.size() + event.size() + 64 : data.size();
    if (entry->second.descriptor.policy == RestDeliveryPolicy::reliable) {
        for (const auto& session : entry->second.subscribers) {
            if (!session->isClosed() && !session->hasRoomFor(frameSize)) {
                metrics.refusedChunks.fetch_add(1);
                result.refused = true;
                return result;
            }
        }
    }
    StreamChunk chunk;
    if (sse) {
        result.eventId = entry->second.nextEventId++;
        chunk = std::make_shared<const std::string>(sseEvent(result.eventId, event, data));
    } else {
        chunk = std::make_shared<const std::string>(std::move(data));
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

bool StreamHub::closeStream(const std::string& streamId) {
    std::lock_guard<std::mutex> lock(mutex);
    const auto entry = streams.find(streamId);
    if (entry == streams.end() || entry->second.closed) {
        return false;
    }
    entry->second.closed = true;
    for (const auto& session : entry->second.subscribers) {
        session->endAfterDrain();
    }
    return true;
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

void StreamHub::shutdown() {
    std::lock_guard<std::mutex> lock(mutex);
    cancelling = true;
    for (auto& stream : streams) {
        stream.second.closed = true;
        for (const auto& session : stream.second.subscribers) {
            session->cancel();
        }
    }
}

void StreamHub::reopen() {
    std::lock_guard<std::mutex> lock(mutex);
    cancelling = false;
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
    RestStreamDescriptor descriptor;
    {
        std::lock_guard<std::mutex> lock(mutex);
        descriptor = streams.at(streamId).descriptor;
    }
    const bool sse = descriptor.lane == RestStreamLane::sse;
    if (sse) {
        response.contentType = "text/event-stream";
        response.headers.emplace("Cache-Control", "no-cache");
        response.headers.emplace("X-Accel-Buffering", "no");
    } else {
        response.contentType = descriptor.contentType.empty() ? "application/octet-stream" : descriptor.contentType;
    }
    if (!descriptor.format.empty()) {
        response.headers.emplace("X-Stream-Format", descriptor.format);
    }
    if (!descriptor.formatVersion.empty()) {
        response.headers.emplace("X-Stream-Format-Version", descriptor.formatVersion);
    }
    const int heartbeat = sse ? descriptor.heartbeatMilliseconds : 0;
    auto lastWrite = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());
    response.streamProducer = [session, sse, heartbeat, lastWrite](TransportWriter& writer) {
        const StreamChunk chunk = session->next(100);
        if (chunk) {
            *lastWrite = std::chrono::steady_clock::now();
            return writer.isWritable() && writer.write(chunk->data(), chunk->size());
        }
        if (session->isClosed()) {
            return false;
        }
        if (session->isEnding()) {
            if (sse) {
                const std::string end = "event: end\ndata: {}\n\n";
                writer.write(end.data(), end.size());
            }
            writer.finish();
            return true;
        }
        if (heartbeat > 0 && std::chrono::steady_clock::now() - *lastWrite >= std::chrono::milliseconds(heartbeat)) {
            *lastWrite = std::chrono::steady_clock::now();
            const std::string comment = ": heartbeat\n\n";
            return writer.isWritable() && writer.write(comment.data(), comment.size());
        }
        return true;
    };
    response.release = [this, session](bool) {
        session->releaseLease();
        unsubscribe(session);
    };
    return response;
}

RestStream::RestStream(std::shared_ptr<StreamHub> streamHub, std::string id) : hub(std::move(streamHub)), streamId(std::move(id)) {}

RestPublishResult RestStream::tryPublish(const std::string& data, const std::string& event) {
    const PublishResult published = hub->publish(streamId, data, event);
    RestPublishResult result;
    result.subscribers = published.delivered;
    result.dropped = published.dropped;
    result.evicted = published.evicted;
    result.eventId = published.eventId;
    if (!published.streamKnown || published.closed) {
        result.status = RestPublishStatus::closed;
    } else if (published.invalidEvent) {
        result.status = RestPublishStatus::invalidEvent;
    } else if (published.refused) {
        result.status = RestPublishStatus::refused;
    }
    return result;
}

void RestStream::close() {
    hub->closeStream(streamId);
}

const std::string& RestStream::id() const {
    return streamId;
}

} // namespace gigante
