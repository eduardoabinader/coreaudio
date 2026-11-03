#pragma once

#include "gigante/RestTypes.h"

#include <functional>
#include <memory>
#include <string>

namespace gigante {

// Register routes and providers before start(). Not for use on an audio thread.
// Anything a handler or callback captures must outlive this object.
class RestServer final {
public:
    explicit RestServer(RestServerConfig config);

    // Blocks until running callbacks end, so no callback runs after it returns.
    // Must not run on a handler, service or dispatcher thread.
    ~RestServer();

    RestServer(const RestServer&) = delete;
    RestServer& operator=(const RestServer&) = delete;

    // Patterns use {name} parameters. /core/v1/ is reserved for common endpoints.
    RestRouteResult add(RestMethod method, const std::string& pattern, RestHandler handler, RestRouteOptions options = {});
    RestRouteResult GET(const std::string& pattern, RestHandler handler, RestRouteOptions options = {});
    RestRouteResult POST(const std::string& pattern, RestHandler handler, RestRouteOptions options = {});
    RestRouteResult PUT(const std::string& pattern, RestHandler handler, RestRouteOptions options = {});
    RestRouteResult PATCH(const std::string& pattern, RestHandler handler, RestRouteOptions options = {});
    RestRouteResult DELETE(const std::string& pattern, RestHandler handler, RestRouteOptions options = {});
    RestRouteResult OPTIONS(const std::string& pattern, RestHandler handler, RestRouteOptions options = {});

    // Production start() fails without an authenticator.
    void setAuthenticator(RestAuthenticator authenticator);
    void setDispatcher(std::shared_ptr<RestDispatcher> dispatcher);

    void addHealthCheck(const std::string& name, std::function<bool()> check);

    // Each provider returns a JSON value, included in the matching common endpoint.
    void setStatusDetailProvider(std::function<std::string()> provider);
    void setHostFactsProvider(std::function<std::string()> provider);
    void setDependencyReportProvider(std::function<std::string()> provider);

    // Opt-in: adds /core/v1/services routes for logical services the project owns.
    bool registerService(const std::string& name, RestServiceCallbacks callbacks);

    // Opt-in: adds POST /core/v1/services/server/stop, which drains after replying.
    void enableRemoteStop();

    RestStartResult start();

    // Returns false when work is still running after the timeout; lastStopCause() says what.
    // Must not run on a handler thread: use requestStop() there.
    bool stop(int timeoutMilliseconds = 10000);
    void requestStop();

    RestServerState state() const;
    int boundPort() const;
    std::string lastStopCause() const;
    RestServerCounters counters() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
};

} // namespace gigante
