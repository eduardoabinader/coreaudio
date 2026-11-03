#pragma once

#include "CopyService.h"
#include "MiddlewareChain.h"
#include "Router.h"

#include "gigante/RestTypes.h"

#include <functional>
#include <string>
#include <vector>

namespace gigante {

struct DiagnosticsSources {
    std::vector<std::string> logRoots;
    RestLogQueryLimits limits;
    std::function<std::vector<RestCommandRecord>()> commandStatus;
    std::function<bool(const RestLogMarker&)> markerWriter;
};

// getLog, cmdStatus and appendLogMarker. Each route exists only when the project
// supplied its source. getLog never opens a name a client sends: it lists files under
// the registered roots and matches their names against a glob.
class DiagnosticsEndpoints final {
public:
    DiagnosticsEndpoints(DiagnosticsSources sources, MiddlewareErrorBuilder errorBuilder, ResponseFinisher finisher, ServiceLog log);

    void registerRoutes(Router& router);
    std::vector<std::string> routeNames() const;

    // '*' and '?' only, case-sensitive.
    static bool globMatch(const std::string& pattern, const std::string& text);

private:
    TransportResponse getLog(const TransportRequest& request);
    TransportResponse cmdStatus(const TransportRequest& request);
    TransportResponse appendLogMarker(const TransportRequest& request);
    TransportResponse json(const TransportRequest& request, std::string body);

    DiagnosticsSources sources;
    MiddlewareErrorBuilder errorBuilder;
    ResponseFinisher finisher;
    ServiceLog log;
};

} // namespace gigante
