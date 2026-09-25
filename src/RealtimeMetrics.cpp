#include "gigante/RealtimeMetrics.h"

#include <cmath>
#include <limits>

namespace gigante
{
namespace
{
static_assert(
    std::atomic<std::uint32_t>::is_always_lock_free,
    "Realtime metrics publication requires lock-free 32-bit atomics");
}

RealtimeMetricsObserver::RealtimeMetricsObserver(
    TickClock& clock,
    const RealtimeMetricsConfiguration& configuration) noexcept
    : tickClock(clock)
{
    current.ticksPerSecond = configuration.ticksPerSecond;
    lastRead.ticksPerSecond = configuration.ticksPerSecond;
    if (std::isfinite(configuration.callbackBudgetTicksPerSample)
        && configuration.callbackBudgetTicksPerSample > 0.0)
    {
        budgetTicksPerSample = configuration.callbackBudgetTicksPerSample;
    }
}

void RealtimeMetricsObserver::onBlockBegin(
    const BlockObservation& observation) noexcept
{
    blockBeginTick = tickClock.currentTick();
    current.firstSamplePosition = observation.firstSamplePosition;
    current.algorithmicLatencySamples = observation.algorithmicLatencySamples;
    current.bypassed = observation.bypassed;
}

void RealtimeMetricsObserver::onSignal(
    EffectsSignalPoint point,
    AudioBufferReadView buffer) noexcept
{
    const SignalMetrics measured = measureSignal(buffer);
    switch (point)
    {
        case EffectsSignalPoint::input: current.input = measured; break;
        case EffectsSignalPoint::postChain: current.postChain = measured; break;
        case EffectsSignalPoint::output: current.output = measured; break;
    }
}

void RealtimeMetricsObserver::onStageBegin(EffectsStage stage) noexcept
{
    stageBeginTicks[stageIndex(stage)] = tickClock.currentTick();
}

void RealtimeMetricsObserver::onStageEnd(EffectsStage stage) noexcept
{
    const std::size_t index = stageIndex(stage);
    const std::uint64_t endTick = tickClock.currentTick();
    const std::uint64_t elapsed = endTick - stageBeginTicks[index];
    StageMetrics& metrics = current.stages[index];
    ++metrics.callCount;
    metrics.totalTicks += elapsed;
    updateMaximum(metrics.maximumTicks, elapsed);
}

void RealtimeMetricsObserver::onBlockEnd(
    const BlockObservation& observation) noexcept
{
    const std::uint64_t elapsed = tickClock.currentTick() - blockBeginTick;
    ++current.blockCount;
    updateMaximum(current.maximumCallbackTicks, elapsed);

    if (budgetTicksPerSample > 0.0)
    {
        const double budget = budgetTicksPerSample
            * static_cast<double>(observation.sampleCount);
        if (static_cast<double>(elapsed) > budget)
            ++current.callbackBudgetExceededCount;
    }

    current.nonFiniteSampleCount += observation.nonFiniteSampleCount;
    current.invalidParameterCount += observation.invalidParameterCount;
    current.recoveryCount += observation.recoveryCount;
    current.firstSamplePosition = observation.firstSamplePosition;
    current.algorithmicLatencySamples = observation.algorithmicLatencySamples;
    current.bypassed = observation.bypassed;
    publish();
}

void RealtimeMetricsObserver::onReset() noexcept
{
    ++current.resetCount;
}

RealtimeMetricsSnapshot RealtimeMetricsObserver::snapshot() noexcept
{
    std::uint32_t read = readPosition.load(std::memory_order_relaxed);
    const std::uint32_t written = writePosition.load(std::memory_order_acquire);
    while (read != written)
    {
        lastRead = publications[read];
        read = (read + 1U) % static_cast<std::uint32_t>(publicationCapacity);
    }
    readPosition.store(read, std::memory_order_release);
    return lastRead;
}

std::size_t RealtimeMetricsObserver::stageIndex(EffectsStage stage) noexcept
{
    return static_cast<std::size_t>(stage);
}

SignalMetrics RealtimeMetricsObserver::measureSignal(
    AudioBufferReadView buffer) noexcept
{
    SignalMetrics result;
    double sumSquares = 0.0;
    for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
    {
        const float* const samples = buffer.channels[channel];
        for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
        {
            const float value = samples[sample];
            if (!std::isfinite(value))
                continue;
            const float magnitude = std::abs(value);
            if (magnitude > result.peak)
                result.peak = magnitude;
            sumSquares += static_cast<double>(value) * static_cast<double>(value);
            ++result.finiteSampleCount;
        }
    }
    if (result.finiteSampleCount > 0)
    {
        result.rootMeanSquare = static_cast<float>(std::sqrt(
            sumSquares / static_cast<double>(result.finiteSampleCount)));
    }
    return result;
}

void RealtimeMetricsObserver::updateMaximum(
    std::uint64_t& maximum,
    std::uint64_t value) noexcept
{
    if (value > maximum)
        maximum = value;
}

void RealtimeMetricsObserver::publish() noexcept
{
    const std::uint32_t write = writePosition.load(std::memory_order_relaxed);
    const std::uint32_t next =
        (write + 1U) % static_cast<std::uint32_t>(publicationCapacity);
    if (next == readPosition.load(std::memory_order_acquire))
    {
        ++current.droppedPublicationCount;
        return;
    }
    publications[write] = current;
    writePosition.store(next, std::memory_order_release);
}

} // namespace gigante
