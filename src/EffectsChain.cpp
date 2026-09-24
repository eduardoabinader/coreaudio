#include "gigante/EffectsChain.h"

#include "EffectsChainImplementation.h"

#include <cmath>

namespace gigante
{

EffectsChain::EffectsChain()
    : implementation(createEffectsChainImplementation())
{
}

EffectsChain::~EffectsChain() = default;

Status EffectsChain::prepare(const PrepareSpecification& requested) noexcept
{
    return prepare(requested, EffectsParameters {});
}

Status EffectsChain::prepare(
    const PrepareSpecification& requested,
    const EffectsParameters& initialParameters) noexcept
{
    requestedConfigurationValue = requested;
    requestedParameterValues = initialParameters;

    if (!std::isfinite(requested.sampleRate) || requested.sampleRate <= 0.0)
    {
        release();
        requestedConfigurationValue = requested;
        requestedParameterValues = initialParameters;
        return status(ErrorCode::invalidSampleRate);
    }
    if (requested.channelCount == 0 || requested.channelCount > 2)
    {
        release();
        requestedConfigurationValue = requested;
        requestedParameterValues = initialParameters;
        return status(ErrorCode::invalidChannelCount);
    }
    if (requested.maximumBlockSize == 0)
    {
        release();
        requestedConfigurationValue = requested;
        requestedParameterValues = initialParameters;
        return status(ErrorCode::invalidMaximumBlockSize);
    }

    effectiveConfigurationValue.sampleRate = requested.sampleRate;
    effectiveConfigurationValue.channelCount = requested.channelCount;
    effectiveConfigurationValue.maximumBlockSize = requested.maximumBlockSize;

    std::uint64_t invalidParameterCount = 0;
    effectiveParameterValues = resolveParameters(
        initialParameters, invalidParameterCount);
    invalidParameterTotal = invalidParameterCount;
    recoveryTotal = invalidParameterCount > 0 ? 1 : 0;
    nonFiniteSampleTotal = 0;

    implementation->prepare(requested, effectiveParameterValues);
    currentState = LifecycleState::prepared;
    return status(ErrorCode::none);
}

void EffectsChain::reset() noexcept
{
    reset(effectiveParameterValues);
}

void EffectsChain::reset(const EffectsParameters& parameters) noexcept
{
    requestedParameterValues = parameters;
    std::uint64_t ignoredInvalidParameterCount = 0;
    effectiveParameterValues = resolveParameters(
        parameters, ignoredInvalidParameterCount);
    implementation->reset(effectiveParameterValues);
    nonFiniteSampleTotal = 0;
    invalidParameterTotal = 0;
    recoveryTotal = 0;
}

void EffectsChain::release() noexcept
{
    implementation->release();
    nonFiniteSampleTotal = 0;
    invalidParameterTotal = 0;
    recoveryTotal = 0;
    effectiveConfigurationValue = {};
    currentState = LifecycleState::unprepared;
}

ProcessReport EffectsChain::process(
    AudioBufferView buffer,
    const EffectsParameters& parameters,
    const ProcessingContext& context,
    EffectsStageObserver* observer) noexcept
{
    ProcessReport report;
    report.status = status(ErrorCode::none);
    report.algorithmicLatencySamples = algorithmicLatencySamples();
    report.bypassed = context.mode == ProcessingMode::neutralBypass;

    if (currentState != LifecycleState::prepared)
    {
        report.status = status(ErrorCode::notPrepared);
        return report;
    }
    if (context.mode == ProcessingMode::effects && !implementation->effectsAvailable())
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

    requestedParameterValues = parameters;
    effectiveParameterValues = resolveParameters(
        parameters, report.invalidParameterCount);
    if (report.invalidParameterCount > 0)
        ++report.recoveryCount;

    BlockObservation observation;
    observation.firstSamplePosition = context.firstSamplePosition;
    observation.channelCount = buffer.channelCount;
    observation.sampleCount = buffer.sampleCount;
    observation.algorithmicLatencySamples = algorithmicLatencySamples();
    observation.bypassed = report.bypassed;

    if (observer != nullptr)
    {
        observer->onBlockBegin(observation);
        observer->onStageBegin(EffectsStage::input);
        observer->onStageEnd(EffectsStage::input);
    }

    const auto replaceNonFiniteSamples = [&buffer, &report]() noexcept
    {
        std::uint64_t replaced = 0;
        for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
        {
            float* const samples = buffer.channels[channel];
            for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
            {
                if (!std::isfinite(samples[sample]))
                {
                    samples[sample] = 0.0F;
                    ++replaced;
                }
            }
        }
        report.nonFiniteSampleCount += replaced;
        if (replaced > 0)
            ++report.recoveryCount;
    };

    replaceNonFiniteSamples();

    if (context.mode == ProcessingMode::effects)
    {
        implementation->process(
            buffer,
            effectiveParameterValues,
            context,
            observer,
            report);
        replaceNonFiniteSamples();
    }

    nonFiniteSampleTotal += report.nonFiniteSampleCount;
    invalidParameterTotal += report.invalidParameterCount;
    recoveryTotal += report.recoveryCount;
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

const EffectsParameters& EffectsChain::requestedParameters() const noexcept
{
    return requestedParameterValues;
}

const EffectsParameters& EffectsChain::effectiveParameters() const noexcept
{
    return effectiveParameterValues;
}

std::size_t EffectsChain::algorithmicLatencySamples() const noexcept
{
    return implementation->algorithmicLatencySamples();
}

std::uint64_t EffectsChain::totalNonFiniteSampleCount() const noexcept
{
    return nonFiniteSampleTotal;
}

std::uint64_t EffectsChain::totalInvalidParameterCount() const noexcept
{
    return invalidParameterTotal;
}

std::uint64_t EffectsChain::totalRecoveryCount() const noexcept
{
    return recoveryTotal;
}

bool EffectsChain::effectsAvailable() const noexcept
{
    return implementation->effectsAvailable();
}

Status EffectsChain::status(ErrorCode code) const noexcept
{
    Status result;
    result.code = code;
    result.state = currentState;
    return result;
}

EffectsParameters EffectsChain::resolveParameters(
    const EffectsParameters& requested,
    std::uint64_t& invalidParameterCount) noexcept
{
    EffectsParameters resolved = requested;
    const auto finiteOrPrevious = [&invalidParameterCount](
        float value,
        float previous) noexcept
    {
        if (std::isfinite(value))
            return value;
        ++invalidParameterCount;
        return previous;
    };

    resolved.tremoloDepth = finiteOrPrevious(
        requested.tremoloDepth, effectiveParameterValues.tremoloDepth);
    resolved.tremoloRateHz = finiteOrPrevious(
        requested.tremoloRateHz, effectiveParameterValues.tremoloRateHz);
    resolved.growlAmount = finiteOrPrevious(
        requested.growlAmount, effectiveParameterValues.growlAmount);
    resolved.growlAttackMs = finiteOrPrevious(
        requested.growlAttackMs, effectiveParameterValues.growlAttackMs);
    resolved.growlReleaseMs = finiteOrPrevious(
        requested.growlReleaseMs, effectiveParameterValues.growlReleaseMs);
    resolved.filterFrequencyHz = finiteOrPrevious(
        requested.filterFrequencyHz, effectiveParameterValues.filterFrequencyHz);
    resolved.filterResonance = finiteOrPrevious(
        requested.filterResonance, effectiveParameterValues.filterResonance);
    resolved.driveAmount = finiteOrPrevious(
        requested.driveAmount, effectiveParameterValues.driveAmount);
    resolved.bellCenterHz = finiteOrPrevious(
        requested.bellCenterHz, effectiveParameterValues.bellCenterHz);
    resolved.bellMix = finiteOrPrevious(
        requested.bellMix, effectiveParameterValues.bellMix);
    resolved.phaserAmount = finiteOrPrevious(
        requested.phaserAmount, effectiveParameterValues.phaserAmount);
    resolved.phaserRateHz = finiteOrPrevious(
        requested.phaserRateHz, effectiveParameterValues.phaserRateHz);
    resolved.chorusDepth = finiteOrPrevious(
        requested.chorusDepth, effectiveParameterValues.chorusDepth);
    resolved.chorusRateHz = finiteOrPrevious(
        requested.chorusRateHz, effectiveParameterValues.chorusRateHz);
    resolved.chorusMix = finiteOrPrevious(
        requested.chorusMix, effectiveParameterValues.chorusMix);
    resolved.chorusCenterDelayMs = finiteOrPrevious(
        requested.chorusCenterDelayMs,
        effectiveParameterValues.chorusCenterDelayMs);
    resolved.reverbMix = finiteOrPrevious(
        requested.reverbMix, effectiveParameterValues.reverbMix);
    resolved.reverbSize = finiteOrPrevious(
        requested.reverbSize, effectiveParameterValues.reverbSize);
    resolved.reverbDamping = finiteOrPrevious(
        requested.reverbDamping, effectiveParameterValues.reverbDamping);
    resolved.noiseAmount = finiteOrPrevious(
        requested.noiseAmount, effectiveParameterValues.noiseAmount);
    resolved.noiseColor = finiteOrPrevious(
        requested.noiseColor, effectiveParameterValues.noiseColor);
    resolved.outputGainDb = finiteOrPrevious(
        requested.outputGainDb, effectiveParameterValues.outputGainDb);
    return resolved;
}

} // namespace gigante
