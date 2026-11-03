#pragma once

#include "CapacityGate.h"
#include "HttpTransport.h"
#include "RestMetrics.h"
#include "StreamSession.h"

#include "gigante/RestTypes.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace gigante {

struct PublishResult {
    bool streamKnown = false;
    bool refused = false;
    bool closed = false;
    bool invalidEvent = false;
    std::size_t delivered = 0;
    std::size_t dropped = 0;
    std::size_t evicted = 0;
    std::uint64_t eventId = 0;
};

enum class SubscribeResult { subscribed, unknownStream, streamEnded, stopping };

class StreamHub {
public:
    explicit StreamHub(RestMetrics& metrics);

    bool declare(const RestStreamDescriptor& descriptor);
    bool isDeclared(const std::string& streamId) const;
    std::vector<RestStreamDescriptor> descriptors() const;
    SubscribeResult check(const std::string& streamId) const;

    // A non-empty lastEventId on the SSE lane makes the first event a gap event.
    std::shared_ptr<StreamSession> subscribe(const std::string& streamId, CapacityLease lease, const std::string& lastEventId = "");
    void unsubscribe(const std::shared_ptr<StreamSession>& session);

    // Never blocks on a subscriber. A reliable stream delivers to all or refuses.
    PublishResult publish(const std::string& streamId, std::string data, const std::string& event = "");

    // Ends every subscriber after its queue drains; later publishing is refused.
    bool closeStream(const std::string& streamId);

    void cancelAll();
    void reopen();
    // Ends every stream for good. A RestStream that outlives the server then only sees closed.
    void shutdown();
    std::size_t subscriberCount() const;

    TransportResponse makeResponse(const std::string& streamId, const std::shared_ptr<StreamSession>& session);

    static bool isValidEventName(const std::string& event);

private:
    struct StreamEntry {
        RestStreamDescriptor descriptor;
        std::set<std::shared_ptr<StreamSession>> subscribers;
        std::uint64_t nextEventId = 1;
        bool closed = false;
    };

    RestMetrics& metrics;
    mutable std::mutex mutex;
    std::map<std::string, StreamEntry> streams;
    bool cancelling = false;
};

} // namespace gigante
