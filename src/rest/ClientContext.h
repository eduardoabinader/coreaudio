#pragma once

#include "CppHttplibClientTransport.h"

#include "gigante/RestTypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>

namespace gigante {

// What every client call shares: configuration, logging and cancellation. cancel()
// ends every transport opened under an earlier generation; later calls are unaffected.
class ClientContext final {
public:
    using Clock = std::chrono::steady_clock;

    explicit ClientContext(RestClientConfig config);

    const RestClientConfig& config() const;
    // Each timeout is capped by the remaining budget.
    ClientTransportConfig transportConfig(int budgetMilliseconds) const;

    std::uint64_t generation() const;
    bool isCanceled(std::uint64_t generation) const;
    void cancel();

    // Returns true when a cancel arrived before wakeAt.
    bool waitUntil(Clock::time_point wakeAt, std::uint64_t generation);

    void log(RestLogLevel level, const std::string& event, const std::string& detail) const;

private:
    friend class TrackedTransport;

    RestClientConfig clientConfig;
    mutable std::mutex mutex;
    std::condition_variable cancelled;
    std::set<HttpClientTransport*> active;
    std::atomic<std::uint64_t> cancelGeneration{0};
};

// A transport that cancel() can reach for as long as it exists.
class TrackedTransport final {
public:
    TrackedTransport(ClientContext& context, const ClientTransportConfig& config, std::uint64_t generation);
    ~TrackedTransport();

    TrackedTransport(const TrackedTransport&) = delete;
    TrackedTransport& operator=(const TrackedTransport&) = delete;

    // False when the generation was already canceled; the transport must not be used.
    bool isUsable() const;
    HttpClientTransport& transport();

private:
    ClientContext& context;
    CppHttplibClientTransport client;
    bool usable = false;
};

} // namespace gigante
