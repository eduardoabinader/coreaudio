#pragma once

#include "gigante/EffectsStageObserver.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace gigante
{

class TickClock
{
public:
    virtual ~TickClock() = default;
    virtual std::uint64_t currentTick() noexcept = 0;
};

struct RealtimeMetricsConfiguration
{
    std::uint64_t ticksPerSecond = 0;
    // Precompute from the effective sample rate outside the callback. Zero
    // explicitly disables callback-budget counting.
    double callbackBudgetTicksPerSample = 0.0;
};

struct SignalMetrics
{
    float peak = 0.0F;
    float rootMeanSquare = 0.0F;
    std::uint64_t finiteSampleCount = 0;
};

struct StageMetrics
{
    std::uint64_t callCount = 0;
    std::uint64_t totalTicks = 0;
    std::uint64_t maximumTicks = 0;
};

struct RealtimeMetricsSnapshot
{
    std::array<StageMetrics, effectsStageCount> stages {};
    SignalMetrics input {};
    SignalMetrics postChain {};
    SignalMetrics output {};
    std::uint64_t blockCount = 0;
    std::uint64_t maximumCallbackTicks = 0;
    std::uint64_t callbackBudgetExceededCount = 0;
    std::uint64_t resetCount = 0;
    std::uint64_t nonFiniteSampleCount = 0;
    std::uint64_t invalidParameterCount = 0;
    std::uint64_t recoveryCount = 0;
    std::uint64_t droppedPublicationCount = 0;
    std::uint64_t ticksPerSecond = 0;
    std::uint64_t firstSamplePosition = 0;
    std::size_t algorithmicLatencySamples = 0;
    bool bypassed = true;
};

class RealtimeMetricsObserver final : public EffectsStageObserver
{
public:
    static constexpr std::size_t publicationCapacity = 8;

    RealtimeMetricsObserver(
        TickClock& clock,
        const RealtimeMetricsConfiguration& configuration) noexcept;

    RealtimeMetricsObserver(const RealtimeMetricsObserver&) = delete;
    RealtimeMetricsObserver& operator=(const RealtimeMetricsObserver&) = delete;

    void onBlockBegin(const BlockObservation& observation) noexcept override;
    void onSignal(
        EffectsSignalPoint point,
        AudioBufferReadView buffer) noexcept override;
    void onStageBegin(EffectsStage stage) noexcept override;
    void onStageEnd(EffectsStage stage) noexcept override;
    void onBlockEnd(const BlockObservation& observation) noexcept override;
    void onReset() noexcept override;

    // One audio-thread producer and one non-realtime reader are supported.
    // Drains available publications and returns the newest stable snapshot, or
    // the last one again when no update is pending.
    RealtimeMetricsSnapshot snapshot() noexcept;

private:
    static std::size_t stageIndex(EffectsStage stage) noexcept;
    static SignalMetrics measureSignal(AudioBufferReadView buffer) noexcept;
    static void updateMaximum(std::uint64_t& maximum, std::uint64_t value) noexcept;
    void publish() noexcept;

    TickClock& tickClock;
    double budgetTicksPerSample = 0.0;
    RealtimeMetricsSnapshot current {};
    RealtimeMetricsSnapshot lastRead {};
    std::array<std::uint64_t, effectsStageCount> stageBeginTicks {};
    std::uint64_t blockBeginTick = 0;
    std::array<RealtimeMetricsSnapshot, publicationCapacity> publications {};
    std::atomic<std::uint32_t> writePosition { 0 };
    std::atomic<std::uint32_t> readPosition { 0 };
};

} // namespace gigante
