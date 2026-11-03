#include "RestTestSupport.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace gigante;
using namespace restTest;

namespace {

void retries() {
    std::atomic<int> flakyCalls{0};
    std::atomic<int> postCalls{0};
    RestServer server(developmentServer());
    server.GET("/api/v1/flaky", [&](const RestRequest&) {
        return ++flakyCalls < 3 ? RestResponse::error(503, "busy", "Try again.", true) : RestResponse::json("{\"ok\":true}");
    });
    server.POST("/api/v1/mutate", [&](const RestRequest&) {
        ++postCalls;
        return RestResponse::error(503, "busy", "Try again.", true);
    });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));

    RestCallOptions idempotent;
    idempotent.idempotent = true;
    RestResult result = client.call(RestMethod::get, "/api/v1/flaky", "", "application/json", idempotent);
    check(result.status == 200 && result.attempts == 3 && flakyCalls.load() == 3, "idempotentRetries", std::to_string(result.attempts));

    result = client.call(RestMethod::post, "/api/v1/mutate", "{}");
    check(result.status == 503 && result.attempts == 1 && postCalls.load() == 1, "mutationNeverResent", std::to_string(postCalls.load()));

    flakyCalls.store(-100);
    idempotent.maxAttempts = 2;
    result = client.call(RestMethod::get, "/api/v1/flaky", "", "application/json", idempotent);
    check(result.status == 503 && result.attempts == 2, "attemptLimit", std::to_string(result.attempts));
}

void deadlines() {
    RestServer server(developmentServer());
    server.GET("/api/v1/slow", [](const RestRequest&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        return RestResponse::json("{}");
    });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    RestCallOptions shortDeadline;
    shortDeadline.deadlineMilliseconds = 150;
    const auto before = std::chrono::steady_clock::now();
    const RestResult result = client.call(RestMethod::get, "/api/v1/slow", "", "application/json", shortDeadline);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    check(result.status == 0 && result.error != RestClientError::none && elapsed < 700, "deadlineEndsCall", std::to_string(elapsed));
}

void cancellation() {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    RestServer server(developmentServer());
    server.GET("/api/v1/slow", [&](const RestRequest&) {
        entered.store(true);
        waitUntil([&] { return release.load(); }, 5000);
        return RestResponse::json("{}");
    });
    server.GET("/api/v1/fast", [](const RestRequest&) { return RestResponse::json("{}"); });
    const RestStartResult started = server.start();
    RestClient client(developmentClient(started.port));
    RestResult result;
    const auto before = std::chrono::steady_clock::now();
    std::thread caller([&] { result = client.call(RestMethod::get, "/api/v1/slow"); });
    waitUntil([&] { return entered.load(); });
    client.cancel();
    caller.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before).count();
    check(result.error == RestClientError::canceled && elapsed < 3000, "cancelEndsCall", std::to_string(elapsed));
    release.store(true);
    check(client.call(RestMethod::get, "/api/v1/fast").status == 200, "callsWorkAfterCancel");
}

void asynchronousCalls() {
    std::atomic<bool> release{false};
    RestServer server(developmentServer());
    server.GET("/api/v1/value", [](const RestRequest&) { return RestResponse::text("value"); });
    server.GET("/api/v1/hold", [&](const RestRequest&) {
        waitUntil([&] { return release.load(); }, 5000);
        return RestResponse::json("{}");
    });
    const RestStartResult started = server.start();
    {
        RestClient client(developmentClient(started.port));
        std::atomic<int> completed{0};
        std::mutex mutex;
        std::vector<std::string> bodies;
        for (int index = 0; index < 8; ++index) {
            check(client.callAsync(RestMethod::get, "/api/v1/value", [&](RestResult result) {
                std::lock_guard<std::mutex> lock(mutex);
                bodies.push_back(result.body);
                ++completed;
            }), "asyncAccepted");
        }
        check(waitUntil([&] { return completed.load() == 8; }), "asyncCompletes", std::to_string(completed.load()));
        std::lock_guard<std::mutex> lock(mutex);
        check(bodies.size() == 8 && bodies.front() == "value", "asyncResults");
    }
    {
        RestClientConfig config = developmentClient(started.port);
        config.maxConcurrentAsyncCalls = 1;
        config.maxQueuedAsyncCalls = 2;
        std::atomic<int> callbacks{0};
        {
            RestClient client(config);
            int accepted = 0;
            for (int index = 0; index < 10; ++index) {
                accepted += client.callAsync(RestMethod::get, "/api/v1/hold", [&](RestResult) { ++callbacks; }) ? 1 : 0;
            }
            check(accepted < 10, "asyncQueueBounded", std::to_string(accepted));
        }
        const int atDestroy = callbacks.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        check(callbacks.load() == atDestroy, "noCallbackAfterClientDestroy");
    }
    release.store(true);
}

void unreachableServer() {
    RestClientConfig config = developmentClient(1);
    config.connectTimeoutMilliseconds = 200;
    RestClient client(config);
    RestCallOptions options;
    options.idempotent = true;
    options.maxAttempts = 2;
    const RestResult result = client.call(RestMethod::get, "/core/v1/health", "", "application/json", options);
    check(result.error == RestClientError::connection && result.attempts == 2, "connectionErrorRetried", result.errorText);
}

} // namespace

int main() {
    retries();
    deadlines();
    cancellation();
    asynchronousCalls();
    unreachableServer();
    return finish("RestClientTests");
}
