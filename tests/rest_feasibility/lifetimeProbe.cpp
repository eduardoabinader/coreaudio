#include "CapacityGate.h"
#include "CppHttplibClientTransport.h"
#include "CppHttplibTransport.h"
#include "LoadClient.h"
#include "ProbeReport.h"
#include "RestMetrics.h"
#include "Router.h"
#include "StreamHub.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;

namespace {

using Clock = std::chrono::steady_clock;

std::string certs;

TransportConfig serverConfig(bool useTls, const std::string& name = "server") {
    TransportConfig config;
    config.useTls = useTls;
    config.tls.certificatePath = certs + "/" + name + ".pem";
    config.tls.privateKeyPath = certs + "/" + name + ".key";
    config.baseThreads = 8;
    config.maxThreads = 48;
    config.maxQueuedConnections = 16;
    config.readTimeoutMilliseconds = 1000;
    config.writeTimeoutMilliseconds = 1000;
    config.keepAliveTimeoutMilliseconds = 500;
    return config;
}

ClientTransportConfig clientConfig(int port, bool useTls, const std::string& ca = "ca.pem") {
    ClientTransportConfig config;
    config.baseUrl = std::string(useTls ? "https" : "http") + "://localhost:" + std::to_string(port);
    if (useTls) {
        config.caCertificatePath = certs + "/" + ca;
    }
    return config;
}

void checkPolicies(ProbeReport& report) {
    RestMetrics metrics;
    CapacityGate gate(4, 0, metrics);
    StreamHub hub(metrics);
    for (const auto& entry : {std::make_pair(std::string("reliable"), DeliveryPolicy::reliable),
                              std::make_pair(std::string("realtime"), DeliveryPolicy::realtime),
                              std::make_pair(std::string("broadcast"), DeliveryPolicy::broadcast)}) {
        StreamDescriptor descriptor;
        descriptor.id = entry.first;
        descriptor.policy = entry.second;
        descriptor.maxQueuedBytesPerSubscriber = 8;
        hub.declare(descriptor);
    }
    const std::string chunk(4, 'x');

    auto reliable = hub.subscribe("reliable", std::move(*gate.tryAcquireStream()));
    bool refused = false;
    int acceptedChunks = 0;
    for (int index = 0; index < 4; ++index) {
        const PublishResult result = hub.publish("reliable", chunk);
        if (result.refused) {
            refused = true;
        } else {
            ++acceptedChunks;
        }
    }
    report.check("policy:reliableRefusesWithoutDropping", refused && acceptedChunks == 2 && metrics.snapshot().droppedChunks == 0);
    report.check("policy:reliableKeepsAcceptedChunks", reliable->next(0) != nullptr && reliable->next(0) != nullptr && reliable->next(0) == nullptr);

    auto realtime = hub.subscribe("realtime", std::move(*gate.tryAcquireStream()));
    for (int index = 0; index < 6; ++index) {
        hub.publish("realtime", std::to_string(1000 + index));
    }
    const auto newestChunk = realtime->next(0);
    report.check(
        "policy:realtimeDropsOldestAndCounts",
        metrics.snapshot().droppedChunks == 4 && newestChunk != nullptr && *newestChunk == "1004",
        "dropped=" + std::to_string(metrics.snapshot().droppedChunks));

    auto slow = hub.subscribe("broadcast", std::move(*gate.tryAcquireStream()));
    auto fast = hub.subscribe("broadcast", std::move(*gate.tryAcquireStream()));
    int fastReceived = 0;
    for (int index = 0; index < 6; ++index) {
        hub.publish("broadcast", chunk);
        if (fast->next(0)) {
            ++fastReceived;
        }
    }
    report.check("policy:broadcastEvictsOnlySlowSubscriber", slow->wasEvicted() && !fast->wasEvicted() && fastReceived == 6, "fastReceived=" + std::to_string(fastReceived));
    report.check("policy:capRefusesFifthStream", !gate.tryAcquireStream().has_value());

    const auto start = Clock::now();
    for (int index = 0; index < 200000; ++index) {
        hub.publish("broadcast", chunk);
    }
    const double perPublishMicroseconds = std::chrono::duration<double, std::micro>(Clock::now() - start).count() / 200000.0;
    report.check("policy:publishDoesNotBlock", perPublishMicroseconds < 100.0, "averageUs=" + std::to_string(perPublishMicroseconds));
    hub.cancelAll();
    slow->releaseLease();
    fast->releaseLease();
    reliable->releaseLease();
    realtime->releaseLease();
    report.check("policy:leasesReturn", gate.activeStreams() == 0);
}

struct StreamServer {
    RestMetrics metrics;
    CapacityGate gate{32, 0, metrics};
    StreamHub hub{metrics};
    CppHttplibTransport transport{metrics};
    Router router{transport};
    std::atomic<std::uint64_t> producerCalls{0};
    std::atomic<bool> stopped{false};
    std::atomic<std::uint64_t> callbacksAfterStop{0};
    int port = 0;

    bool start(bool useTls) {
        StreamDescriptor descriptor;
        descriptor.id = "live";
        descriptor.policy = DeliveryPolicy::realtime;
        descriptor.contentType = "application/octet-stream";
        hub.declare(descriptor);
        router.add(HttpMethod::get, "/stream", [this](const TransportRequest&) {
            auto lease = gate.tryAcquireStream();
            if (!lease) {
                TransportResponse busy;
                busy.status = 503;
                return busy;
            }
            TransportResponse response = hub.makeResponse("live", hub.subscribe("live", std::move(*lease)));
            StreamProducer inner = response.streamProducer;
            response.streamProducer = [this, inner](TransportWriter& writer) {
                producerCalls.fetch_add(1);
                if (stopped.load()) {
                    callbacksAfterStop.fetch_add(1);
                }
                return inner(writer);
            };
            return response;
        });
        const TransportStartResult result = transport.start(serverConfig(useTls));
        port = result.boundPort;
        return result.started;
    }
};

void stopDuringStreams(bool useTls, ProbeReport& report) {
    for (int cycle = 0; cycle < 3; ++cycle) {
        StreamServer server;
        if (!server.start(useTls)) {
            report.check("stop:serverStarts", false);
            return;
        }
        std::atomic<bool> publishing{true};
        std::thread publisher([&] {
            const std::string chunk(32 * 1024, 'p');
            while (publishing.load()) {
                server.hub.publish("live", chunk);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
        std::vector<std::thread> clients;
        const ClientTransportConfig config = clientConfig(server.port, useTls);
        for (int index = 0; index < 15; ++index) {
            clients.emplace_back([&config] {
                LoadClient client(config);
                client.openStream("/stream", 2.5);
            });
        }
        for (int index = 0; index < 5; ++index) {
            clients.emplace_back([&config] {
                LoadClient client(config);
                client.openStalledStream("/stream", 2.5);
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        const std::size_t activeBefore = server.gate.activeStreams();
        const auto start = Clock::now();
        server.gate.refuseNewLeases();
        server.hub.cancelAll();
        const bool stopped = server.transport.stop(8000);
        const double stopSeconds = std::chrono::duration<double>(Clock::now() - start).count();
        server.stopped.store(true);
        const std::uint64_t callsAtStop = server.producerCalls.load();
        publishing.store(false);
        publisher.join();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        for (auto& client : clients) {
            client.join();
        }
        const std::string prefix = "stop:cycle" + std::to_string(cycle);
        report.check(prefix + ":streamsWereActive", activeBefore > 0, "active=" + std::to_string(activeBefore));
        report.check(prefix + ":stopWithinBound", stopped, "seconds=" + std::to_string(stopSeconds));
        report.check(prefix + ":noCallbackAfterStop", server.callbacksAfterStop.load() == 0 && server.producerCalls.load() == callsAtStop);
        report.check(prefix + ":allLeasesReleased", server.gate.activeStreams() == 0 && server.hub.subscriberCount() == 0);
    }
}

void abruptDisconnects(bool useTls, ProbeReport& report) {
    StreamServer server;
    if (!server.start(useTls)) {
        report.check("disconnect:serverStarts", false);
        return;
    }
    std::atomic<bool> publishing{true};
    std::thread publisher([&] {
        while (publishing.load()) {
            server.hub.publish("live", std::string(1024, 'p'));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    const ClientTransportConfig config = clientConfig(server.port, useTls);
    for (int round = 0; round < 4; ++round) {
        std::vector<std::thread> clients;
        for (int index = 0; index < 10; ++index) {
            clients.emplace_back([&config] {
                LoadClient client(config);
                client.openStream("/stream", 0.3);
            });
        }
        for (auto& client : clients) {
            client.join();
        }
    }
    const auto deadline = Clock::now() + std::chrono::seconds(5);
    while (server.gate.activeStreams() != 0 && Clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    report.check("disconnect:leasesReturn", server.gate.activeStreams() == 0, "active=" + std::to_string(server.gate.activeStreams()));
    publishing.store(false);
    publisher.join();
    server.hub.cancelAll();
    report.check("disconnect:stops", server.transport.stop(8000));
}

void restartCycles(bool useTls, ProbeReport& report) {
    RestMetrics metrics;
    CppHttplibTransport transport(metrics);
    transport.addRoute(HttpMethod::get, "/health", [](const TransportRequest&) {
        TransportResponse response;
        response.body = "{}";
        return response;
    });
    int okCycles = 0;
    for (int cycle = 0; cycle < 20; ++cycle) {
        const TransportStartResult started = transport.start(serverConfig(useTls));
        if (!started.started) {
            break;
        }
        LoadClient client(clientConfig(started.boundPort, useTls));
        if (client.request("/health").status == 200 && transport.stop(5000)) {
            ++okCycles;
        }
    }
    report.check("restart:twentyCycles", okCycles == 20, "ok=" + std::to_string(okCycles));
    report.check("restart:repeatStopIsSafe", transport.stop(100) && transport.stop(100));
}

void twoInstances(ProbeReport& report) {
    RestMetrics metricsA;
    RestMetrics metricsB;
    CppHttplibTransport instanceA(metricsA);
    CppHttplibTransport instanceB(metricsB);
    for (auto* transport : {&instanceA, &instanceB}) {
        transport->addRoute(HttpMethod::get, "/health", [](const TransportRequest&) {
            TransportResponse response;
            response.body = "{}";
            return response;
        });
    }
    const TransportStartResult startedA = instanceA.start(serverConfig(true, "server"));
    const TransportStartResult startedB = instanceB.start(serverConfig(true, "instance"));
    report.check("instances:bothStart", startedA.started && startedB.started);
    LoadClient clientA(clientConfig(startedA.boundPort, true, "ca.pem"));
    LoadClient clientB(clientConfig(startedB.boundPort, true, "instanceCa.pem"));
    LoadClient clientBWrongCa(clientConfig(startedB.boundPort, true, "ca.pem"));
    report.check("instances:eachOwnCertificate", clientA.request("/health").status == 200 && clientB.request("/health").status == 200);
    report.check("instances:certificatesNotShared", clientBWrongCa.request("/health").status == 0);
    report.check("instances:stopOne", instanceA.stop(5000));
    report.check("instances:otherStillServes", clientB.request("/health").status == 200);
    report.check("instances:firstRefusesAfterStop", clientA.request("/health").status == 0);
    report.check("instances:separateMetrics", metricsA.snapshot().requests == 1 && metricsB.snapshot().requests == 2);
    instanceB.stop(5000);
}

void routerChecks(ProbeReport& report) {
    RestMetrics metrics;
    CppHttplibTransport transport(metrics);
    Router router(transport);
    std::mutex mutex;
    std::vector<std::string> seen;
    const RouteHandler handler = [&](const TransportRequest& request) {
        std::lock_guard<std::mutex> lock(mutex);
        seen.push_back(request.pathParameters.at("id"));
        TransportResponse response;
        response.body = "{}";
        return response;
    };
    const RouteHandler noop = [](const TransportRequest&) { return TransportResponse{}; };
    report.check("router:adds", router.add(HttpMethod::get, "/api/v1/items/{id}", handler) == RouteAddResult::added);
    report.check("router:refusesDuplicate", router.add(HttpMethod::get, "/api/v1/items/{id}", noop) == RouteAddResult::duplicate);
    report.check("router:refusesReservedPrefix", router.add(HttpMethod::get, "/core/v1/health", noop) == RouteAddResult::reservedPrefix);
    report.check("router:commonMayUseReservedPrefix", router.addCommon(HttpMethod::get, "/core/v1/health", noop) == RouteAddResult::added);
    for (const char* bad : {"items/{id}", "/items/{}", "/items/{id", "/items/id}", "/items/x{id}", "/items/{id}x", "/items/:id", "/items/(.*)", "/items/*"}) {
        report.check(std::string("router:refusesPattern:") + bad, router.add(HttpMethod::get, bad, noop) == RouteAddResult::invalidPattern);
    }
    router.add(HttpMethod::get, "/throw/{id}", [](const TransportRequest&) -> TransportResponse {
        throw std::runtime_error("secret internal path /private/key");
    });
    const TransportStartResult started = transport.start(serverConfig(false));
    report.check("router:starts", started.started);
    ClientTransportConfig config = clientConfig(started.boundPort, false);
    CppHttplibClientTransport client(config);
    const ClientResponse good = client.request(HttpMethod::get, "/api/v1/items/abc-1_2.3~4", "", "", {});
    report.check("router:validParameterReachesHandler", good.status == 200, "status=" + std::to_string(good.status));
    for (const char* attack : {"%00", "abc%00def", "..", "%2e%2e", "%2E%2e", "a%2Fb", "%2F", "..%2F..%2Fetc", "a%5Cb", "%0d%0aX:y", "%uFF0E", "a%20b", "%252e"}) {
        std::size_t countBefore = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            countBefore = seen.size();
        }
        const ClientResponse response = client.request(HttpMethod::get, std::string("/api/v1/items/") + attack, "", "", {});
        std::size_t countAfter = 0;
        {
            std::lock_guard<std::mutex> lock(mutex);
            countAfter = seen.size();
        }
        report.check(
            std::string("router:rejectsParameter:") + attack,
            response.status != 200 && countAfter == countBefore,
            "status=" + std::to_string(response.status));
    }
    const ClientResponse thrown = client.request(HttpMethod::get, "/throw/abc", "", "", {});
    report.check("router:exceptionBecomes500", thrown.status == 500, "status=" + std::to_string(thrown.status));
    report.check("router:exceptionTextNotExposed", thrown.body.find("secret") == std::string::npos, thrown.body);
    report.check("router:exceptionCounted", metrics.snapshot().handlerExceptions >= 1);
    transport.stop(5000);
}

void stuckHandler(ProbeReport& report) {
    RestMetrics metrics;
    CppHttplibTransport transport(metrics);
    std::atomic<bool> release{false};
    std::atomic<bool> entered{false};
    transport.addRoute(HttpMethod::get, "/stuck", [&](const TransportRequest&) {
        entered.store(true);
        while (!release.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return TransportResponse{};
    });
    const TransportStartResult started = transport.start(serverConfig(false));
    std::thread caller([port = started.boundPort] {
        LoadClient client(clientConfig(port, false));
        client.request("/stuck");
    });
    while (!entered.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto start = Clock::now();
    const bool firstStop = transport.stop(1000);
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    report.check("stuck:stopReportsTimeout", !firstStop && seconds < 2.0, "seconds=" + std::to_string(seconds));
    release.store(true);
    report.check("stuck:stopCompletesAfterRelease", transport.stop(5000));
    caller.join();
}

void globalState(ProbeReport& report) {
    struct sigaction before {};
    sigaction(SIGPIPE, nullptr, &before);
    report.note("global:sigpipeAfterServerUse", before.sa_handler == SIG_IGN ? "ignored process-wide" : "not ignored");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: lifetimeProbe certsDirectory [tls|plain]\n";
        return 2;
    }
    certs = argv[1];
    const bool useTls = argc < 3 || std::string(argv[2]) == "tls";
    ProbeReport report("lifetime");
    checkPolicies(report);
    routerChecks(report);
    stopDuringStreams(useTls, report);
    abruptDisconnects(useTls, report);
    restartCycles(useTls, report);
    twoInstances(report);
    stuckHandler(report);
    globalState(report);
    return report.finish();
}
