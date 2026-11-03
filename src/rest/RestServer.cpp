#include "gigante/RestServer.h"

#include "CapacityGate.h"
#include "CommonEndpoints.h"
#include "CopyService.h"
#include "CppHttplibTransport.h"
#include "DiagnosticsEndpoints.h"
#include "HandlerDispatcher.h"
#include "MiddlewareChain.h"
#include "RestJson.h"
#include "RestMetrics.h"
#include "Router.h"
#include "SecurityPolicy.h"
#include "ServiceRegistry.h"
#include "StreamHub.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <limits>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifndef GIGANTE_REST_ALLOW_PLAINTEXT
#define GIGANTE_REST_ALLOW_PLAINTEXT 1
#endif

namespace gigante {

std::string RestRequest::pathParameter(const std::string& name) const {
    const auto entry = pathParameters.find(name);
    return entry == pathParameters.end() ? std::string() : entry->second;
}

std::optional<std::string> RestRequest::queryParameter(const std::string& name) const {
    const auto entry = query.find(name);
    return entry == query.end() ? std::nullopt : std::optional<std::string>(entry->second);
}

std::optional<std::string> RestRequest::header(const std::string& name) const {
    const auto entry = headers.find(name);
    return entry == headers.end() ? std::nullopt : std::optional<std::string>(entry->second);
}

RestResponse RestResponse::json(std::string body, int status) {
    RestResponse response;
    response.status = status;
    response.body = std::move(body);
    return response;
}

RestResponse RestResponse::text(std::string body, int status) {
    RestResponse response = json(std::move(body), status);
    response.contentType = "text/plain";
    return response;
}

RestResponse RestResponse::error(int status, std::string code, std::string message, bool retryable) {
    RestResponse response;
    response.status = status;
    response.errorCode = std::move(code);
    response.errorMessage = std::move(message);
    response.retryable = retryable;
    return response;
}

namespace {

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

RestRouteResult toRouteResult(RouteAddResult result) {
    switch (result) {
        case RouteAddResult::added: return RestRouteResult::added;
        case RouteAddResult::reservedPrefix: return RestRouteResult::reservedPrefix;
        case RouteAddResult::duplicate: return RestRouteResult::duplicate;
        case RouteAddResult::invalidPattern: return RestRouteResult::invalidPattern;
    }
    return RestRouteResult::invalidPattern;
}

std::string randomInstanceId() {
    std::random_device device;
    char text[17];
    std::snprintf(text, sizeof(text), "%08x%08x", device(), device());
    return text;
}

const char* stateName(RestServerState state) {
    switch (state) {
        case RestServerState::created: return "created";
        case RestServerState::running: return "running";
        case RestServerState::draining: return "draining";
        case RestServerState::stopped: return "stopped";
    }
    return "unknown";
}

const char* errorCodeFor(int status) {
    switch (status) {
        case 400: return "badRequest";
        case 401: return "unauthenticated";
        case 403: return "forbidden";
        case 404: return "notFound";
        case 405: return "methodNotAllowed";
        case 408: return "requestTimeout";
        case 411: return "lengthRequired";
        case 413: return "bodyTooLarge";
        case 414: return "uriTooLong";
        case 431: return "headersTooLarge";
        default: return status >= 500 ? "internalError" : "requestRejected";
    }
}

std::string safeProviderValue(const std::function<std::string()>& provider) {
    if (!provider) {
        return "null";
    }
    try {
        const std::string value = provider();
        return value.empty() ? "null" : value;
    } catch (const std::exception&) {
        return "null";
    } catch (...) {
        return "null";
    }
}

} // namespace

class RestServer::Implementation {
public:
    explicit Implementation(RestServerConfig serverConfig)
        : config(std::move(serverConfig)),
          transport(metrics),
          router(transport),
          middleware(router, metrics, [this](int status, const std::string& code, const std::string& message, bool retryable, const TransportRequest& request) {
              return errorResponse(status, code, message, retryable, request);
          }),
          common(makeSources()),
          gate(std::make_shared<DispatchGate>()),
          capacity(config.limits.maxStreams, config.limits.maxTransfers, metrics),
          hub(std::make_shared<StreamHub>(metrics)),
          instanceId(randomInstanceId()),
          startedAt(std::chrono::steady_clock::now()) {
        router.setInvalidParameterResponder([this](const std::string&, const TransportRequest& request) {
            return errorResponse(400, "invalidPathParameter", "A path parameter has an invalid value.", false, request);
        });
    }

    ~Implementation() {
        {
            std::lock_guard<std::mutex> lock(stopThreadMutex);
            destroying = true;
            if (stopThread.joinable()) {
                stopThread.join();
            }
        }
        stopWithDeadline(10000);
        gate->close();
        gate->waitUntilIdle(std::chrono::steady_clock::time_point::max());
        hub->shutdown();
    }

    RestRouteResult add(RestMethod method, const std::string& pattern, RestHandler handler, const RestRouteOptions& options) {
        if (serverState.load() == RestServerState::running || serverState.load() == RestServerState::draining) {
            return RestRouteResult::alreadyStarted;
        }
        RouteInfo info;
        info.authRequired = options.auth == RestAuth::required;
        info.maxBodyBytes = options.maxBodyBytes;
        info.idempotent = options.idempotent;
        return toRouteResult(router.add(toTransportMethod(method), pattern, wrap(std::move(handler)), info));
    }

    RestStartResult start() {
        std::lock_guard<std::mutex> lock(lifecycleMutex);
        RestStartResult result;
        const RestServerState current = serverState.load();
        if (current == RestServerState::running || current == RestServerState::draining) {
            result.cause = std::string("the server is ") + stateName(current);
            return result;
        }
        const bool tlsAvailable = std::string(transport.tlsBackend()) != "none";
        const ServerSecurityDecision decision = SecurityPolicy::checkServer(config, static_cast<bool>(authenticator), tlsAvailable, GIGANTE_REST_ALLOW_PLAINTEXT != 0);
        if (!decision.allowed) {
            log(RestLogLevel::error, "restServerStartRefused", decision.cause);
            result.cause = decision.cause;
            return result;
        }
        if (!decision.warning.empty()) {
            log(RestLogLevel::warn, "restServerPlaintext", decision.warning);
        }
        const bool streamsEnabled = !hub->descriptors().empty();
        const std::size_t neededThreads = config.limits.reservedThreads + (streamsEnabled ? config.limits.maxStreams : 0) + (store ? config.limits.maxTransfers : 0);
        if (neededThreads > config.limits.maxThreads) {
            result.cause = "the stream and transfer caps plus reservedThreads (" + std::to_string(neededThreads) + ") exceed maxThreads (" + std::to_string(config.limits.maxThreads) + ")";
            log(RestLogLevel::error, "restServerStartRefused", result.cause);
            return result;
        }
        const std::size_t transferLimit = static_cast<std::size_t>(std::min<std::uint64_t>(config.limits.maxTransferBytes, std::numeric_limits<std::size_t>::max()));
        const std::size_t transportBodyLimit = store ? std::max(config.limits.maxBodyBytes, transferLimit) : config.limits.maxBodyBytes;
        if (!routesRegistered) {
            middleware.setTransportBodyLimit(transportBodyLimit);
            registerEngineRoutes();
            routesRegistered = true;
        }
        TransportConfig transportConfig;
        transportConfig.host = config.host;
        transportConfig.port = config.port;
        transportConfig.useTls = decision.useTls;
        transportConfig.tls.certificatePath = config.tls.certificatePath;
        transportConfig.tls.privateKeyPath = config.tls.privateKeyPath;
        transportConfig.tls.clientCaPath = config.tls.clientCaPath;
        transportConfig.baseThreads = config.limits.baseThreads;
        transportConfig.maxThreads = config.limits.maxThreads;
        transportConfig.maxQueuedConnections = config.limits.maxQueuedConnections;
        transportConfig.maxPayloadBytes = transportBodyLimit;
        transportConfig.readTimeoutMilliseconds = config.limits.readTimeoutMilliseconds;
        transportConfig.writeTimeoutMilliseconds = config.limits.writeTimeoutMilliseconds;
        transportConfig.keepAliveTimeoutMilliseconds = config.limits.keepAliveTimeoutMilliseconds;
        transportConfig.keepAliveMaxCount = config.limits.keepAliveMaxCount;
        gate->open();
        services.open();
        capacity.acceptNewLeases();
        hub->reopen();
        if (copy) {
            copy->reopen();
        }
        const TransportStartResult started = transport.start(transportConfig);
        if (!started.started) {
            services.close(std::chrono::steady_clock::time_point::max());
            log(RestLogLevel::error, "restServerStartFailed", started.cause);
            result.cause = started.cause;
            return result;
        }
        effectiveScheme = decision.useTls ? "https" : "http";
        port.store(started.boundPort);
        startedAt = std::chrono::steady_clock::now();
        serverState.store(RestServerState::running);
        log(RestLogLevel::info, "restServerStarted", effectiveScheme + " port " + std::to_string(started.boundPort));
        result.started = true;
        result.port = started.boundPort;
        result.scheme = effectiveScheme;
        return result;
    }

    bool stopWithDeadline(int timeoutMilliseconds) {
        std::lock_guard<std::mutex> lock(lifecycleMutex);
        const RestServerState current = serverState.load();
        if (current == RestServerState::created || current == RestServerState::stopped) {
            return true;
        }
        serverState.store(RestServerState::draining);
        gate->close();
        capacity.refuseNewLeases();
        hub->cancelAll();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
        std::string cause;
        if (!transport.stop(timeoutMilliseconds)) {
            cause = "a handler or connection is still running";
        } else {
            // No upload can be running now, so every uncommitted transfer is discarded.
            if (copy) {
                copy->close();
            }
            if (!services.close(deadline)) {
                cause = "a service callback is still running";
            } else if (!gate->waitUntilIdle(deadline)) {
                cause = "a handler on the injected dispatcher is still running";
            }
        }
        {
            std::lock_guard<std::mutex> causeLock(causeMutex);
            stopCause = cause;
        }
        if (!cause.empty()) {
            log(RestLogLevel::warn, "restServerStopTimeout", cause);
            return false;
        }
        port.store(0);
        serverState.store(RestServerState::stopped);
        log(RestLogLevel::info, "restServerStopped", "");
        return true;
    }

    void requestStop() {
        std::lock_guard<std::mutex> lock(stopThreadMutex);
        if (destroying || stopThreadStarted.exchange(true)) {
            return;
        }
        if (stopThread.joinable()) {
            stopThread.join();
        }
        // Handlers and service callbacks may call this, and stop waits for those threads.
        stopThread = std::thread([this] {
            stopWithDeadline(10000);
            stopThreadStarted.store(false);
        });
    }

    std::string statusJson() {
        const RestMetricsSnapshot counters = metrics.snapshot();
        const char* requestedScheme = config.scheme == RestScheme::automatic ? "automatic" : (config.scheme == RestScheme::http ? "http" : "https");
        return std::string("{\"state\":") + jsonString(stateName(serverState.load())) +
               ",\"profile\":" + jsonString(config.profile == SecurityProfile::production ? "production" : "development") +
               ",\"requested\":{\"scheme\":" + jsonString(requestedScheme) + ",\"host\":" + jsonString(config.host) + ",\"port\":" + std::to_string(config.port) + "}" +
               ",\"effective\":{\"scheme\":" + jsonString(effectiveScheme) + ",\"host\":" + jsonString(config.host) + ",\"port\":" + std::to_string(port.load()) + "}" +
               ",\"tlsBackend\":" + jsonString(transport.tlsBackend()) +
               ",\"limits\":{\"maxThreads\":" + std::to_string(config.limits.maxThreads) + ",\"maxQueuedConnections\":" + std::to_string(config.limits.maxQueuedConnections) + ",\"maxBodyBytes\":" + std::to_string(config.limits.maxBodyBytes) + "}" +
               ",\"counters\":{\"requests\":" + std::to_string(counters.requests) + ",\"authenticationFailures\":" + std::to_string(counters.authenticationFailures) + ",\"dispatcherFull\":" + std::to_string(counters.dispatcherFull) + ",\"dispatcherDeadlines\":" + std::to_string(counters.dispatcherDeadlines) + ",\"handlerExceptions\":" + std::to_string(counters.handlerExceptions) + ",\"resourceExhaustion\":" + std::to_string(counters.resourceExhaustion) + ",\"transportErrors\":" + std::to_string(counters.transportErrors) +
               ",\"rejectedStreams\":" + std::to_string(counters.rejectedStreams) + ",\"rejectedTransfers\":" + std::to_string(counters.rejectedTransfers) +
               ",\"droppedChunks\":" + std::to_string(counters.droppedChunks) + ",\"refusedChunks\":" + std::to_string(counters.refusedChunks) +
               ",\"evictedSubscribers\":" + std::to_string(counters.evictedSubscribers) + "}" +
               ",\"streams\":{\"declared\":" + std::to_string(hub->descriptors().size()) + ",\"subscribers\":" + std::to_string(capacity.activeStreams()) + ",\"maxStreams\":" + std::to_string(config.limits.maxStreams) + "}" +
               ",\"transfers\":{\"active\":" + std::to_string(capacity.activeTransfers()) + ",\"pending\":" + std::to_string(copy ? copy->pendingTransfers() : 0) + ",\"maxTransfers\":" + std::to_string(config.limits.maxTransfers) + "}" +
               ",\"services\":" + services.statusJson() +
               ",\"detail\":" + safeProviderValue(statusDetailProvider) + "}";
    }

    std::string capabilitiesJson() {
        std::string routes = "[\"health\",\"heartbeat\",\"status\",\"getIdentity\",\"getCapabilities\"";
        if (dependencyReportProvider) {
            routes += ",\"checkDeps\"";
        }
        if (!services.isEmpty() || remoteStopEnabled) {
            routes += ",\"services\"";
        }
        bool sseLane = false;
        bool binaryLane = false;
        for (const RestStreamDescriptor& descriptor : hub->descriptors()) {
            (descriptor.lane == RestStreamLane::sse ? sseLane : binaryLane) = true;
        }
        if (sseLane || binaryLane) {
            routes += ",\"streams\"";
        }
        if (store) {
            routes += ",\"copyList\",\"copyCreate\",\"copyUpload\",\"copyDownload\",\"copyStatus\",\"copyCancel\"";
        }
        for (const std::string& name : diagnosticsRouteNames()) {
            routes += ",\"" + name + "\"";
        }
        std::string lanes = "[";
        if (sseLane) {
            lanes += "\"sse\"";
        }
        if (binaryLane) {
            lanes += std::string(sseLane ? "," : "") + "\"binary\"";
        }
        return "{\"apiVersion\":\"core/v1\",\"routes\":" + routes + "],\"streamLanes\":" + lanes + "],\"tlsBackend\":" + jsonString(transport.tlsBackend()) +
               ",\"limits\":{\"maxThreads\":" + std::to_string(config.limits.maxThreads) + ",\"maxBodyBytes\":" + std::to_string(config.limits.maxBodyBytes) +
               ",\"maxStreams\":" + std::to_string(config.limits.maxStreams) + ",\"maxTransfers\":" + std::to_string(config.limits.maxTransfers) +
               ",\"maxTransferBytes\":" + std::to_string(config.limits.maxTransferBytes) + "}" +
               ",\"hostFacts\":" + safeProviderValue(hostFactsProvider) + "}";
    }

    RestServerConfig config;
    RestMetrics metrics;
    CppHttplibTransport transport;
    Router router;
    MiddlewareChain middleware;
    ServiceRegistry services;
    CommonEndpoints common;
    std::shared_ptr<DispatchGate> gate;
    CapacityGate capacity;
    std::shared_ptr<StreamHub> hub;
    std::shared_ptr<RestResourceStore> store;
    std::unique_ptr<CopyService> copy;
    DiagnosticsSources diagnosticsSources;
    std::unique_ptr<DiagnosticsEndpoints> diagnostics;
    std::shared_ptr<RestDispatcher> dispatcher;
    RestAuthenticator authenticator;
    std::function<std::string()> statusDetailProvider;
    std::function<std::string()> hostFactsProvider;
    std::function<std::string()> dependencyReportProvider;
    bool remoteStopEnabled = false;
    std::atomic<RestServerState> serverState{RestServerState::created};
    std::atomic<int> port{0};
    mutable std::mutex causeMutex;
    std::string stopCause;
    std::mutex lifecycleMutex;
    bool routesRegistered = false;

private:
    CommonEndpointSources makeSources() {
        CommonEndpointSources sources;
        sources.buildIdentity = config.buildIdentity;
        sources.uptimeMilliseconds = [this] {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startedAt).count();
        };
        sources.instanceId = [this] { return instanceId; };
        sources.finishResponse = [this](TransportResponse& response, const TransportRequest& request) {
            if (response.headers.count("X-Request-Id") == 0) {
                addStandardHeaders(response, request, requestIdFor(request));
            }
        };
        sources.dependencyReportMissing = [this](const TransportRequest& request) {
            return errorResponse(404, "notProvided", "This server has no dependency report.", false, request);
        };
        sources.statusJson = [this] { return statusJson(); };
        sources.capabilitiesJson = [this] { return capabilitiesJson(); };
        sources.dependencyReportJson = [this] { return safeProviderValue(dependencyReportProvider); };
        sources.hasDependencyReport = [this] { return static_cast<bool>(dependencyReportProvider); };
        return sources;
    }

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

    std::string requestIdFor(const TransportRequest& request) {
        const std::string provided = safeRequestHeaderValue(request, "X-Request-Id");
        return provided.empty() ? instanceId + "-" + std::to_string(nextRequestNumber.fetch_add(1)) : provided;
    }

    void addStandardHeaders(TransportResponse& response, const TransportRequest& request, const std::string& requestId) const {
        response.headers.emplace("X-Request-Id", requestId);
        const std::string correlation = safeRequestHeaderValue(request, "X-Correlation-Id");
        if (!correlation.empty()) {
            response.headers.emplace("X-Correlation-Id", correlation);
        }
        if (!config.corsAllowOrigin.empty()) {
            response.headers.emplace("Access-Control-Allow-Origin", config.corsAllowOrigin);
        }
    }

    TransportResponse errorResponse(int status, const std::string& code, const std::string& message, bool retryable, const TransportRequest& request) {
        const std::string requestId = requestIdFor(request);
        TransportResponse response;
        response.status = status;
        response.body = jsonError(code, message, requestId, retryable);
        addStandardHeaders(response, request, requestId);
        return response;
    }

    TransportResponse toTransportResponse(RestResponse restResponse, const TransportRequest& request, const std::string& requestId) const {
        TransportResponse response;
        response.status = restResponse.status;
        response.contentType = std::move(restResponse.contentType);
        response.headers = std::move(restResponse.headers);
        if (restResponse.errorCode.empty()) {
            response.body = std::move(restResponse.body);
        } else {
            response.contentType = "application/json";
            response.body = jsonError(restResponse.errorCode, restResponse.errorMessage, requestId, restResponse.retryable);
        }
        addStandardHeaders(response, request, requestId);
        return response;
    }

    RestResponse invoke(const RestHandler& handler, const RestRequest& request) {
        try {
            return handler(request);
        } catch (const std::exception&) {
            metrics.handlerExceptions.fetch_add(1);
        } catch (...) {
            metrics.handlerExceptions.fetch_add(1);
        }
        log(RestLogLevel::error, "restHandlerException", request.requestId);
        return RestResponse::error(500, "internalError", "The server could not complete the request.");
    }

    RouteHandler wrap(RestHandler restHandler) {
        auto handler = std::make_shared<RestHandler>(std::move(restHandler));
        return [this, handler](const TransportRequest& request) {
            const std::string requestId = requestIdFor(request);
            auto restRequest = std::make_shared<RestRequest>(toRestRequest(request, requestId));
            const std::shared_ptr<RestDispatcher> activeDispatcher = dispatcher;
            RestResponse response = activeDispatcher
                ? gate->run(*activeDispatcher, [this, handler, restRequest] { return invoke(*handler, *restRequest); }, config.limits.dispatcherDeadlineMilliseconds, metrics)
                : invoke(*handler, *restRequest);
            return toTransportResponse(std::move(response), request, requestId);
        };
    }

    void registerEngineRoutes() {
        middleware.setAuthenticator(authenticator);
        middleware.setCorsAllowOrigin(config.corsAllowOrigin);
        middleware.setDefaultBodyLimit(config.limits.maxBodyBytes);
        transport.setPreRouteFilter([this](const TransportRequest& request) { return middleware.evaluate(request); });
        transport.setErrorResponder([this](int status, const TransportRequest& request) {
            return errorResponse(status, errorCodeFor(status), status >= 500 ? "The server could not complete the request." : "The request was rejected.", false, request);
        });
        common.registerRoutes(router);
        RouteInfo protectedRoute;
        protectedRoute.authRequired = true;
        registerStreamRoutes(protectedRoute);
        const auto errorBuilder = [this](int status, const std::string& code, const std::string& message, bool retryable, const TransportRequest& request) {
            return errorResponse(status, code, message, retryable, request);
        };
        const auto finisher = [this](TransportResponse& response, const TransportRequest& request) {
            addStandardHeaders(response, request, requestIdFor(request));
        };
        const auto serviceLog = [this](RestLogLevel level, const std::string& event, const std::string& detail) { log(level, event, detail); };
        if (store) {
            CopyServiceConfig copyConfig;
            copyConfig.maxTransferBytes = config.limits.maxTransferBytes;
            copyConfig.uploadBodyDeadlineMilliseconds = config.limits.uploadBodyDeadlineMilliseconds;
            copyConfig.resumeWindowMilliseconds = config.limits.transferResumeWindowMilliseconds;
            copyConfig.maxDownloadBytesPerSecond = config.limits.maxDownloadBytesPerSecond;
            copyConfig.maxPendingTransfers = std::max<std::size_t>(1, config.limits.maxTransfers) * 4;
            copy = std::make_unique<CopyService>(store, capacity, copyConfig, errorBuilder, finisher, serviceLog);
            copy->registerRoutes(router);
        }
        diagnostics = std::make_unique<DiagnosticsEndpoints>(diagnosticsSources, errorBuilder, finisher, serviceLog);
        diagnostics->registerRoutes(router);
        if (services.isEmpty() && !remoteStopEnabled) {
            return;
        }
        router.addCommon(HttpMethod::get, "/core/v1/services", [this](const TransportRequest& request) {
            const std::string requestId = requestIdFor(request);
            TransportResponse response;
            response.body = "{\"services\":" + services.statusJson() + "}";
            addStandardHeaders(response, request, requestId);
            return response;
        }, protectedRoute);
        const auto addAction = [this, &protectedRoute](const std::string& suffix, ServiceAction action) {
            router.addCommon(HttpMethod::post, "/core/v1/services/{name}/" + suffix, [this, action](const TransportRequest& request) {
                return serviceAction(request, action);
            }, protectedRoute);
        };
        addAction("start", ServiceAction::start);
        addAction("stop", ServiceAction::stop);
        addAction("restart", ServiceAction::restart);
    }

    std::vector<std::string> diagnosticsRouteNames() const {
        return diagnostics ? diagnostics->routeNames() : std::vector<std::string>();
    }

    void registerStreamRoutes(const RouteInfo& protectedRoute) {
        if (hub->descriptors().empty()) {
            return;
        }
        RouteInfo readRoute = protectedRoute;
        readRoute.idempotent = true;
        router.addCommon(HttpMethod::get, "/core/v1/streams", [this](const TransportRequest& request) {
            std::string body = "{\"streams\":[";
            bool first = true;
            for (const RestStreamDescriptor& descriptor : hub->descriptors()) {
                const char* policy = descriptor.policy == RestDeliveryPolicy::reliable ? "reliable" : (descriptor.policy == RestDeliveryPolicy::realtime ? "realtime" : "broadcast");
                body += std::string(first ? "" : ",") + "{\"id\":" + jsonString(descriptor.id) +
                        ",\"lane\":" + jsonString(descriptor.lane == RestStreamLane::sse ? "sse" : "binary") +
                        ",\"contentType\":" + jsonString(descriptor.lane == RestStreamLane::sse ? "text/event-stream" : (descriptor.contentType.empty() ? "application/octet-stream" : descriptor.contentType)) +
                        ",\"format\":" + jsonString(descriptor.format) + ",\"formatVersion\":" + jsonString(descriptor.formatVersion) +
                        ",\"policy\":" + jsonString(policy) + ",\"maxQueuedBytesPerSubscriber\":" + std::to_string(descriptor.maxQueuedBytesPerSubscriber) +
                        ",\"heartbeatMilliseconds\":" + std::to_string(descriptor.lane == RestStreamLane::sse ? descriptor.heartbeatMilliseconds : 0) + "}";
                first = false;
            }
            TransportResponse response;
            response.body = body + "]}";
            addStandardHeaders(response, request, requestIdFor(request));
            return response;
        }, readRoute);
        router.addCommon(HttpMethod::get, "/core/v1/streams/{id}/live", [this](const TransportRequest& request) { return subscribe(request); }, readRoute);
    }

    // Cheapest checks first, so a refused subscriber never holds capacity.
    TransportResponse subscribe(const TransportRequest& request) {
        const std::string id = request.pathParameters.at("id");
        switch (hub->check(id)) {
            case SubscribeResult::unknownStream:
                return errorResponse(404, "unknownStream", "No stream with that ID is declared.", false, request);
            case SubscribeResult::streamEnded:
                return errorResponse(410, "streamEnded", "The producer has closed this stream.", false, request);
            case SubscribeResult::stopping:
                return errorResponse(503, "serverStopping", "The server is stopping.", true, request);
            case SubscribeResult::subscribed:
                break;
        }
        std::optional<CapacityLease> lease = capacity.tryAcquireStream();
        if (!lease) {
            return errorResponse(503, "streamCapReached", "Too many streams are open. Try again later.", true, request);
        }
        std::string lastEventId = requestHeader(request, "Last-Event-ID").value_or("");
        if (lastEventId.size() > 32) {
            lastEventId = "invalid";
        }
        const std::shared_ptr<StreamSession> session = hub->subscribe(id, std::move(*lease), lastEventId);
        if (!session) {
            return errorResponse(503, "serverStopping", "The server is stopping.", true, request);
        }
        TransportResponse response = hub->makeResponse(id, session);
        response.release = [keep = hub, release = std::move(response.release)](bool success) { release(success); };
        addStandardHeaders(response, request, requestIdFor(request));
        return response;
    }

    TransportResponse serviceAction(const TransportRequest& request, ServiceAction action) {
        const std::string name = request.pathParameters.at("name");
        if (name == "server" && action == ServiceAction::stop && remoteStopEnabled) {
            TransportResponse accepted = serviceAccepted(request, name);
            accepted.release = [this](bool) { requestStop(); };
            return accepted;
        }
        switch (services.check(name, action)) {
            case ServiceSubmitResult::unknownService:
                return errorResponse(404, "serviceNotRegistered", "No service with that name is registered.", false, request);
            case ServiceSubmitResult::notOffered:
                return errorResponse(405, "actionNotOffered", "This service does not offer that action.", false, request);
            case ServiceSubmitResult::queueFull:
                return errorResponse(503, "serviceQueueFull", "Too many service actions are waiting. Try again.", true, request);
            case ServiceSubmitResult::closed:
                return errorResponse(503, "serverStopping", "The server is stopping.", true, request);
            case ServiceSubmitResult::accepted:
                break;
        }
        TransportResponse accepted = serviceAccepted(request, name);
        accepted.release = [this, name, action](bool) {
            if (services.submit(name, action) != ServiceSubmitResult::accepted) {
                log(RestLogLevel::warn, "restServiceActionDropped", name);
            }
        };
        return accepted;
    }

    TransportResponse serviceAccepted(const TransportRequest& request, const std::string& name) {
        const std::string requestId = requestIdFor(request);
        TransportResponse response;
        response.status = 202;
        response.body = "{\"accepted\":true,\"service\":" + jsonString(name) + ",\"requestId\":" + jsonString(requestId) + "}";
        addStandardHeaders(response, request, requestId);
        return response;
    }

    std::string instanceId;
    std::chrono::steady_clock::time_point startedAt;
    std::string effectiveScheme;
    std::atomic<std::uint64_t> nextRequestNumber{1};
    std::mutex stopThreadMutex;
    std::thread stopThread;
    std::atomic<bool> stopThreadStarted{false};
    bool destroying = false;
};

RestServer::RestServer(RestServerConfig config) : implementation(std::make_unique<Implementation>(std::move(config))) {}

RestServer::~RestServer() = default;

RestRouteResult RestServer::add(RestMethod method, const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return implementation->add(method, pattern, std::move(handler), options);
}

RestRouteResult RestServer::GET(const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return add(RestMethod::get, pattern, std::move(handler), options);
}

RestRouteResult RestServer::POST(const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return add(RestMethod::post, pattern, std::move(handler), options);
}

RestRouteResult RestServer::PUT(const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return add(RestMethod::put, pattern, std::move(handler), options);
}

RestRouteResult RestServer::PATCH(const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return add(RestMethod::patch, pattern, std::move(handler), options);
}

RestRouteResult RestServer::DELETE(const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return add(RestMethod::del, pattern, std::move(handler), options);
}

RestRouteResult RestServer::OPTIONS(const std::string& pattern, RestHandler handler, RestRouteOptions options) {
    return add(RestMethod::options, pattern, std::move(handler), options);
}

void RestServer::setAuthenticator(RestAuthenticator authenticator) {
    implementation->authenticator = std::move(authenticator);
}

void RestServer::setDispatcher(std::shared_ptr<RestDispatcher> dispatcher) {
    implementation->dispatcher = std::move(dispatcher);
}

void RestServer::addHealthCheck(const std::string& name, std::function<bool()> check) {
    implementation->common.addHealthCheck(name, std::move(check));
}

void RestServer::setStatusDetailProvider(std::function<std::string()> provider) {
    implementation->statusDetailProvider = std::move(provider);
}

void RestServer::setHostFactsProvider(std::function<std::string()> provider) {
    implementation->hostFactsProvider = std::move(provider);
}

void RestServer::setDependencyReportProvider(std::function<std::string()> provider) {
    implementation->dependencyReportProvider = std::move(provider);
}

bool RestServer::registerService(const std::string& name, RestServiceCallbacks callbacks) {
    return name != "server" && Router::isValidPathParameter(name) && implementation->services.add(name, std::move(callbacks));
}

void RestServer::enableRemoteStop() {
    implementation->remoteStopEnabled = true;
}

RestStartResult RestServer::start() {
    return implementation->start();
}

bool RestServer::stop(int timeoutMilliseconds) {
    return implementation->stopWithDeadline(timeoutMilliseconds);
}

void RestServer::requestStop() {
    implementation->requestStop();
}

RestServerState RestServer::state() const {
    return implementation->serverState.load();
}

int RestServer::boundPort() const {
    return implementation->port.load();
}

std::string RestServer::lastStopCause() const {
    std::lock_guard<std::mutex> lock(implementation->causeMutex);
    return implementation->stopCause;
}

std::shared_ptr<RestStream> RestServer::declareStream(RestStreamDescriptor descriptor) {
    std::lock_guard<std::mutex> lock(implementation->lifecycleMutex);
    if (implementation->routesRegistered || !implementation->hub->declare(descriptor)) {
        return nullptr;
    }
    return std::make_shared<RestStream>(implementation->hub, descriptor.id);
}

void RestServer::setResourceStore(std::shared_ptr<RestResourceStore> store) {
    std::lock_guard<std::mutex> lock(implementation->lifecycleMutex);
    if (!implementation->routesRegistered) {
        implementation->store = std::move(store);
    }
}

void RestServer::enableLogQuery(std::vector<std::string> roots, RestLogQueryLimits limits) {
    std::lock_guard<std::mutex> lock(implementation->lifecycleMutex);
    if (!implementation->routesRegistered) {
        implementation->diagnosticsSources.logRoots = std::move(roots);
        implementation->diagnosticsSources.limits = limits;
    }
}

void RestServer::setCommandStatusProvider(std::function<std::vector<RestCommandRecord>()> provider) {
    std::lock_guard<std::mutex> lock(implementation->lifecycleMutex);
    if (!implementation->routesRegistered) {
        implementation->diagnosticsSources.commandStatus = std::move(provider);
    }
}

void RestServer::setLogMarkerWriter(std::function<bool(const RestLogMarker&)> writer) {
    std::lock_guard<std::mutex> lock(implementation->lifecycleMutex);
    if (!implementation->routesRegistered) {
        implementation->diagnosticsSources.markerWriter = std::move(writer);
    }
}

RestServerCounters RestServer::counters() const {
    const RestMetricsSnapshot snapshot = implementation->metrics.snapshot();
    RestServerCounters result;
    result.requests = snapshot.requests;
    result.authenticationFailures = snapshot.authenticationFailures;
    result.dispatcherFull = snapshot.dispatcherFull;
    result.dispatcherDeadlines = snapshot.dispatcherDeadlines;
    result.handlerExceptions = snapshot.handlerExceptions;
    result.resourceExhaustion = snapshot.resourceExhaustion;
    result.transportErrors = snapshot.transportErrors;
    result.rejectedStreams = snapshot.rejectedStreams;
    result.rejectedTransfers = snapshot.rejectedTransfers;
    result.droppedChunks = snapshot.droppedChunks;
    result.refusedChunks = snapshot.refusedChunks;
    result.evictedSubscribers = snapshot.evictedSubscribers;
    return result;
}

} // namespace gigante
