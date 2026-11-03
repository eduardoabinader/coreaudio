#pragma once

#include "HttpTransport.h"

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace gigante {

enum class RouteAddResult { added, reservedPrefix, duplicate, invalidPattern };

struct RouteInfo {
    bool authRequired = false;
    std::size_t maxBodyBytes = 0;
    bool idempotent = false;
};

enum class RouteLookup { unknownPath, wrongMethod, found };

using InvalidParameterResponder = std::function<TransportResponse(const std::string& name, const TransportRequest& request)>;

class Router {
public:
    explicit Router(HttpTransport& transport);

    RouteAddResult add(HttpMethod method, const std::string& pattern, RouteHandler handler, RouteInfo info = {});
    RouteAddResult addUpload(HttpMethod method, const std::string& pattern, UploadHandler handler, RouteInfo info = {});

    // Common endpoints are the only routes allowed under the reserved prefix.
    RouteAddResult addCommon(HttpMethod method, const std::string& pattern, RouteHandler handler, RouteInfo info = {});
    RouteAddResult addCommonUpload(HttpMethod method, const std::string& pattern, UploadHandler handler, RouteInfo info = {});

    // Used before the body is read, so limits and authentication can apply per route.
    RouteLookup lookup(HttpMethod method, const std::string& path, RouteInfo& info) const;

    void setInvalidParameterResponder(InvalidParameterResponder responder);

    static bool isValidPattern(const std::string& pattern);
    static bool isValidPathParameter(const std::string& value);

private:
    struct Entry {
        HttpMethod method;
        std::vector<std::string> segments;
        RouteInfo info;
    };

    RouteAddResult check(HttpMethod method, const std::string& pattern, bool allowReserved);
    RouteAddResult addUploadRoute(HttpMethod method, const std::string& pattern, UploadHandler handler, RouteInfo info, bool allowReserved);
    TransportResponse invalidParameter(const std::string& name, const TransportRequest& request) const;
    bool findInvalidParameter(const TransportRequest& request, std::string& name) const;

    HttpTransport& transport;
    std::set<std::pair<int, std::string>> registered;
    std::vector<Entry> entries;
    InvalidParameterResponder invalidParameterResponder;
};

} // namespace gigante
