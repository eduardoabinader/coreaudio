#include "gigante/RestClient.h"

#include "CppHttplibClientTransport.h"
#include "SecurityPolicy.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef GIGANTE_REST_ALLOW_PLAINTEXT
#define GIGANTE_REST_ALLOW_PLAINTEXT 1
#endif
#ifndef GIGANTE_REST_HAS_TLS
#define GIGANTE_REST_HAS_TLS 1
#endif

namespace gigante {

namespace {

using Clock = std::chrono::steady_clock;

HttpMethod toTransportMethod(RestMethod method) {
    switch (method) {
        case RestMethod::get: return HttpMethod::get;
        case RestMethod::post: return HttpMethod::post;
        case RestMethod::put: return HttpMethod::put;
        case RestMethod::patch: return HttpMethod::patch;
        case RestMethod::del: return HttpMethod::del;
        case RestMethod::options: return HttpMethod::options;
    }
    return HttpMethod::get;
}

RestClientError toRestError(ClientError error) {
    switch (error) {
        case ClientError::none: return RestClientError::none;
        case ClientError::connection: return RestClientError::connection;
        case ClientError::tlsVerification: return RestClientError::tlsVerification;
        case ClientError::hostnameVerification: return RestClientError::hostnameVerification;
        case ClientError::tlsHandshake: return RestClientError::tlsHandshake;
        case ClientError::canceled: return RestClientError::canceled;
        case ClientError::other: return RestClientError::other;
    }
    return RestClientError::other;
}

bool isRetryable(const RestResult& result) {
    return result.status == 503 || result.error == RestClientError::connection;
}

} // namespace

class RestClient::Implementation {
public:
    explicit Implementation(RestClientConfig clientConfig) : config(std::move(clientConfig)) {
        const ClientSecurityDecision decision = SecurityPolicy::checkClient(config, GIGANTE_REST_HAS_TLS != 0, GIGANTE_REST_ALLOW_PLAINTEXT != 0);
        if (!decision.allowed) {
            configurationError = decision.cause;
            log(RestLogLevel::error, "restClientRefused", decision.cause);
        } else if (!decision.warning.empty()) {
            log(RestLogLevel::warn, "restClientVerificationDisabled", decision.warning);
        }
    }

    ~Implementation() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closing = true;
            pending.clear();
            workAvailable.notify_all();
        }
        cancel();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }

    RestResult call(RestMethod method, const std::string& path, const std::string& body, const std::string& contentType, const RestCallOptions& options) {
        RestResult result;
        if (!configurationError.empty()) {
            result.error = RestClientError::refusedByPolicy;
            result.errorText = configurationError;
            return result;
        }
        const std::uint64_t generation = cancelGeneration.load();
        const auto deadline = Clock::now() + std::chrono::milliseconds(options.deadlineMilliseconds);
        const int attempts = options.idempotent ? std::max(1, options.maxAttempts) : 1;
        int backoffMilliseconds = 100;
        for (int attempt = 1; attempt <= attempts; ++attempt) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            if (remaining <= 0) {
                result.error = RestClientError::deadline;
                result.errorText = "the call deadline passed";
                return result;
            }
            const int attemptBudget = static_cast<int>(remaining);
            ClientTransportConfig transportConfig;
            transportConfig.baseUrl = config.baseUrl;
            transportConfig.caCertificatePath = config.caCertificatePath;
            transportConfig.clientCertificatePath = config.clientCertificatePath;
            transportConfig.clientPrivateKeyPath = config.clientPrivateKeyPath;
            transportConfig.verifyPeer = config.verifyPeer;
            transportConfig.verifyHostname = config.verifyHostname;
            transportConfig.connectTimeoutMilliseconds = std::min(config.connectTimeoutMilliseconds, attemptBudget);
            transportConfig.readTimeoutMilliseconds = std::min(config.readTimeoutMilliseconds, attemptBudget);
            transportConfig.writeTimeoutMilliseconds = std::min(config.writeTimeoutMilliseconds, attemptBudget);
            CppHttplibClientTransport transport(transportConfig);
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (cancelGeneration.load() != generation) {
                    result.error = RestClientError::canceled;
                    result.errorText = "the call was canceled";
                    return result;
                }
                active.insert(&transport);
            }
            const ClientResponse response = transport.request(toTransportMethod(method), path, body, contentType, options.headers);
            {
                std::lock_guard<std::mutex> lock(mutex);
                active.erase(&transport);
            }
            result.attempts = attempt;
            result.status = response.status;
            result.body = response.body;
            result.errorText = response.error == ClientError::none ? std::string() : response.errorText;
            result.error = cancelGeneration.load() != generation ? RestClientError::canceled : toRestError(response.error);
            if (result.error == RestClientError::canceled || !isRetryable(result) || attempt == attempts) {
                return result;
            }
            std::unique_lock<std::mutex> lock(mutex);
            const auto wakeAt = std::min(deadline, Clock::now() + std::chrono::milliseconds(backoffMilliseconds));
            cancelled.wait_until(lock, wakeAt, [&] { return cancelGeneration.load() != generation; });
            backoffMilliseconds *= 2;
        }
        return result;
    }

    bool callAsync(RestMethod method, const std::string& path, std::function<void(RestResult)> callback, const std::string& body, const std::string& contentType, RestCallOptions options) {
        std::lock_guard<std::mutex> lock(mutex);
        if (closing || pending.size() >= config.maxQueuedAsyncCalls) {
            return false;
        }
        pending.push_back([this, method, path, body, contentType, options = std::move(options), work = std::move(callback)] {
            RestResult result = call(method, path, body, contentType, options);
            if (work) {
                try {
                    work(std::move(result));
                } catch (const std::exception&) {
                    log(RestLogLevel::error, "restClientCallbackException", path);
                } catch (...) {
                    log(RestLogLevel::error, "restClientCallbackException", path);
                }
            }
        });
        if (idleWorkers == 0 && workers.size() < config.maxConcurrentAsyncCalls) {
            workers.emplace_back([this] { runWorker(); });
        }
        workAvailable.notify_one();
        return true;
    }

    void cancel() {
        std::lock_guard<std::mutex> lock(mutex);
        cancelGeneration.fetch_add(1);
        for (CppHttplibClientTransport* transport : active) {
            transport->cancel();
        }
        cancelled.notify_all();
    }

    std::string configurationError;

private:
    void log(RestLogLevel level, const std::string& event, const std::string& detail) const {
        if (!config.log) {
            return;
        }
        try {
            config.log(level, event, detail);
        } catch (const std::exception&) {
        } catch (...) {
        }
    }

    void runWorker() {
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            ++idleWorkers;
            workAvailable.wait(lock, [this] { return closing || !pending.empty(); });
            --idleWorkers;
            if (closing) {
                return;
            }
            std::function<void()> work = std::move(pending.front());
            pending.pop_front();
            lock.unlock();
            work();
            lock.lock();
        }
    }

    RestClientConfig config;
    std::mutex mutex;
    std::condition_variable cancelled;
    std::condition_variable workAvailable;
    std::set<CppHttplibClientTransport*> active;
    std::deque<std::function<void()>> pending;
    std::vector<std::thread> workers;
    std::size_t idleWorkers = 0;
    std::atomic<std::uint64_t> cancelGeneration{0};
    bool closing = false;
};

RestClient::RestClient(RestClientConfig config) : implementation(std::make_unique<Implementation>(std::move(config))) {}

RestClient::~RestClient() = default;

std::string RestClient::configurationError() const {
    return implementation->configurationError;
}

RestResult RestClient::call(RestMethod method, const std::string& path, const std::string& body, const std::string& contentType, RestCallOptions options) {
    return implementation->call(method, path, body, contentType, options);
}

bool RestClient::callAsync(RestMethod method, const std::string& path, std::function<void(RestResult)> callback, const std::string& body, const std::string& contentType, RestCallOptions options) {
    return implementation->callAsync(method, path, std::move(callback), body, contentType, std::move(options));
}

namespace {

RestCallOptions safeRead() {
    RestCallOptions options;
    options.idempotent = true;
    return options;
}

} // namespace

RestResult RestClient::health() {
    return call(RestMethod::get, "/core/v1/health", "", "application/json", safeRead());
}

RestResult RestClient::heartbeat() {
    return call(RestMethod::get, "/core/v1/heartbeat", "", "application/json", safeRead());
}

RestResult RestClient::status() {
    return call(RestMethod::get, "/core/v1/status", "", "application/json", safeRead());
}

RestResult RestClient::getBuildVersion() {
    return call(RestMethod::get, "/core/v1/getBuildVersion", "", "application/json", safeRead());
}

RestResult RestClient::getCapabilities() {
    return call(RestMethod::get, "/core/v1/getCapabilities", "", "application/json", safeRead());
}

RestResult RestClient::checkDeps() {
    return call(RestMethod::get, "/core/v1/checkDeps", "", "application/json", safeRead());
}

RestResult RestClient::startService(const std::string& name) {
    return call(RestMethod::post, "/core/v1/services/" + name + "/start");
}

RestResult RestClient::stopService(const std::string& name) {
    return call(RestMethod::post, "/core/v1/services/" + name + "/stop");
}

RestResult RestClient::restartService(const std::string& name) {
    return call(RestMethod::post, "/core/v1/services/" + name + "/restart");
}

void RestClient::cancel() {
    implementation->cancel();
}

} // namespace gigante
