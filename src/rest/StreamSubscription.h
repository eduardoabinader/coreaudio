#pragma once

#include "CppHttplibClientTransport.h"

#include "gigante/RestTypes.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace gigante {

// Incremental text/event-stream parser. Comments are reported as activity only.
class SseParser final {
public:
    struct Event {
        std::string event;
        std::string data;
        std::string id;
    };

    // Returns false when a line grows past the limit.
    bool feed(const char* data, std::size_t size, const std::function<bool(const Event&)>& dispatch);

    const std::string& lastEventId() const;
    int retryMilliseconds() const;

private:
    bool line(const std::string& text, const std::function<bool(const Event&)>& dispatch);

    std::string pending;
    std::string eventName;
    std::string dataBuffer;
    bool hasData = false;
    std::string lastId;
    int retry = 0;
    bool stopped = false;
};

// One subscription on its own thread. Reconnects within the limits in its options,
// waiting at least the minimum interval whatever retry: the server sends.
class StreamSubscription final {
public:
    using EventHandler = std::function<void(const RestStreamEvent&)>;
    using CloseHandler = std::function<void(RestSubscriptionEnd, const std::string& detail)>;

    StreamSubscription(RestClientConfig config, std::string path, EventHandler onEvent, CloseHandler onClosed, RestSubscriptionOptions options);
    ~StreamSubscription();

    StreamSubscription(const StreamSubscription&) = delete;
    StreamSubscription& operator=(const StreamSubscription&) = delete;

    void start();
    // Ends at once, without connecting, for a call the client refused.
    void startClosed(RestSubscriptionEnd reason, std::string detail);
    void cancel();
    bool isActive() const;

private:
    void run();
    void finish(RestSubscriptionEnd reason, const std::string& detail);
    void deliver(const RestStreamEvent& event);
    bool isCanceled() const;
    void log(RestLogLevel level, const std::string& event, const std::string& detail) const;

    RestClientConfig config;
    std::string path;
    EventHandler onEvent;
    CloseHandler onClosed;
    RestSubscriptionOptions options;
    mutable std::mutex mutex;
    std::condition_variable wake;
    HttpClientTransport* current = nullptr;
    bool canceled = false;
    std::atomic<bool> active{true};
    bool closedReported = false;
    std::thread worker;
};

} // namespace gigante
