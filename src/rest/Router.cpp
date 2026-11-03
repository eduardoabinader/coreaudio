#include "Router.h"

#include <cctype>

namespace gigante {

namespace {

const char* const reservedPrefix = "/core/v1/";

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::size_t start = 1;
    while (start <= path.size()) {
        const std::size_t end = path.find('/', start);
        segments.push_back(path.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return segments;
}

bool segmentsMatch(const std::vector<std::string>& pattern, const std::vector<std::string>& path) {
    if (pattern.size() != path.size()) {
        return false;
    }
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const bool isParameter = !pattern[index].empty() && pattern[index].front() == '{';
        if (isParameter ? path[index].empty() : pattern[index] != path[index]) {
            return false;
        }
    }
    return true;
}

} // namespace

Router::Router(HttpTransport& httpTransport) : transport(httpTransport) {}

bool Router::isValidPattern(const std::string& pattern) {
    if (pattern.empty() || pattern.front() != '/') {
        return false;
    }
    std::size_t index = 0;
    while (index < pattern.size()) {
        const char current = pattern[index];
        if (current == '}' || current == ':' || current == '*' || current == '(' || current == '\\') {
            return false;
        }
        if (current == '{') {
            const std::size_t end = pattern.find('}', index);
            if (end == std::string::npos || end == index + 1 || pattern[index - 1] != '/') {
                return false;
            }
            for (std::size_t nameIndex = index + 1; nameIndex < end; ++nameIndex) {
                if (std::isalnum(static_cast<unsigned char>(pattern[nameIndex])) == 0) {
                    return false;
                }
            }
            if (end + 1 < pattern.size() && pattern[end + 1] != '/') {
                return false;
            }
            index = end;
        }
        ++index;
    }
    return true;
}

bool Router::isValidPathParameter(const std::string& value) {
    if (value.empty() || value == "." || value == "..") {
        return false;
    }
    for (const char current : value) {
        const auto character = static_cast<unsigned char>(current);
        const bool allowed = std::isalnum(character) != 0 || current == '-' || current == '_' || current == '.' || current == '~';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

RouteAddResult Router::check(HttpMethod method, const std::string& pattern, bool allowReserved) {
    if (!isValidPattern(pattern)) {
        return RouteAddResult::invalidPattern;
    }
    if (!allowReserved && pattern.rfind(reservedPrefix, 0) == 0) {
        return RouteAddResult::reservedPrefix;
    }
    if (!registered.emplace(static_cast<int>(method), pattern).second) {
        return RouteAddResult::duplicate;
    }
    return RouteAddResult::added;
}

bool Router::findInvalidParameter(const TransportRequest& request, std::string& name) const {
    for (const auto& parameter : request.pathParameters) {
        if (!isValidPathParameter(parameter.second)) {
            name = parameter.first;
            return true;
        }
    }
    return false;
}

TransportResponse Router::invalidParameter(const std::string& name, const TransportRequest& request) const {
    if (invalidParameterResponder) {
        return invalidParameterResponder(name, request);
    }
    TransportResponse response;
    response.status = 400;
    response.body = "{\"code\":\"invalidPathParameter\",\"parameter\":\"" + name + "\",\"retryable\":false}";
    return response;
}

RouteAddResult Router::add(HttpMethod method, const std::string& pattern, RouteHandler handler, RouteInfo info) {
    const RouteAddResult result = check(method, pattern, false);
    if (result == RouteAddResult::added) {
        entries.push_back({method, splitPath(pattern), info});
        transport.addRoute(method, pattern, [this, routeHandler = std::move(handler)](const TransportRequest& request) {
            std::string name;
            return findInvalidParameter(request, name) ? invalidParameter(name, request) : routeHandler(request);
        });
    }
    return result;
}

RouteAddResult Router::addCommon(HttpMethod method, const std::string& pattern, RouteHandler handler, RouteInfo info) {
    const RouteAddResult result = check(method, pattern, true);
    if (result == RouteAddResult::added) {
        entries.push_back({method, splitPath(pattern), info});
        transport.addRoute(method, pattern, [this, routeHandler = std::move(handler)](const TransportRequest& request) {
            std::string name;
            return findInvalidParameter(request, name) ? invalidParameter(name, request) : routeHandler(request);
        });
    }
    return result;
}

RouteAddResult Router::addUpload(HttpMethod method, const std::string& pattern, UploadHandler handler, RouteInfo info) {
    const RouteAddResult result = check(method, pattern, false);
    if (result == RouteAddResult::added) {
        entries.push_back({method, splitPath(pattern), info});
        transport.addUploadRoute(
            method,
            pattern,
            [this, routeHandler = std::move(handler)](const TransportRequest& request, const BodyChunkReader& reader) {
                std::string name;
                return findInvalidParameter(request, name) ? invalidParameter(name, request) : routeHandler(request, reader);
            });
    }
    return result;
}

RouteLookup Router::lookup(HttpMethod method, const std::string& path, RouteInfo& info) const {
    if (path.empty() || path.front() != '/') {
        return RouteLookup::unknownPath;
    }
    const std::vector<std::string> segments = splitPath(path);
    RouteLookup result = RouteLookup::unknownPath;
    for (const Entry& entry : entries) {
        if (!segmentsMatch(entry.segments, segments)) {
            continue;
        }
        if (entry.method == method) {
            info = entry.info;
            return RouteLookup::found;
        }
        result = RouteLookup::wrongMethod;
    }
    return result;
}

void Router::setInvalidParameterResponder(InvalidParameterResponder responder) {
    invalidParameterResponder = std::move(responder);
}

} // namespace gigante
