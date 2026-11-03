#include "MiddlewareChain.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <utility>

namespace gigante {

namespace {

RestMethod toRestMethod(HttpMethod method) {
    switch (method) {
        case HttpMethod::get: return RestMethod::get;
        case HttpMethod::post: return RestMethod::post;
        case HttpMethod::put: return RestMethod::put;
        case HttpMethod::patch: return RestMethod::patch;
        case HttpMethod::del: return RestMethod::del;
        case HttpMethod::options: return RestMethod::options;
    }
    return RestMethod::get;
}

bool isSafeIdentifier(const std::string& value) {
    if (value.empty() || value.size() > 64) {
        return false;
    }
    for (const char current : value) {
        if (std::isalnum(static_cast<unsigned char>(current)) == 0 && current != '-' && current != '_') {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<std::string> requestHeader(const TransportRequest& request, const std::string& name) {
    for (const auto& entry : request.headers) {
        if (entry.first.size() == name.size() &&
            std::equal(entry.first.begin(), entry.first.end(), name.begin(), [](char left, char right) {
                return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
            })) {
            return entry.second;
        }
    }
    return std::nullopt;
}

std::string safeRequestHeaderValue(const TransportRequest& request, const std::string& name) {
    const auto entry = request.headers.find(name);
    return entry != request.headers.end() && isSafeIdentifier(entry->second) ? entry->second : std::string();
}

RestRequest toRestRequest(const TransportRequest& request, const std::string& requestId) {
    RestRequest result;
    result.method = toRestMethod(request.method);
    result.path = request.path;
    result.pathParameters = request.pathParameters;
    result.query = request.query;
    result.headers = request.headers;
    result.body = request.body;
    result.requestId = requestId;
    result.correlationId = safeRequestHeaderValue(request, "X-Correlation-Id");
    return result;
}

MiddlewareChain::MiddlewareChain(Router& routeTable, RestMetrics& restMetrics, MiddlewareErrorBuilder builder)
    : router(routeTable), metrics(restMetrics), errorBuilder(std::move(builder)) {}

void MiddlewareChain::setAuthenticator(RestAuthenticator restAuthenticator) {
    authenticator = std::move(restAuthenticator);
}

void MiddlewareChain::setCorsAllowOrigin(std::string origin) {
    corsAllowOrigin = std::move(origin);
}

void MiddlewareChain::setDefaultBodyLimit(std::size_t bytes) {
    defaultBodyLimit = bytes;
}

void MiddlewareChain::setTransportBodyLimit(std::size_t bytes) {
    transportBodyLimit = bytes;
}

std::optional<TransportResponse> MiddlewareChain::evaluate(const TransportRequest& request) const {
    if (!corsAllowOrigin.empty() && request.method == HttpMethod::options && request.headers.count("Access-Control-Request-Method") != 0) {
        TransportResponse preflight;
        preflight.status = 204;
        preflight.headers.emplace("Access-Control-Allow-Origin", corsAllowOrigin);
        preflight.headers.emplace("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        preflight.headers.emplace("Access-Control-Allow-Headers", "Authorization, Content-Type, X-Request-Id, X-Correlation-Id");
        return preflight;
    }

    RouteInfo info;
    const RouteLookup lookup = router.lookup(request.method, request.path, info);
    if (lookup == RouteLookup::unknownPath) {
        return std::nullopt;
    }
    if (lookup == RouteLookup::wrongMethod) {
        return errorBuilder(405, "methodNotAllowed", "This route does not accept that method.", false, request);
    }

    const std::size_t limit = info.maxBodyBytes != 0 ? info.maxBodyBytes : defaultBodyLimit;
    const auto length = request.headers.find("Content-Length");
    if (limit != 0 && length != request.headers.end()) {
        const unsigned long long declared = std::strtoull(length->second.c_str(), nullptr, 10);
        if (declared > limit) {
            return errorBuilder(413, "bodyTooLarge", "The request body is over this route's limit.", false, request);
        }
    }
    if (limit != 0 && limit < transportBodyLimit && length == request.headers.end() && request.headers.count("Transfer-Encoding") != 0) {
        return errorBuilder(411, "lengthRequired", "This route needs a Content-Length header.", false, request);
    }

    if (info.authRequired && authenticator) {
        bool allowed = false;
        try {
            allowed = authenticator(toRestRequest(request, safeRequestHeaderValue(request, "X-Request-Id")));
        } catch (const std::exception&) {
            metrics.handlerExceptions.fetch_add(1);
        } catch (...) {
            metrics.handlerExceptions.fetch_add(1);
        }
        if (!allowed) {
            metrics.authenticationFailures.fetch_add(1);
            return errorBuilder(401, "unauthenticated", "Valid credentials are required.", false, request);
        }
    }
    return std::nullopt;
}

} // namespace gigante
