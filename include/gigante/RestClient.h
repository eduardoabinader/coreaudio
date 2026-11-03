#pragma once

#include "gigante/RestTypes.h"

#include <functional>
#include <memory>
#include <string>

namespace gigante {

// Calls are safe from any non-realtime thread. A refused configuration does not
// throw: configurationError() explains it, and every call returns refusedByPolicy.
class RestClient final {
public:
    explicit RestClient(RestClientConfig config);

    // Cancels running calls and joins them. Queued asynchronous calls are dropped
    // without a callback, and no callback runs after it returns.
    ~RestClient();

    RestClient(const RestClient&) = delete;
    RestClient& operator=(const RestClient&) = delete;

    std::string configurationError() const;

    RestResult call(RestMethod method, const std::string& path, const std::string& body = "", const std::string& contentType = "application/json", RestCallOptions options = {});

    // Returns false when the bounded queue is full. The callback runs on a client thread.
    bool callAsync(RestMethod method, const std::string& path, std::function<void(RestResult)> callback, const std::string& body = "", const std::string& contentType = "application/json", RestCallOptions options = {});

    RestResult health();
    RestResult heartbeat();
    RestResult status();
    RestResult getBuildVersion();
    RestResult getCapabilities();
    RestResult checkDeps();
    RestResult startService(const std::string& name);
    RestResult stopService(const std::string& name);
    RestResult restartService(const std::string& name);

    // Ends calls in progress, from any thread. Later calls are unaffected.
    void cancel();

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
};

} // namespace gigante
