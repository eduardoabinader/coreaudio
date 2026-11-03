#include "RestTestSupport.h"

#include "CapacityGate.h"
#include "RestMetrics.h"
#include "StreamHub.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;
using namespace restTest;

namespace {

struct Received {
    std::mutex mutex;
    std::vector<RestStreamEvent> events;
    std::atomic<int> closedCount{0};
    std::atomic<int> lastEnd{-1};
    std::string closedDetail;

    void add(const RestStreamEvent& event) {
        std::lock_guard<std::mutex> lock(mutex);
        events.push_back(event);
    }
    void closed(RestSubscriptionEnd end, const std::string& detail) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closedDetail = detail;
        }
        lastEnd.store(static_cast<int>(end));
        closedCount.fetch_add(1);
    }
    std::size_t count() {
        std::lock_guard<std::mutex> lock(mutex);
        return events.size();
    }
    std::vector<RestStreamEvent> snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        return events;
    }
};

std::unique_ptr<RestSubscription> subscribeTo(RestClient& client, const std::string& id, Received& received, RestSubscriptionOptions options = {}) {
    return client.subscribe(
        id,
        [&received](const RestStreamEvent& event) { received.add(event); },
        [&received](RestSubscriptionEnd end, const std::string& detail) { received.closed(end, detail); },
        std::move(options));
}

RestSubscriptionOptions quickOptions() {
    RestSubscriptionOptions options;
    options.minimumReconnectMilliseconds = 20;
    options.maximumAttempts = 3;
    options.idleTimeoutMilliseconds = 3000;
    return options;
}

RestStreamDescriptor sseStream(const std::string& id) {
    RestStreamDescriptor descriptor;
    descriptor.id = id;
    descriptor.format = "testEvents";
    descriptor.formatVersion = "1";
    return descriptor;
}

bool waitForSubscribers(RestClient& client, int count) {
    return waitUntil([&] { return contains(client.status().body, "\"subscribers\":" + std::to_string(count)); });
}

// Policies, checked on the hub itself with a subscriber that never reads.
void deliveryPolicies() {
    RestMetrics metrics;
    CapacityGate gate(8, 8, metrics);
    StreamHub hub(metrics);
    for (const auto& entry : {std::make_pair("reliable", RestDeliveryPolicy::reliable), std::make_pair("realtime", RestDeliveryPolicy::realtime), std::make_pair("broadcast", RestDeliveryPolicy::broadcast)}) {
        RestStreamDescriptor descriptor;
        descriptor.id = entry.first;
        descriptor.lane = RestStreamLane::binary;
        descriptor.policy = entry.second;
        descriptor.maxQueuedBytesPerSubscriber = 1000;
        check(hub.declare(descriptor), "declarePolicyStream", entry.first);
    }
    const std::string chunk(400, 'x');

    auto stalled = hub.subscribe("reliable", std::move(*gate.tryAcquireStream()));
    auto reader = hub.subscribe("reliable", std::move(*gate.tryAcquireStream()));
    int delivered = 0;
    PublishResult result;
    while ((result = hub.publish("reliable", chunk)).delivered == 2) {
        ++delivered;
        reader->next(0);
    }
    check(delivered == 2 && result.refused && result.delivered == 0, "reliableRefusesWhole", std::to_string(delivered));
    check(metrics.droppedChunks.load() == 0, "reliableNeverDrops");

    auto slow = hub.subscribe("realtime", std::move(*gate.tryAcquireStream()));
    for (int index = 0; index < 5; ++index) {
        result = hub.publish("realtime", chunk + std::to_string(index));
    }
    check(result.delivered == 1 && result.dropped == 1 && metrics.droppedChunks.load() == 3, "realtimeDropsOldest", std::to_string(metrics.droppedChunks.load()));
    const StreamChunk oldest = slow->next(0);
    check(oldest && oldest->back() == '3', "realtimeKeepsNewest", oldest ? *oldest : "none");

    auto lagging = hub.subscribe("broadcast", std::move(*gate.tryAcquireStream()));
    auto keeping = hub.subscribe("broadcast", std::move(*gate.tryAcquireStream()));
    int evicted = 0;
    for (int index = 0; index < 5; ++index) {
        result = hub.publish("broadcast", chunk);
        evicted += static_cast<int>(result.evicted);
        keeping->next(0);
    }
    check(evicted == 1 && lagging->wasEvicted() && !keeping->isClosed() && metrics.evictedSubscribers.load() == 1, "broadcastEvictsSlowOnly", std::to_string(evicted));

    for (const auto& session : {stalled, reader, slow, lagging, keeping}) {
        session->releaseLease();
        hub.unsubscribe(session);
    }
    check(gate.activeStreams() == 0 && hub.subscriberCount() == 0, "leasesReleased", std::to_string(gate.activeStreams()));
}

void declaration() {
    RestServer server(developmentServer());
    check(server.declareStream(sseStream("events")) != nullptr, "declareStream");
    check(server.declareStream(sseStream("events")) == nullptr, "duplicateStreamRefused");
    check(server.declareStream(sseStream("bad/id")) == nullptr, "invalidStreamIdRefused");
    RestStreamDescriptor binary;
    binary.id = "frames";
    binary.lane = RestStreamLane::binary;
    binary.contentType = "application/x-test-frames";
    check(server.declareStream(binary) != nullptr, "declareBinaryStream");
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    check(server.declareStream(sseStream("late")) == nullptr, "declareAfterStartRefused");
    RestClient client(developmentClient(started.port));
    const RestResult capabilities = client.getCapabilities();
    check(contains(capabilities.body, "\"streamLanes\":[\"sse\",\"binary\"]") && contains(capabilities.body, "\"streams\""), "capabilitiesListLanes", capabilities.body);
    const RestResult listing = client.call(RestMethod::get, "/core/v1/streams");
    check(listing.status == 200 && contains(listing.body, "\"id\":\"events\"") && contains(listing.body, "\"lane\":\"binary\"") &&
              contains(listing.body, "application/x-test-frames") && contains(listing.body, "\"format\":\"testEvents\""),
          "streamListing", listing.body);
}

void sseDelivery() {
    RestServer server(developmentServer());
    const std::shared_ptr<RestStream> stream = server.declareStream(sseStream("events"));
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    RestClient client(developmentClient(started.port));
    Received received;
    auto subscription = subscribeTo(client, "events", received, quickOptions());
    check(waitForSubscribers(client, 1), "subscriberCounted");
    check(stream->tryPublish("first", "status").status == RestPublishStatus::delivered, "publishDelivered");
    stream->tryPublish("line one\nline two");
    const RestPublishResult third = stream->tryPublish("{\"n\":3}", "sample-update");
    check(third.eventId == 3 && third.subscribers == 1, "eventIdsIncrease", std::to_string(third.eventId));
    check(waitUntil([&] { return received.count() == 3; }), "eventsArrive", std::to_string(received.count()));
    const std::vector<RestStreamEvent> events = received.snapshot();
    if (events.size() == 3) {
        check(events[0].event == "status" && events[0].data == "first" && events[0].id == "1", "firstEvent", events[0].data);
        check(events[1].data == "line one\nline two" && events[1].id == "2", "multiLineData", events[1].data);
        check(events[2].event == "sample-update" && events[2].id == "3", "orderKept");
    }
    for (const char* reserved : {"gap", "end", "bad name", ""}) {
        if (std::string(reserved).empty()) {
            continue;
        }
        check(stream->tryPublish("x", reserved).status == RestPublishStatus::invalidEvent, "reservedEventRefused", reserved);
    }
    subscription->cancel();
    check(waitUntil([&] { return received.closedCount.load() == 1; }), "cancelClosesOnce");
    check(received.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::canceled), "cancelReason");
    subscription.reset();
    check(received.closedCount.load() == 1, "onClosedOnlyOnce");
    check(waitForSubscribers(client, 0), "leaseReleasedAfterCancel");
}

void reconnectGap() {
    RestServer server(developmentServer());
    const std::shared_ptr<RestStream> stream = server.declareStream(sseStream("events"));
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    for (int index = 0; index < 5; ++index) {
        stream->tryPublish("before " + std::to_string(index));
    }
    RestClient client(developmentClient(started.port));
    Received received;
    RestSubscriptionOptions options = quickOptions();
    options.lastEventId = "2";
    auto subscription = subscribeTo(client, "events", received, options);
    check(waitUntil([&] { return received.count() == 1; }), "gapArrives");
    const std::vector<RestStreamEvent> events = received.snapshot();
    check(!events.empty() && events[0].event == "gap" && contains(events[0].data, "\"missed\":3"), "gapCountsMissed", events.empty() ? "" : events[0].data);
    stream->tryPublish("after");
    check(waitUntil([&] { return received.count() == 2; }), "eventsFollowGap");
    const std::vector<RestStreamEvent> after = received.snapshot();
    check(after.size() == 2 && after[1].id == "6" && after[1].data == "after", "noReplay", after.size() == 2 ? after[1].id : "");
}

void producerClose() {
    RestServer server(developmentServer());
    const std::shared_ptr<RestStream> stream = server.declareStream(sseStream("events"));
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    RestClient client(developmentClient(started.port));
    Received received;
    auto subscription = subscribeTo(client, "events", received, quickOptions());
    check(waitForSubscribers(client, 1), "closeSubscriberReady");
    stream->tryPublish("last words");
    stream->close();
    check(waitUntil([&] { return received.closedCount.load() == 1; }), "endEventCloses");
    check(received.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::streamEnded) && received.count() == 1, "drainedBeforeEnd", std::to_string(received.count()));
    check(stream->tryPublish("too late").status == RestPublishStatus::closed, "publishAfterCloseRefused");
    Received late;
    auto lateSubscription = subscribeTo(client, "events", late, quickOptions());
    check(waitUntil([&] { return late.closedCount.load() == 1; }) && late.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::streamEnded), "closedStreamAnswers410");
}

void rejections() {
    RestServerConfig config = developmentServer();
    config.limits.maxStreams = 1;
    RestServer server(config);
    server.declareStream(sseStream("events"));
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    RestClient client(developmentClient(started.port));

    Received unknown;
    auto unknownSubscription = subscribeTo(client, "missing", unknown, quickOptions());
    check(waitUntil([&] { return unknown.closedCount.load() == 1; }) && unknown.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::rejected), "unknownStreamRejected");

    Received first;
    auto holder = subscribeTo(client, "events", first, quickOptions());
    check(waitForSubscribers(client, 1), "capHolderConnected");
    Received second;
    RestSubscriptionOptions options = quickOptions();
    options.maximumAttempts = 2;
    const auto before = std::chrono::steady_clock::now();
    auto refused = subscribeTo(client, "events", second, options);
    check(waitUntil([&] { return second.closedCount.load() == 1; }) && second.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::attemptsExhausted), "capRefusesWith503");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    check(elapsed >= 20, "minimumReconnectHonored", std::to_string(elapsed));
    check(server.counters().rejectedStreams == 2, "rejectedStreamsCounted", std::to_string(server.counters().rejectedStreams));
    check(client.health().status == 200 && client.status().status == 200, "healthAnswersAtCap");

    RestClientConfig production(SecurityProfile::production);
    production.baseUrl = "http://127.0.0.1:" + std::to_string(started.port);
    RestClient refusedClient(production);
    Received policy;
    auto refusedByPolicy = subscribeTo(refusedClient, "events", policy);
    check(waitUntil([&] { return policy.closedCount.load() == 1; }) && policy.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::refusedByPolicy), "policyRefusalReported");
}

void heartbeats() {
    RestServer server(developmentServer());
    RestStreamDescriptor descriptor = sseStream("quiet");
    descriptor.heartbeatMilliseconds = 100;
    const std::shared_ptr<RestStream> stream = server.declareStream(descriptor);
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    RestClient client(developmentClient(started.port));
    Received received;
    RestSubscriptionOptions options = quickOptions();
    options.idleTimeoutMilliseconds = 400;
    options.maximumAttempts = 1;
    auto subscription = subscribeTo(client, "quiet", received, options);
    check(waitForSubscribers(client, 1), "quietSubscriberReady");
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    check(received.closedCount.load() == 0 && subscription->isActive(), "heartbeatKeepsIdleStreamOpen");
    stream->tryPublish("after silence");
    check(waitUntil([&] { return received.count() == 1; }), "eventAfterSilence");
}

void binarySlowSubscriber() {
    RestServer server(developmentServer());
    RestStreamDescriptor descriptor;
    descriptor.id = "frames";
    descriptor.lane = RestStreamLane::binary;
    descriptor.policy = RestDeliveryPolicy::realtime;
    descriptor.maxQueuedBytesPerSubscriber = 64 * 1024;
    const std::shared_ptr<RestStream> stream = server.declareStream(descriptor);
    const RestStartResult started = server.start();
    check(started.started, "serverStarts", started.cause);
    RestClient client(developmentClient(started.port));
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<int> closed{0};
    RestSubscriptionOptions options = quickOptions();
    options.binary = true;
    auto subscription = client.subscribe(
        "frames",
        [&](const RestStreamEvent& event) {
            bytes.fetch_add(event.data.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        },
        [&](RestSubscriptionEnd, const std::string&) { closed.fetch_add(1); },
        options);
    check(waitForSubscribers(client, 1), "binarySubscriberReady");
    const std::string frame(16 * 1024, 'f');
    long long slowest = 0;
    for (int index = 0; index < 2000; ++index) {
        const auto before = std::chrono::steady_clock::now();
        stream->tryPublish(frame);
        slowest = std::max<long long>(slowest, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count());
    }
    check(slowest < 100, "publishNeverBlocks", std::to_string(slowest));
    check(server.counters().droppedChunks > 0, "slowSubscriberDropsCounted", std::to_string(server.counters().droppedChunks));
    check(waitUntil([&] { return bytes.load() > 0; }), "binaryBytesArrive");
    subscription.reset();
    check(closed.load() == 1, "binaryClosedOnce");
}

void stopWithSubscribers() {
    auto server = std::make_unique<RestServer>(developmentServer());
    const std::shared_ptr<RestStream> stream = server->declareStream(sseStream("events"));
    const RestStartResult started = server->start();
    check(started.started, "serverStarts", started.cause);
    RestClient client(developmentClient(started.port));
    Received received;
    RestSubscriptionOptions options = quickOptions();
    options.maximumAttempts = 2;
    auto subscription = subscribeTo(client, "events", received, options);
    check(waitForSubscribers(client, 1), "stopSubscriberReady");
    const auto before = std::chrono::steady_clock::now();
    check(server->stop(5000), "stopWithOpenStream", server->lastStopCause());
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    check(elapsed < 2000, "stopIsPrompt", std::to_string(elapsed));
    check(waitUntil([&] { return received.closedCount.load() == 1; }) && received.lastEnd.load() == static_cast<int>(RestSubscriptionEnd::attemptsExhausted), "subscriberGivesUpAfterStop");
    server.reset();
    check(stream->tryPublish("orphan").status == RestPublishStatus::closed, "streamOutlivesServer");
}

} // namespace

int main() {
    deliveryPolicies();
    declaration();
    sseDelivery();
    reconnectGap();
    producerClose();
    rejections();
    heartbeats();
    binarySlowSubscriber();
    stopWithSubscribers();
    return finish("RestStreamTests");
}
