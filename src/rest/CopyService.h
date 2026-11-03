#pragma once

#include "CapacityGate.h"
#include "MiddlewareChain.h"
#include "RestMetrics.h"
#include "Router.h"
#include "Sha256.h"

#include "gigante/RestResourceStore.h"
#include "gigante/RestTypes.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gigante {

struct CopyServiceConfig {
    std::uint64_t maxTransferBytes = 0;
    int uploadBodyDeadlineMilliseconds = 600000;
    int resumeWindowMilliseconds = 60000;
    std::uint64_t maxDownloadBytesPerSecond = 0;
    std::size_t maxPendingTransfers = 64;
};

using ResponseFinisher = std::function<void(TransportResponse&, const TransportRequest&)>;
using ServiceLog = std::function<void(RestLogLevel, const std::string& event, const std::string& detail)>;

// The copy* routes. Resources are named by IDs the project's store owns, never by
// paths. An upload stages into the store's sink and commits only after the whole
// body has arrived and its digest matches, so no partial result is ever visible.
class CopyService final {
public:
    CopyService(
        std::shared_ptr<RestResourceStore> store,
        CapacityGate& gate,
        CopyServiceConfig config,
        MiddlewareErrorBuilder errorBuilder,
        ResponseFinisher finisher,
        ServiceLog log);
    ~CopyService();

    CopyService(const CopyService&) = delete;
    CopyService& operator=(const CopyService&) = delete;

    void registerRoutes(Router& router);

    // Discards every uncommitted transfer. Call after the transport has stopped.
    void close();
    void reopen();

    std::size_t pendingTransfers() const;

private:
    enum class State { created, receiving, paused, committed, failed, canceled };

    struct Transfer {
        std::string id;
        std::string resource;
        std::uint64_t size = 0;
        std::string expectedDigest;
        std::atomic<std::uint64_t> bytesReceived{0};
        State state = State::created;
        std::string reason;
        std::unique_ptr<RestUploadSink> sink;
        Sha256 digest;
        std::chrono::steady_clock::time_point lastActivity;
        std::atomic<bool> cancelRequested{false};
    };

    TransportResponse list(const TransportRequest& request);
    TransportResponse create(const TransportRequest& request);
    TransportResponse upload(const TransportRequest& request, const BodyChunkReader& reader);
    TransportResponse status(const TransportRequest& request);
    TransportResponse cancel(const TransportRequest& request);
    TransportResponse download(const TransportRequest& request);

    TransportResponse finishUpload(const TransportRequest& request, const std::shared_ptr<Transfer>& transfer);
    TransportResponse json(const TransportRequest& request, int status, std::string body);
    TransportResponse offsetConflict(const TransportRequest& request, const Transfer& transfer, int status, const std::string& code, const std::string& message, bool retryable);
    std::string describe(const Transfer& transfer) const;
    void purgeExpired();
    void discard(std::unique_ptr<RestUploadSink> sink);

    static const char* stateName(State state);
    static bool isFinal(State state);

    std::shared_ptr<RestResourceStore> store;
    CapacityGate& gate;
    CopyServiceConfig config;
    MiddlewareErrorBuilder errorBuilder;
    ResponseFinisher finisher;
    ServiceLog log;
    mutable std::mutex mutex;
    std::map<std::string, std::shared_ptr<Transfer>> transfers;
    bool closed = false;
};

} // namespace gigante
