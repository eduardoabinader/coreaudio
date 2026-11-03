#include "RestTestSupport.h"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace gigante;
using namespace restTest;

namespace {

void replyBeforeCallback() {
    RestServer server(developmentServer());
    std::atomic<bool> replied{false};
    std::atomic<bool> callbackSawReply{false};
    std::atomic<bool> callbackRan{false};
    RestServiceCallbacks callbacks;
    callbacks.start = [&] {
        const bool sawReply = waitUntil([&] { return replied.load(); }, 3000);
        callbackSawReply.store(sawReply);
        callbackRan.store(true);
        return sawReply;
    };
    callbacks.stop = [] { return false; };
    callbacks.restart = []() -> bool { throw std::runtime_error("secret"); };
    check(server.registerService("capture", callbacks), "serviceRegistered");
    check(!server.registerService("capture", callbacks), "duplicateServiceRefused");
    check(!server.registerService("server", callbacks), "reservedServiceNameRefused");
    check(!server.registerService("a/b", callbacks), "invalidServiceNameRefused");
    RestServiceCallbacks startOnly;
    startOnly.start = [] { return true; };
    server.registerService("partial", startOnly);
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    RestResult result = client.startService("capture");
    check(result.status == 202 && contains(result.body, "\"accepted\":true"), "startAccepted", result.body);
    replied.store(true);
    check(waitUntil([&] { return callbackRan.load(); }), "callbackRan");
    check(callbackSawReply.load(), "replyArrivedFirst");
    check(waitUntil([&] { return contains(client.status().body, "\"name\":\"capture\",\"state\":\"running\",\"lastResult\":\"succeeded\""); }), "statusShowsRunning", client.status().body);

    check(client.stopService("capture").status == 202, "stopAccepted");
    check(waitUntil([&] { return contains(client.status().body, "\"name\":\"capture\",\"state\":\"failed\""); }), "failedCallbackReported");
    check(client.restartService("capture").status == 202, "restartAccepted");
    check(waitUntil([&] { return contains(client.call(RestMethod::get, "/core/v1/services").body, "\"lastResult\":\"failed\""); }), "throwingCallbackReported");
    check(!contains(client.status().body, "secret"), "callbackExceptionHidden");

    result = client.startService("missing");
    check(result.status == 404 && contains(result.body, "serviceNotRegistered"), "unknownService", result.body);
    result = client.stopService("partial");
    check(result.status == 405 && contains(result.body, "actionNotOffered"), "actionNotOffered", result.body);
    check(client.stopService("server").status == 404, "remoteStopIsOptIn");
    check(contains(client.getCapabilities().body, "\"services\""), "capabilitiesListServices");
    check(server.stop(), "stopsAfterServices", server.lastStopCause());
}

void remoteStopDoesNotDeadlock() {
    RestServer server(developmentServer());
    server.enableRemoteStop();
    server.GET("/api/v1/self", [&server](const RestRequest&) {
        server.requestStop();
        return RestResponse::json("{}");
    });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    const RestResult result = client.stopService("server");
    check(result.status == 202, "remoteStopAccepted", result.body);
    check(waitUntil([&] { return server.state() == RestServerState::stopped; }, 8000), "remoteStopFinishes");

    const RestStartResult again = server.start();
    check(again.started, "restartAfterRemoteStop", again.cause);
    RestClient second(developmentClient(again.port));
    check(second.call(RestMethod::get, "/api/v1/self").status == 200, "stopFromHandler");
    check(waitUntil([&] { return server.state() == RestServerState::stopped; }, 8000), "handlerStopFinishes");
}

void restartCycles() {
    RestServer server(developmentServer());
    server.GET("/api/v1/ping", [](const RestRequest&) { return RestResponse::json("{}"); });
    int okCycles = 0;
    for (int cycle = 0; cycle < 10; ++cycle) {
        const RestStartResult started = server.start();
        if (!started.started) {
            break;
        }
        RestClient client(developmentClient(started.port));
        if (client.call(RestMethod::get, "/api/v1/ping").status == 200 && server.stop()) {
            ++okCycles;
        }
    }
    check(okCycles == 10, "tenRestarts", std::to_string(okCycles));
    check(server.stop() && server.stop(), "repeatStopIsSafe");
    check(server.start().started, "startAfterRepeatStop");
    check(!server.start().started, "doubleStartRefused");
}

void stuckHandlerReported() {
    std::atomic<bool> release{false};
    std::atomic<bool> entered{false};
    std::atomic<bool> finished{false};
    {
        RestServer server(developmentServer());
        server.GET("/api/v1/stuck", [&](const RestRequest&) {
            entered.store(true);
            while (!release.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            finished.store(true);
            return RestResponse::json("{}");
        });
        const RestStartResult started = server.start();
        std::thread caller([port = started.port] {
            RestClientConfig config = developmentClient(port);
            config.readTimeoutMilliseconds = 8000;
            RestClient client(config);
            client.call(RestMethod::get, "/api/v1/stuck");
        });
        waitUntil([&] { return entered.load(); });
        const auto before = std::chrono::steady_clock::now();
        const bool stopped = server.stop(500);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
        check(!stopped && elapsed < 2000, "stopTimesOut", std::to_string(elapsed));
        check(contains(server.lastStopCause(), "still running"), "stopNamesCause", server.lastStopCause());
        check(server.state() == RestServerState::draining, "stateDraining");
        release.store(true);
        check(server.stop(5000), "stopAfterRelease", server.lastStopCause());
        caller.join();
    }
    check(finished.load(), "stuckHandlerFinished");
}

class HeldDispatcher final : public RestDispatcher {
public:
    bool tryPost(std::function<void()> task) override {
        held = std::move(task);
        return true;
    }
    std::function<void()> held;
};

void noCallbackAfterDestroy() {
    auto dispatcher = std::make_shared<HeldDispatcher>();
    std::atomic<int> handlerCalls{0};
    {
        RestServerConfig config = developmentServer();
        config.limits.dispatcherDeadlineMilliseconds = 100;
        RestServer server(config);
        server.setDispatcher(dispatcher);
        server.GET("/api/v1/held", [&](const RestRequest&) {
            ++handlerCalls;
            return RestResponse::json("{}");
        });
        const RestStartResult started = server.start();
        RestClient client(developmentClient(started.port));
        check(client.call(RestMethod::get, "/api/v1/held").status == 504, "heldTaskTimesOut");
    }
    check(static_cast<bool>(dispatcher->held), "taskWasHeld");
    dispatcher->held();
    check(handlerCalls.load() == 0, "noHandlerAfterDestroy");
}

} // namespace

int main() {
    replyBeforeCallback();
    remoteStopDoesNotDeadlock();
    restartCycles();
    stuckHandlerReported();
    noCallbackAfterDestroy();
    return finish("RestServiceLifecycleTests");
}
