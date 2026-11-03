#include "CommonEndpoints.h"

#include "RestJson.h"

#include <exception>
#include <utility>

namespace gigante {

CommonEndpoints::CommonEndpoints(CommonEndpointSources endpointSources) : sources(std::move(endpointSources)) {}

void CommonEndpoints::addHealthCheck(const std::string& name, std::function<bool()> check) {
    std::lock_guard<std::mutex> lock(mutex);
    healthChecks[name] = std::move(check);
}

TransportResponse CommonEndpoints::health() const {
    std::map<std::string, std::function<bool()>> checks;
    {
        std::lock_guard<std::mutex> lock(mutex);
        checks = healthChecks;
    }
    bool healthy = true;
    std::string details = "[";
    for (const auto& check : checks) {
        bool passed = false;
        try {
            passed = check.second();
        } catch (const std::exception&) {
            passed = false;
        } catch (...) {
            passed = false;
        }
        healthy = healthy && passed;
        if (details.size() > 1) {
            details += ",";
        }
        details += "{\"name\":" + jsonString(check.first) + ",\"ok\":" + (passed ? "true" : "false") + "}";
    }
    TransportResponse response;
    response.status = healthy ? 200 : 503;
    response.body = std::string("{\"status\":") + (healthy ? "\"ok\"" : "\"unavailable\"") + ",\"checks\":" + details + "]}";
    return response;
}

void CommonEndpoints::registerRoutes(Router& router) {
    const auto addRoute = [this, &router](const std::string& name, bool authRequired, std::function<TransportResponse(const TransportRequest&)> produce) {
        RouteInfo info;
        info.authRequired = authRequired;
        info.idempotent = true;
        router.addCommon(HttpMethod::get, "/core/v1/" + name, [this, work = std::move(produce)](const TransportRequest& request) {
            TransportResponse response = work(request);
            sources.finishResponse(response, request);
            return response;
        }, info);
    };
    addRoute("health", false, [this](const TransportRequest&) { return health(); });
    addRoute("heartbeat", false, [this](const TransportRequest&) {
        TransportResponse response;
        response.body = "{\"status\":\"ok\",\"uptimeMilliseconds\":" + std::to_string(sources.uptimeMilliseconds()) +
                        ",\"instanceId\":" + jsonString(sources.instanceId()) + "}";
        return response;
    });
    addRoute("status", true, [this](const TransportRequest&) {
        TransportResponse response;
        response.body = sources.statusJson();
        return response;
    });
    addRoute("getBuildVersion", true, [this](const TransportRequest&) {
        TransportResponse response;
        response.body = "{\"apiVersion\":\"core/v1\",\"buildIdentity\":" + jsonString(sources.buildIdentity) + "}";
        return response;
    });
    addRoute("getCapabilities", true, [this](const TransportRequest&) {
        TransportResponse response;
        response.body = sources.capabilitiesJson();
        return response;
    });
    addRoute("checkDeps", true, [this](const TransportRequest& request) {
        if (!sources.hasDependencyReport()) {
            return sources.dependencyReportMissing(request);
        }
        TransportResponse response;
        response.body = sources.dependencyReportJson();
        return response;
    });
}

} // namespace gigante
