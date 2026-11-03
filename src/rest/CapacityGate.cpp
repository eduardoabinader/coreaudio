#include "CapacityGate.h"

#include <utility>

namespace gigante {

CapacityLease::CapacityLease(std::shared_ptr<std::atomic<std::size_t>> leaseCounter)
    : counter(std::move(leaseCounter)) {}

CapacityLease::CapacityLease(CapacityLease&& other) noexcept : counter(std::move(other.counter)) {
    other.counter.reset();
}

CapacityLease& CapacityLease::operator=(CapacityLease&& other) noexcept {
    if (this != &other) {
        release();
        counter = std::move(other.counter);
        other.counter.reset();
    }
    return *this;
}

CapacityLease::~CapacityLease() {
    release();
}

void CapacityLease::release() noexcept {
    if (counter) {
        counter->fetch_sub(1);
        counter.reset();
    }
}

CapacityGate::CapacityGate(std::size_t streamLimit, std::size_t transferLimit, RestMetrics& restMetrics)
    : maxStreams(streamLimit),
      maxTransfers(transferLimit),
      metrics(restMetrics),
      streamCounter(std::make_shared<std::atomic<std::size_t>>(0)),
      transferCounter(std::make_shared<std::atomic<std::size_t>>(0)) {}

std::optional<CapacityLease> CapacityGate::tryAcquire(
    const std::shared_ptr<std::atomic<std::size_t>>& counter,
    std::size_t limit,
    std::atomic<std::uint64_t>& rejected) {
    if (!refusing.load()) {
        std::size_t current = counter->load();
        while (current < limit) {
            if (counter->compare_exchange_weak(current, current + 1)) {
                return CapacityLease(counter);
            }
        }
    }
    rejected.fetch_add(1);
    return std::nullopt;
}

std::optional<CapacityLease> CapacityGate::tryAcquireStream() {
    return tryAcquire(streamCounter, maxStreams, metrics.rejectedStreams);
}

std::optional<CapacityLease> CapacityGate::tryAcquireTransfer() {
    return tryAcquire(transferCounter, maxTransfers, metrics.rejectedTransfers);
}

void CapacityGate::refuseNewLeases() {
    refusing.store(true);
}

void CapacityGate::acceptNewLeases() {
    refusing.store(false);
}

std::size_t CapacityGate::activeStreams() const {
    return streamCounter->load();
}

std::size_t CapacityGate::activeTransfers() const {
    return transferCounter->load();
}

} // namespace gigante
