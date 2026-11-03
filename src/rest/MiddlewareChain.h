#pragma once

#include "HttpTransport.h"
#include "Router.h"
#include "RestMetrics.h"

#include "gigante/RestTypes.h"

#include <functional>
#include <optional>
#include <string>

namespace gigante {

using MiddlewareErrorBuilder = std::function<TransportResponse(int status, const std::string& code, const std::string& message, bool retryable, const TransportRequest& request)>;

// Runs before the request body is read. Stops a request with a response, or lets it continue.
class MiddlewareChain final {
public:
    MiddlewareChain(Router& router, RestMetrics& metrics, MiddlewareErrorBuilder errorBuilder);

    void setAuthenticator(RestAuthenticator authenticator);
    void setCorsAllowOrigin(std::string origin);
    void setDefaultBodyLimit(std::size_t bytes);

    std::optional<TransportResponse> evaluate(const TransportRequest& request) const;

private:
    Router& router;
    RestMetrics& metrics;
    MiddlewareErrorBuilder errorBuilder;
    RestAuthenticator authenticator;
    std::string corsAllowOrigin;
    std::size_t defaultBodyLimit = 0;
};

// Converts a transport request, assigning its identifiers.
RestRequest toRestRequest(const TransportRequest& request, const std::string& requestId);

// Accepts a client's X-Request-Id only when it is safe to echo.
std::string safeRequestHeaderValue(const TransportRequest& request, const std::string& name);

} // namespace gigante
