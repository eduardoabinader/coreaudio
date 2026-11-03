#include "StreamSubscription.h"

#include "gigante/RestSubscription.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <utility>

namespace gigante {

namespace {

constexpr std::size_t maxLineBytes = 1024 * 1024;
constexpr std::size_t maxErrorBodyBytes = 4096;
constexpr int maxBackoffMilliseconds = 30000;

} // namespace

bool SseParser::feed(const char* data, std::size_t size, const std::function<bool(const Event&)>& dispatch) {
    for (std::size_t index = 0; index < size && !stopped; ++index) {
        const char current = data[index];
        if (current == '\n') {
            if (!pending.empty() && pending.back() == '\r') {
                pending.pop_back();
            }
            const std::string text = std::move(pending);
            pending.clear();
            if (!line(text, dispatch)) {
                stopped = true;
            }
        } else {
            pending += current;
            if (pending.size() > maxLineBytes) {
                return false;
            }
        }
    }
    return true;
}

bool SseParser::line(const std::string& text, const std::function<bool(const Event&)>& dispatch) {
    if (text.empty()) {
        bool keepGoing = true;
        if (hasData) {
            keepGoing = dispatch({eventName, dataBuffer, lastId});
        }
        eventName.clear();
        dataBuffer.clear();
        hasData = false;
        return keepGoing;
    }
    if (text.front() == ':') {
        return true;
    }
    const std::size_t colon = text.find(':');
    const std::string field = text.substr(0, colon);
    std::string value = colon == std::string::npos ? std::string() : text.substr(colon + 1);
    if (!value.empty() && value.front() == ' ') {
        value.erase(0, 1);
    }
    if (field == "event") {
        eventName = value;
    } else if (field == "data") {
        if (dataBuffer.size() + value.size() > maxLineBytes) {
            return false;
        }
        dataBuffer += (hasData ? "\n" : "") + value;
        hasData = true;
    } else if (field == "id") {
        if (value.find('\0') == std::string::npos) {
            lastId = value;
        }
    } else if (field == "retry") {
        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (!value.empty() && end != nullptr && *end == '\0' && parsed >= 0) {
            retry = static_cast<int>(std::min<long>(parsed, maxBackoffMilliseconds));
        }
    }
    return true;
}

const std::string& SseParser::lastEventId() const {
    return lastId;
}

int SseParser::retryMilliseconds() const {
    return retry;
}

StreamSubscription::StreamSubscription(
    RestClientConfig clientConfig, std::string streamPath, EventHandler eventHandler, CloseHandler closeHandler, RestSubscriptionOptions subscriptionOptions)
    : config(std::move(clientConfig)),
      path(std::move(streamPath)),
      onEvent(std::move(eventHandler)),
      onClosed(std::move(closeHandler)),
      options(std::move(subscriptionOptions)) {}

StreamSubscription::~StreamSubscription() {
    cancel();
    if (worker.joinable()) {
        worker.join();
    }
}

void StreamSubscription::start() {
    worker = std::thread([this] { run(); });
}

void StreamSubscription::startClosed(RestSubscriptionEnd reason, std::string detail) {
    worker = std::thread([this, reason, text = std::move(detail)] { finish(reason, text); });
}

void StreamSubscription::cancel() {
    std::lock_guard<std::mutex> lock(mutex);
    canceled = true;
    if (current != nullptr) {
        current->cancel();
    }
    wake.notify_all();
}

bool StreamSubscription::isActive() const {
    return active.load();
}

bool StreamSubscription::isCanceled() const {
    std::lock_guard<std::mutex> lock(mutex);
    return canceled;
}

void StreamSubscription::log(RestLogLevel level, const std::string& event, const std::string& detail) const {
    if (!config.log) {
        return;
    }
    try {
        config.log(level, event, detail);
    } catch (const std::exception&) {
    } catch (...) {
    }
}

void StreamSubscription::finish(RestSubscriptionEnd reason, const std::string& detail) {
    if (closedReported) {
        return;
    }
    closedReported = true;
    if (onClosed) {
        try {
            onClosed(reason, detail);
        } catch (const std::exception&) {
            log(RestLogLevel::error, "restSubscriptionCallbackException", path);
        } catch (...) {
            log(RestLogLevel::error, "restSubscriptionCallbackException", path);
        }
    }
    active.store(false);
}

void StreamSubscription::deliver(const RestStreamEvent& event) {
    if (!onEvent) {
        return;
    }
    try {
        onEvent(event);
    } catch (const std::exception&) {
        log(RestLogLevel::error, "restSubscriptionCallbackException", path);
    } catch (...) {
        log(RestLogLevel::error, "restSubscriptionCallbackException", path);
    }
}

void StreamSubscription::run() {
    std::string lastEventId = options.lastEventId;
    int failures = 0;
    int serverRetry = 0;
    const int attempts = std::max(1, options.maximumAttempts);
    for (;;) {
        ClientTransportConfig transportConfig;
        transportConfig.baseUrl = config.baseUrl;
        transportConfig.caCertificatePath = config.caCertificatePath;
        transportConfig.clientCertificatePath = config.clientCertificatePath;
        transportConfig.clientPrivateKeyPath = config.clientPrivateKeyPath;
        transportConfig.verifyPeer = config.verifyPeer;
        transportConfig.verifyHostname = config.verifyHostname;
        transportConfig.connectTimeoutMilliseconds = config.connectTimeoutMilliseconds;
        transportConfig.readTimeoutMilliseconds = options.idleTimeoutMilliseconds;
        transportConfig.writeTimeoutMilliseconds = config.writeTimeoutMilliseconds;
        CppHttplibClientTransport transport(transportConfig);
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (canceled) {
                break;
            }
            current = &transport;
        }
        std::map<std::string, std::string> headers = options.headers;
        headers["Accept"] = options.binary ? "application/octet-stream" : "text/event-stream";
        if (!options.binary && !lastEventId.empty()) {
            headers["Last-Event-ID"] = lastEventId;
        }
        int status = 0;
        bool delivered = false;
        bool ended = false;
        bool protocolError = false;
        std::string errorBody;
        SseParser parser;
        const ClientResponse response = transport.streamGet(
            path,
            headers,
            [&status](int responseStatus, const std::multimap<std::string, std::string>&) {
                status = responseStatus;
                return true;
            },
            [&](const char* data, std::size_t size) {
                if (status != 200) {
                    errorBody.append(data, std::min(size, maxErrorBodyBytes - std::min(errorBody.size(), maxErrorBodyBytes)));
                    return true;
                }
                if (options.binary) {
                    delivered = true;
                    deliver({"", std::string(data, size), ""});
                    return !isCanceled();
                }
                const bool parsed = parser.feed(data, size, [&](const SseParser::Event& event) {
                    if (event.event == "end") {
                        ended = true;
                        return false;
                    }
                    delivered = true;
                    deliver({event.event, event.data, event.id});
                    return !isCanceled();
                });
                protocolError = !parsed;
                return parsed && !ended && !isCanceled();
            });
        {
            std::lock_guard<std::mutex> lock(mutex);
            current = nullptr;
        }
        if (!parser.lastEventId().empty()) {
            lastEventId = parser.lastEventId();
        }
        serverRetry = std::max(serverRetry, parser.retryMilliseconds());
        if (isCanceled()) {
            break;
        }
        if (ended || status == 410) {
            finish(RestSubscriptionEnd::streamEnded, ended ? "" : "status 410");
            return;
        }
        if (status >= 400 && status < 500 && status != 408 && status != 429) {
            finish(RestSubscriptionEnd::rejected, "status " + std::to_string(status));
            return;
        }
        // Everything else reconnects within limits: a lost connection, an idle timeout,
        // a refused 503 or 5xx, or a malformed stream.
        failures = delivered ? 1 : failures + 1;
        if (failures >= attempts) {
            const std::string cause = protocolError ? "malformed stream" : (status != 0 ? "status " + std::to_string(status) : response.errorText);
            finish(RestSubscriptionEnd::attemptsExhausted, cause);
            return;
        }
        const int base = std::max(options.minimumReconnectMilliseconds, serverRetry);
        const int shift = std::min(failures - 1, 10);
        const int waitMilliseconds = std::min(maxBackoffMilliseconds, std::max(base, base << shift));
        std::unique_lock<std::mutex> lock(mutex);
        if (wake.wait_for(lock, std::chrono::milliseconds(waitMilliseconds), [this] { return canceled; })) {
            break;
        }
    }
    finish(RestSubscriptionEnd::canceled, "");
}

RestSubscription::RestSubscription(std::unique_ptr<StreamSubscription> streamSubscription) : subscription(std::move(streamSubscription)) {}

RestSubscription::~RestSubscription() = default;

void RestSubscription::cancel() {
    subscription->cancel();
}

bool RestSubscription::isActive() const {
    return subscription->isActive();
}

} // namespace gigante
