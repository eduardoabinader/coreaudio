#include "gigante/EffectsChain.h"

#include <cmath>

namespace gigante
{

Status EffectsChain::prepare(const PrepareSpecification& requested) noexcept
{
    requestedConfigurationValue = requested;

    if (!std::isfinite(requested.sampleRate) || requested.sampleRate <= 0.0)
    {
        release();
        requestedConfigurationValue = requested;
        return status(ErrorCode::invalidSampleRate);
    }
    if (requested.channelCount == 0 || requested.channelCount > 2)
    {
        release();
        requestedConfigurationValue = requested;
        return status(ErrorCode::invalidChannelCount);
    }
    if (requested.maximumBlockSize == 0)
    {
        release();
        requestedConfigurationValue = requested;
        return status(ErrorCode::invalidMaximumBlockSize);
    }

    effectiveConfigurationValue.sampleRate = requested.sampleRate;
    effectiveConfigurationValue.channelCount = requested.channelCount;
    effectiveConfigurationValue.maximumBlockSize = requested.maximumBlockSize;
    currentState = LifecycleState::prepared;
    reset();
    return status(ErrorCode::none);
}

void EffectsChain::reset() noexcept
{
    nonFiniteSampleTotal = 0;
}

void EffectsChain::release() noexcept
{
    reset();
    effectiveConfigurationValue = {};
    currentState = LifecycleState::unprepared;
}

ProcessReport EffectsChain::process(
    AudioBufferView buffer,
    const EffectsParameters& parameters,
    const ProcessingContext& context,
    EffectsStageObserver* observer) noexcept
{
    static_cast<void>(parameters);

    ProcessReport report;
    report.status = status(ErrorCode::none);
    report.algorithmicLatencySamples = algorithmicLatencySamples();
    report.bypassed = context.mode == ProcessingMode::neutralBypass;

    if (currentState != LifecycleState::prepared)
    {
        report.status = status(ErrorCode::notPrepared);
        return report;
    }
    if (context.mode != ProcessingMode::neutralBypass)
    {
        report.status = status(ErrorCode::effectsNotImplemented);
        return report;
    }
    if (buffer.channelCount != effectiveConfigurationValue.channelCount)
    {
        report.status = status(ErrorCode::channelCountMismatch);
        report.status.requestedValue = buffer.channelCount;
        report.status.effectiveValue = effectiveConfigurationValue.channelCount;
        return report;
    }
    if (buffer.sampleCount > effectiveConfigurationValue.maximumBlockSize)
    {
        report.status = status(ErrorCode::blockExceedsMaximum);
        report.status.requestedValue = buffer.sampleCount;
        report.status.effectiveValue = effectiveConfigurationValue.maximumBlockSize;
        return report;
    }
    if (buffer.channelCount > 0 && buffer.channels == nullptr)
    {
        report.status = status(ErrorCode::nullChannelArray);
        return report;
    }
    for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
    {
        if (buffer.sampleCount > 0 && buffer.channels[channel] == nullptr)
        {
            report.status = status(ErrorCode::nullChannelData);
            report.status.channelIndex = channel;
            return report;
        }
    }

    BlockObservation observation;
    observation.firstSamplePosition = context.firstSamplePosition;
    observation.channelCount = buffer.channelCount;
    observation.sampleCount = buffer.sampleCount;
    observation.algorithmicLatencySamples = algorithmicLatencySamples();
    observation.bypassed = true;

    if (observer != nullptr)
    {
        observer->onBlockBegin(observation);
        observer->onStageBegin(EffectsStage::input);
        observer->onStageEnd(EffectsStage::input);
    }

    for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
    {
        float* const samples = buffer.channels[channel];
        for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
        {
            if (!std::isfinite(samples[sample]))
            {
                samples[sample] = 0.0F;
                ++report.nonFiniteSampleCount;
            }
        }
    }

    nonFiniteSampleTotal += report.nonFiniteSampleCount;
    observation.nonFiniteSampleCount = report.nonFiniteSampleCount;

    if (observer != nullptr)
    {
        observer->onStageBegin(EffectsStage::output);
        observer->onStageEnd(EffectsStage::output);
        observer->onBlockEnd(observation);
    }

    return report;
}

LifecycleState EffectsChain::lifecycleState() const noexcept
{
    return currentState;
}

const PrepareSpecification& EffectsChain::requestedConfiguration() const noexcept
{
    return requestedConfigurationValue;
}

const EffectiveConfiguration& EffectsChain::effectiveConfiguration() const noexcept
{
    return effectiveConfigurationValue;
}

std::size_t EffectsChain::algorithmicLatencySamples() const noexcept
{
    return 0;
}

std::uint64_t EffectsChain::totalNonFiniteSampleCount() const noexcept
{
    return nonFiniteSampleTotal;
}

Status EffectsChain::status(ErrorCode code) const noexcept
{
    Status result;
    result.code = code;
    result.state = currentState;
    return result;
}

} // namespace gigante
