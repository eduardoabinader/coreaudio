#pragma once

#include "CapacityGate.h"
#include "DeliveryPolicy.h"
#include "HttpTransport.h"
#include "RestMetrics.h"
#include "StreamSession.h"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace gigante {

struct StreamDescriptor {
    std::string id;
    std::string contentType = "text/event-stream";
    DeliveryPolicy policy = DeliveryPolicy::realtime;
    std::size_t maxQueuedBytesPerSubscriber = 256 * 1024;
};

struct PublishResult {
    bool streamKnown = false;
    bool refused = false;
    std::size_t delivered = 0;
    std::size_t dropped = 0;
    std::size_t evicted = 0;
};

class StreamHub {
public:
    explicit StreamHub(RestMetrics& metrics);

    bool declare(const StreamDescriptor& descriptor);
    std::shared_ptr<StreamSession> subscribe(const std::string& streamId, CapacityLease lease);
    void unsubscribe(const std::shared_ptr<StreamSession>& session);

    // Never blocks on a subscriber. A reliable stream delivers to all or refuses.
    PublishResult publish(const std::string& streamId, std::string data);
    void cancelAll();
    std::size_t subscriberCount() const;

    TransportResponse makeResponse(const std::string& streamId, const std::shared_ptr<StreamSession>& session);

private:
    struct StreamEntry {
        StreamDescriptor descriptor;
        std::set<std::shared_ptr<StreamSession>> subscribers;
    };

    RestMetrics& metrics;
    mutable std::mutex mutex;
    std::map<std::string, StreamEntry> streams;
    bool cancelling = false;
};

} // namespace gigante
