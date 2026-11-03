#pragma once

#include "gigante/RestSubscription.h"
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
    RestResult getIdentity();
    RestResult getCapabilities();
    RestResult checkDeps();
    RestResult startService(const std::string& name);
    RestResult stopService(const std::string& name);
    RestResult restartService(const std::string& name);

    // Callbacks run on the subscription's own thread. onClosed runs exactly once.
    std::unique_ptr<RestSubscription> subscribe(
        const std::string& streamId,
        std::function<void(const RestStreamEvent&)> onEvent,
        std::function<void(RestSubscriptionEnd, const std::string& detail)> onClosed,
        RestSubscriptionOptions options = {});

    RestResult listResources();
    // Resumes from the server's received offset after a lost connection.
    RestResult upload(const std::string& resourceId, std::uint64_t size, const std::string& sha256, RestUploadSource source, RestCallOptions options = {});
    // result.sha256 covers the bytes delivered by this call, so it is the resource's
    // digest only when fromOffset is 0.
    RestResult download(const std::string& resourceId, RestDownloadSink sink, std::uint64_t fromOffset = 0, RestCallOptions options = {});

    RestResult getLog(const std::string& fileGlob, const std::vector<std::string>& filters = {}, std::size_t maxLines = 200);
    RestResult cmdStatus(const std::string& nameGlob = "*");
    RestResult appendLogMarker(const std::string& command, const std::string& event, const std::string& text, const std::string& origin = "client");

    // Ends calls and transfers in progress, from any thread. Subscriptions are
    // canceled through their own handles. Later calls are unaffected.
    void cancel();

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation;
};

} // namespace gigante
