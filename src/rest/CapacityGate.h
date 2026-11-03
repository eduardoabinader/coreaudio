#pragma once

#include "RestMetrics.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>

namespace gigante {

class CapacityGate;

// Releases its slot exactly once, and may safely outlive the gate that issued it.
class CapacityLease {
public:
    CapacityLease(CapacityLease&& other) noexcept;
    CapacityLease& operator=(CapacityLease&& other) noexcept;
    CapacityLease(const CapacityLease&) = delete;
    CapacityLease& operator=(const CapacityLease&) = delete;
    ~CapacityLease();

    void release() noexcept;

private:
    friend class CapacityGate;
    explicit CapacityLease(std::shared_ptr<std::atomic<std::size_t>> counter);

    std::shared_ptr<std::atomic<std::size_t>> counter;
};

class CapacityGate {
public:
    CapacityGate(std::size_t maxStreams, std::size_t maxTransfers, RestMetrics& metrics);

    std::optional<CapacityLease> tryAcquireStream();
    std::optional<CapacityLease> tryAcquireTransfer();
    void refuseNewLeases();
    void acceptNewLeases();
    std::size_t activeStreams() const;
    std::size_t activeTransfers() const;

private:
    std::optional<CapacityLease> tryAcquire(
        const std::shared_ptr<std::atomic<std::size_t>>& counter,
        std::size_t limit,
        std::atomic<std::uint64_t>& rejected);

    std::size_t maxStreams;
    std::size_t maxTransfers;
    RestMetrics& metrics;
    std::atomic<bool> refusing{false};
    std::shared_ptr<std::atomic<std::size_t>> streamCounter;
    std::shared_ptr<std::atomic<std::size_t>> transferCounter;
};

} // namespace gigante
