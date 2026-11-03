#pragma once

#include <atomic>
#include <cstdint>

namespace gigante {

struct RestMetricsSnapshot {
    std::uint64_t requests = 0;
    std::uint64_t rejectedStreams = 0;
    std::uint64_t rejectedTransfers = 0;
    std::uint64_t resourceExhaustion = 0;
    std::uint64_t transportErrors = 0;
    std::uint64_t droppedChunks = 0;
    std::uint64_t refusedChunks = 0;
    std::uint64_t evictedSubscribers = 0;
    std::uint64_t handlerExceptions = 0;
    std::uint64_t authenticationFailures = 0;
    std::uint64_t dispatcherFull = 0;
    std::uint64_t dispatcherDeadlines = 0;
};

class RestMetrics {
public:
    std::atomic<std::uint64_t> requests{0};
    std::atomic<std::uint64_t> rejectedStreams{0};
    std::atomic<std::uint64_t> rejectedTransfers{0};
    std::atomic<std::uint64_t> resourceExhaustion{0};
    std::atomic<std::uint64_t> transportErrors{0};
    std::atomic<std::uint64_t> droppedChunks{0};
    std::atomic<std::uint64_t> refusedChunks{0};
    std::atomic<std::uint64_t> evictedSubscribers{0};
    std::atomic<std::uint64_t> handlerExceptions{0};
    std::atomic<std::uint64_t> authenticationFailures{0};
    std::atomic<std::uint64_t> dispatcherFull{0};
    std::atomic<std::uint64_t> dispatcherDeadlines{0};

    RestMetricsSnapshot snapshot() const;
};

} // namespace gigante
