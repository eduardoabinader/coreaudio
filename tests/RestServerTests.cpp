#include "RestTestSupport.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;
using namespace restTest;

namespace {

class QueueDispatcher final : public RestDispatcher {
public:
    explicit QueueDispatcher(std::size_t maxQueued) : limit(maxQueued) {
        worker = std::thread([this] {
            std::unique_lock<std::mutex> lock(mutex);
            for (;;) {
                wake.wait(lock, [this] { return done || (!tasks.empty() && !paused); });
                if (done) {
                    return;
                }
                auto task = std::move(tasks.front());
                tasks.pop_front();
                lock.unlock();
                task();
                lock.lock();
            }
        });
    }
    ~QueueDispatcher() override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            done = true;
        }
        wake.notify_all();
        worker.join();
    }
    bool tryPost(std::function<void()> task) override {
        std::lock_guard<std::mutex> lock(mutex);
        if (tasks.size() >= limit) {
            return false;
        }
        tasks.push_back(std::move(task));
        wake.notify_all();
        return true;
    }
    void setPaused(bool value) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            paused = value;
        }
        wake.notify_all();
    }
    std::thread::id workerId() const {
        return worker.get_id();
    }

private:
    std::size_t limit;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::function<void()>> tasks;
    bool paused = false;
    bool done = false;
    std::thread worker;
};

void routesAndRequests() {
    RestServer server(developmentServer());
    const RestHandler item = [](const RestRequest& request) {
        return RestResponse::json("{\"id\":\"" + request.pathParameter("id") + "\",\"q\":\"" + request.queryParameter("q").value_or("") + "\",\"requestId\":\"" + request.requestId + "\"}");
    };
    check(server.GET("/api/v1/items/{id}", item) == RestRouteResult::added, "routeAdded");
    check(server.GET("/api/v1/items/{id}", item) == RestRouteResult::duplicate, "duplicateRefused");
    check(server.GET("/core/v1/health", item) == RestRouteResult::reservedPrefix, "reservedPrefixRefused");
    check(server.GET("/items/{id", item) == RestRouteResult::invalidPattern, "invalidPatternRefused");
    check(server.GET("/items/:id", item) == RestRouteResult::invalidPattern, "librarySyntaxRefused");
    server.POST("/api/v1/items", [](const RestRequest& request) { return RestResponse::json(request.body, 201); });
    server.PUT("/api/v1/items/{id}", item);
    server.PATCH("/api/v1/items/{id}", item);
    server.DELETE("/api/v1/items/{id}", [](const RestRequest&) { return RestResponse::json("", 204); });
    RestRouteOptions small;
    small.maxBodyBytes = 16;
    server.POST("/api/v1/small", [](const RestRequest&) { return RestResponse::json("{}"); }, small);
    server.GET("/api/v1/throws/{id}", [](const RestRequest&) -> RestResponse { throw std::runtime_error("secret /private/path"); });
    server.GET("/api/v1/refuses", [](const RestRequest&) { return RestResponse::error(409, "conflict", "That item already exists.", true); });

    const RestStartResult started = server.start();
    check(started.started && started.scheme == "http", "startsHttp", started.cause);
    check(server.state() == RestServerState::running, "stateRunning");
    check(server.GET("/late", item) == RestRouteResult::alreadyStarted, "routeAfterStartRefused");
    RestClient client(developmentClient(started.port));

    RestResult result = client.call(RestMethod::get, "/api/v1/items/abc-1?q=find");
    check(result.status == 200 && contains(result.body, "\"id\":\"abc-1\"") && contains(result.body, "\"q\":\"find\""), "pathAndQuery", result.body);
    RestCallOptions withHeaders;
    withHeaders.headers["X-Request-Id"] = "client-id-1";
    result = client.call(RestMethod::get, "/api/v1/items/a", "", "application/json", withHeaders);
    check(contains(result.body, "client-id-1"), "requestIdAccepted", result.body);
    withHeaders.headers["X-Request-Id"] = "bad id\"<>";
    result = client.call(RestMethod::get, "/api/v1/items/a", "", "application/json", withHeaders);
    check(result.status == 200 && !contains(result.body, "bad id"), "unsafeRequestIdReplaced", result.body);
    result = client.call(RestMethod::post, "/api/v1/items", "{\"a\":1}");
    check(result.status == 201 && result.body == "{\"a\":1}", "postBody", result.body);
    check(client.call(RestMethod::put, "/api/v1/items/a", "{}").status == 200, "put");
    check(client.call(RestMethod::patch, "/api/v1/items/a", "{}").status == 200, "patch");
    check(client.call(RestMethod::del, "/api/v1/items/a").status == 204, "delete");

    result = client.call(RestMethod::get, "/api/v1/unknown");
    check(result.status == 404 && contains(result.body, "\"version\":1") && contains(result.body, "\"code\":\"notFound\"") && contains(result.body, "requestId"), "structured404", result.body);
    result = client.call(RestMethod::patch, "/api/v1/items");
    check(result.status == 405 && contains(result.body, "methodNotAllowed"), "structured405", result.body);
    result = client.call(RestMethod::post, "/api/v1/small", std::string(64, 'x'));
    check(result.status == 413 && contains(result.body, "bodyTooLarge"), "routeBodyLimit", result.body);
    result = client.call(RestMethod::post, "/api/v1/items", std::string(2 * 1024 * 1024, 'x'));
    check(result.status == 413, "serverBodyLimit", std::to_string(result.status));
    result = client.call(RestMethod::get, "/api/v1/refuses");
    check(result.status == 409 && contains(result.body, "\"code\":\"conflict\"") && contains(result.body, "\"retryable\":true"), "handlerError", result.body);

    for (const char* attack : {"%00", "abc%00def", "..", "%2e%2e", "a%5Cb", "%0d%0aX:y", "a%20b"}) {
        result = client.call(RestMethod::get, std::string("/api/v1/items/") + attack);
        check(result.status == 400 && contains(result.body, "invalidPathParameter"), std::string("attackRejected:") + attack, std::to_string(result.status));
    }
    for (const char* attack : {"a%2Fb", "..%2F..%2Fetc"}) {
        check(client.call(RestMethod::get, std::string("/api/v1/items/") + attack).status != 200, std::string("attackRejected:") + attack);
    }

    result = client.call(RestMethod::get, "/api/v1/throws/a");
    check(result.status == 500 && !contains(result.body, "secret") && contains(result.body, "internalError"), "exceptionHidden", result.body);
    check(server.counters().handlerExceptions == 1, "exceptionCounted");
    check(server.stop(), "stops");
    check(server.state() == RestServerState::stopped && server.boundPort() == 0, "stateStopped");
}

void commonEndpoints() {
    RestServerConfig config = developmentServer();
    config.buildIdentity = "build-123";
    std::atomic<bool> databaseHealthy{true};
    RestServer server(config);
    server.addHealthCheck("database", [&] { return databaseHealthy.load(); });
    server.setStatusDetailProvider([] { return std::string("{\"mode\":\"test\"}"); });
    server.setHostFactsProvider([] { return std::string("{\"cores\":8}"); });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    RestResult result = client.health();
    check(result.status == 200 && contains(result.body, "\"name\":\"database\",\"ok\":true"), "healthOk", result.body);
    databaseHealthy.store(false);
    result = client.health();
    check(result.status == 503 && contains(result.body, "\"ok\":false"), "healthFails", result.body);
    result = client.heartbeat();
    check(result.status == 200 && contains(result.body, "uptimeMilliseconds") && contains(result.body, "instanceId"), "heartbeat", result.body);
    result = client.status();
    check(result.status == 200 && contains(result.body, "\"state\":\"running\"") && contains(result.body, "\"requested\":{\"scheme\":\"automatic\"") && contains(result.body, "\"effective\":{\"scheme\":\"http\"") && contains(result.body, "\"detail\":{\"mode\":\"test\"}"), "status", result.body);
    result = client.getBuildVersion();
    check(result.status == 200 && contains(result.body, "build-123"), "buildVersion", result.body);
    result = client.getCapabilities();
    check(result.status == 200 && contains(result.body, "\"hostFacts\":{\"cores\":8}") && !contains(result.body, "services"), "capabilities", result.body);
    result = client.checkDeps();
    check(result.status == 404 && contains(result.body, "notProvided"), "checkDepsMissing", result.body);
    check(client.call(RestMethod::get, "/core/v1/services").status == 404, "servicesOptIn");
    server.stop();

    RestServer withReport(developmentServer());
    withReport.setDependencyReportProvider([] { return std::string("{\"missing\":[]}"); });
    const RestStartResult reportStarted = withReport.start();
    RestClient reportClient(developmentClient(reportStarted.port));
    result = reportClient.checkDeps();
    check(result.status == 200 && contains(result.body, "missing"), "checkDepsProvided", result.body);
}

void authenticationAndCors() {
    RestServerConfig config = developmentServer();
    config.corsAllowOrigin = "https://app.example";
    std::atomic<int> handlerCalls{0};
    std::atomic<std::size_t> bodySeenByAuthenticator{0};
    RestServer server(config);
    server.POST("/api/v1/private", [&](const RestRequest&) {
        ++handlerCalls;
        return RestResponse::json("{}");
    });
    RestRouteOptions open;
    open.auth = RestAuth::none;
    server.GET("/api/v1/public", [](const RestRequest&) { return RestResponse::json("{}"); }, open);
    server.setAuthenticator([&](const RestRequest& request) {
        bodySeenByAuthenticator.store(request.body.size());
        return request.header("Authorization").value_or("") == "Bearer good";
    });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    RestResult result = client.call(RestMethod::post, "/api/v1/private", "{\"x\":1}");
    check(result.status == 401 && handlerCalls.load() == 0 && contains(result.body, "unauthenticated"), "authRequired", result.body);
    check(bodySeenByAuthenticator.load() == 0, "authRunsBeforeBody");
    RestCallOptions authorized;
    authorized.headers["Authorization"] = "Bearer good";
    check(client.call(RestMethod::post, "/api/v1/private", "{}", "application/json", authorized).status == 200, "authAccepted");
    check(client.call(RestMethod::get, "/api/v1/public").status == 200, "explicitNoAuth");
    check(client.health().status == 200 && client.heartbeat().status == 200, "healthOpen");
    check(client.status().status == 401, "statusProtected");
    check(server.counters().authenticationFailures == 2, "authFailuresCounted");
    RestCallOptions preflight;
    preflight.headers["Access-Control-Request-Method"] = "POST";
    preflight.headers["Origin"] = "https://app.example";
    check(client.call(RestMethod::options, "/api/v1/private", "", "", preflight).status == 204, "corsPreflight");
}

void concurrentHandlers() {
    std::mutex mutex;
    std::condition_variable allArrived;
    int arrived = 0;
    const int expected = 8;
    bool barrierMet = true;
    RestServer server(developmentServer());
    server.GET("/api/v1/concurrent", [&](const RestRequest&) {
        std::unique_lock<std::mutex> lock(mutex);
        ++arrived;
        allArrived.notify_all();
        const bool met = allArrived.wait_for(lock, std::chrono::seconds(5), [&] { return arrived >= expected; });
        barrierMet = barrierMet && met;
        return RestResponse::json("{}");
    });
    const RestStartResult started = server.start();
    std::vector<std::thread> callers;
    std::atomic<int> okCalls{0};
    for (int index = 0; index < expected; ++index) {
        callers.emplace_back([&, port = started.port] {
            RestClient client(developmentClient(port));
            if (client.call(RestMethod::get, "/api/v1/concurrent").status == 200) {
                ++okCalls;
            }
        });
    }
    for (auto& caller : callers) {
        caller.join();
    }
    check(barrierMet && okCalls.load() == expected, "handlersRunConcurrently", std::to_string(okCalls.load()));
}

void dispatcher() {
    auto queue = std::make_shared<QueueDispatcher>(1);
    RestServerConfig config = developmentServer();
    config.limits.dispatcherDeadlineMilliseconds = 300;
    std::atomic<bool> onWorker{false};
    std::atomic<int> calls{0};
    RestServer server(config);
    server.setDispatcher(queue);
    server.GET("/api/v1/onThread", [&](const RestRequest&) {
        ++calls;
        onWorker.store(std::this_thread::get_id() == queue->workerId());
        return RestResponse::json("{}");
    });
    server.GET("/api/v1/throws", [](const RestRequest&) -> RestResponse { throw std::runtime_error("secret"); });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    check(client.call(RestMethod::get, "/api/v1/onThread").status == 200 && onWorker.load(), "handlerOnInjectedThread");
    RestResult result = client.call(RestMethod::get, "/api/v1/throws");
    check(result.status == 500 && !contains(result.body, "secret"), "exceptionOnDispatcher", result.body);

    queue->setPaused(true);
    calls.store(0);
    RestResult first;
    std::thread waiting([&] {
        RestClient other(developmentClient(started.port));
        first = other.call(RestMethod::get, "/api/v1/onThread");
    });
    check(waitUntil([&] { return client.health().status == 200; }), "healthNeedsNoDispatcher");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    result = client.call(RestMethod::get, "/api/v1/onThread");
    check(result.status == 503 && contains(result.body, "dispatcherFull") && contains(result.body, "\"retryable\":true"), "queueFullGives503", result.body);
    waiting.join();
    check(first.status == 504 && contains(first.body, "handlerDeadline"), "deadlineGives504", first.body);
    check(server.counters().dispatcherFull == 1 && server.counters().dispatcherDeadlines == 1, "dispatcherCounted");
    queue->setPaused(false);
    check(waitUntil([&] { return calls.load() == 1; }), "lateHandlerStillRuns");
    check(client.call(RestMethod::get, "/api/v1/onThread").status == 200, "dispatcherRecovers");
}

void multipleInstances() {
    RestServer first(developmentServer());
    RestServer second(developmentServer());
    first.GET("/api/v1/name", [](const RestRequest&) { return RestResponse::text("first"); });
    second.GET("/api/v1/name", [](const RestRequest&) { return RestResponse::text("second"); });
    const RestStartResult startedFirst = first.start();
    const RestStartResult startedSecond = second.start();
    RestClient clientFirst(developmentClient(startedFirst.port));
    RestClient clientSecond(developmentClient(startedSecond.port));
    check(clientFirst.call(RestMethod::get, "/api/v1/name").body == "first", "firstInstance");
    check(clientSecond.call(RestMethod::get, "/api/v1/name").body == "second", "secondInstance");
    first.stop();
    check(clientSecond.call(RestMethod::get, "/api/v1/name").body == "second", "secondSurvivesFirstStop");
    check(clientFirst.call(RestMethod::get, "/api/v1/name").status == 0, "firstStopped");
}

int serve(const std::string& certs, bool useTls) {
    RestServerConfig config = developmentServer();
    config.buildIdentity = "interop";
    if (useTls) {
        config.scheme = RestScheme::https;
        config.tls.certificatePath = certs + "/server.pem";
        config.tls.privateKeyPath = certs + "/server.key";
    }
    RestServer server(config);
    server.GET("/api/v1/items/{id}", [](const RestRequest& request) {
        return RestResponse::json("{\"id\":\"" + request.pathParameter("id") + "\"}");
    });
    server.POST("/api/v1/echo", [](const RestRequest& request) { return RestResponse::json(request.body); });
    const RestStartResult started = server.start();
    if (!started.started) {
        std::cerr << started.cause << "\n";
        return 1;
    }
    std::cout << "PORT " << started.port << std::endl;
    std::string line;
    std::getline(std::cin, line);
    return server.stop() ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[2]) == "--serve") {
        return serve(argv[1], false);
    }
    if (argc >= 3 && std::string(argv[2]) == "--serveTls") {
        return serve(argv[1], true);
    }
    routesAndRequests();
    commonEndpoints();
    authenticationAndCors();
    concurrentHandlers();
    dispatcher();
    multipleInstances();
    return finish("RestServerTests");
}
