#include "gigante/RestClient.h"

#include "ClientContext.h"
#include "RestJson.h"
#include "Router.h"
#include "SecurityPolicy.h"
#include "StreamSubscription.h"
#include "TransferClient.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <mutex>
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

std::string percentEncode(const std::string& value) {
    std::string result;
    for (const char current : value) {
        const auto byte = static_cast<unsigned char>(current);
        if (std::isalnum(byte) != 0 || current == '-' || current == '_' || current == '.' || current == '~') {
            result += current;
        } else {
            char escaped[4];
            std::snprintf(escaped, sizeof(escaped), "%%%02X", byte);
            result += escaped;
        }
    }
    return result;
}

} // namespace

class RestClient::Implementation {
public:
    explicit Implementation(RestClientConfig clientConfig) : context(std::move(clientConfig)), transfers(context) {
        const RestClientConfig& config = context.config();
        const ClientSecurityDecision decision = SecurityPolicy::checkClient(config, GIGANTE_REST_HAS_TLS != 0, GIGANTE_REST_ALLOW_PLAINTEXT != 0);
        if (!decision.allowed) {
            configurationError = decision.cause;
            context.log(RestLogLevel::error, "restClientRefused", decision.cause);
        } else if (!decision.warning.empty()) {
            context.log(RestLogLevel::warn, "restClientVerificationDisabled", decision.warning);
        }
    }

    ~Implementation() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closing = true;
            pending.clear();
            workAvailable.notify_all();
        }
        context.cancel();
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
        const std::uint64_t generation = context.generation();
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
            ClientResponse response;
            {
                TrackedTransport tracked(context, context.transportConfig(static_cast<int>(remaining)), generation);
                if (!tracked.isUsable()) {
                    result.error = RestClientError::canceled;
                    result.errorText = "the call was canceled";
                    return result;
                }
                response = tracked.transport().request(toTransportMethod(method), path, body, contentType, options.headers);
            }
            result.attempts = attempt;
            result.status = response.status;
            result.body = response.body;
            result.errorText = response.error == ClientError::none ? std::string() : response.errorText;
            result.error = context.isCanceled(generation) ? RestClientError::canceled : toRestError(response.error);
            if (result.error == RestClientError::canceled || !isRetryable(result) || attempt == attempts) {
                return result;
            }
            context.waitUntil(std::min(deadline, Clock::now() + std::chrono::milliseconds(backoffMilliseconds)), generation);
            backoffMilliseconds *= 2;
        }
        return result;
    }

    bool callAsync(RestMethod method, const std::string& path, std::function<void(RestResult)> callback, const std::string& body, const std::string& contentType, RestCallOptions options) {
        std::lock_guard<std::mutex> lock(mutex);
        if (closing || pending.size() >= context.config().maxQueuedAsyncCalls) {
            return false;
        }
        pending.push_back([this, method, path, body, contentType, options = std::move(options), work = std::move(callback)] {
            RestResult result = call(method, path, body, contentType, options);
            if (work) {
                try {
                    work(std::move(result));
                } catch (const std::exception&) {
                    context.log(RestLogLevel::error, "restClientCallbackException", path);
                } catch (...) {
                    context.log(RestLogLevel::error, "restClientCallbackException", path);
                }
            }
        });
        if (idleWorkers == 0 && workers.size() < context.config().maxConcurrentAsyncCalls) {
            workers.emplace_back([this] { runWorker(); });
        }
        workAvailable.notify_one();
        return true;
    }

    std::unique_ptr<RestSubscription> subscribe(const std::string& streamId, std::function<void(const RestStreamEvent&)> onEvent, std::function<void(RestSubscriptionEnd, const std::string&)> onClosed, RestSubscriptionOptions options) {
        auto subscription = std::make_unique<StreamSubscription>(
            context.config(), "/core/v1/streams/" + streamId + "/live", std::move(onEvent), std::move(onClosed), std::move(options));
        if (!configurationError.empty()) {
            subscription->startClosed(RestSubscriptionEnd::refusedByPolicy, configurationError);
        } else if (!Router::isValidPathParameter(streamId)) {
            subscription->startClosed(RestSubscriptionEnd::rejected, "the stream ID is not valid");
        } else {
            subscription->start();
        }
        return std::make_unique<RestSubscription>(std::move(subscription));
    }

    RestResult refused() const {
        RestResult result;
        result.error = RestClientError::refusedByPolicy;
        result.errorText = configurationError;
        return result;
    }

    ClientContext context;
    TransferClient transfers;
    std::string configurationError;

private:
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

    std::mutex mutex;
    std::condition_variable workAvailable;
    std::deque<std::function<void()>> pending;
    std::vector<std::thread> workers;
    std::size_t idleWorkers = 0;
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

RestResult RestClient::getIdentity() {
    return call(RestMethod::get, "/core/v1/getIdentity", "", "application/json", safeRead());
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

std::unique_ptr<RestSubscription> RestClient::subscribe(
    const std::string& streamId,
    std::function<void(const RestStreamEvent&)> onEvent,
    std::function<void(RestSubscriptionEnd, const std::string& detail)> onClosed,
    RestSubscriptionOptions options) {
    return implementation->subscribe(streamId, std::move(onEvent), std::move(onClosed), std::move(options));
}

RestResult RestClient::listResources() {
    return call(RestMethod::get, "/core/v1/resources", "", "application/json", safeRead());
}

RestResult RestClient::upload(const std::string& resourceId, std::uint64_t size, const std::string& sha256, RestUploadSource source, RestCallOptions options) {
    if (!implementation->configurationError.empty()) {
        return implementation->refused();
    }
    return implementation->transfers.upload(resourceId, size, sha256, source, options);
}

RestResult RestClient::download(const std::string& resourceId, RestDownloadSink sink, std::uint64_t fromOffset, RestCallOptions options) {
    if (!implementation->configurationError.empty()) {
        return implementation->refused();
    }
    return implementation->transfers.download(resourceId, sink, fromOffset, options);
}

RestResult RestClient::getLog(const std::string& fileGlob, const std::vector<std::string>& filters, std::size_t maxLines) {
    std::string path = "/core/v1/getLog?file=" + percentEncode(fileGlob) + "&maxLines=" + std::to_string(maxLines);
    for (const std::string& filter : filters) {
        path += "&filter=" + percentEncode(filter);
    }
    return call(RestMethod::get, path, "", "application/json", safeRead());
}

RestResult RestClient::cmdStatus(const std::string& nameGlob) {
    return call(RestMethod::get, "/core/v1/cmdStatus?name=" + percentEncode(nameGlob), "", "application/json", safeRead());
}

RestResult RestClient::appendLogMarker(const std::string& command, const std::string& event, const std::string& text, const std::string& origin) {
    const std::string body = "{\"command\":" + jsonString(command) + ",\"event\":" + jsonString(event) + ",\"text\":" + jsonString(text) + ",\"origin\":" + jsonString(origin) + "}";
    return call(RestMethod::post, "/core/v1/appendLogMarker", body);
}

void RestClient::cancel() {
    implementation->context.cancel();
}

} // namespace gigante
