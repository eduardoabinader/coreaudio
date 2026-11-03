#pragma once

#include "Router.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace gigante {

struct CommonEndpointSources {
    std::string buildIdentity;
    std::function<std::string()> instanceId;
    std::function<void(TransportResponse&, const TransportRequest&)> finishResponse;
    std::function<TransportResponse(const TransportRequest&)> dependencyReportMissing;
    std::function<std::int64_t()> uptimeMilliseconds;
    std::function<std::string()> statusJson;
    std::function<std::string()> capabilitiesJson;
    std::function<std::string()> dependencyReportJson;
    std::function<bool()> hasDependencyReport;
};

// The always-on routes. health and heartbeat answer without authentication.
class CommonEndpoints final {
public:
    explicit CommonEndpoints(CommonEndpointSources sources);

    void addHealthCheck(const std::string& name, std::function<bool()> check);
    void registerRoutes(Router& router);

private:
    TransportResponse health() const;

    CommonEndpointSources sources;
    mutable std::mutex mutex;
    std::map<std::string, std::function<bool()>> healthChecks;
};

} // namespace gigante
