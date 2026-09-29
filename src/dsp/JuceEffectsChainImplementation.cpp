#include "EffectsChainImplementation.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace gigante
{
namespace
{
constexpr float pi = 3.14159265358979323846F;
constexpr float twoPi = 2.0F * pi;
constexpr float filterMinimumHz = 250.0F;
constexpr float filterMaximumHz = 11000.0F;
constexpr int controlStride = 32;
constexpr int chorusRingSize = 8192;

float resonanceToQ(float requested) noexcept
{
    constexpr float parameterMinimum = 0.1F;
    constexpr float parameterMaximum = 1.0F;
    constexpr float qMinimum = 0.707F;
    constexpr float qMaximum = 8.0F;
    const float amount = std::clamp(
        (requested - parameterMinimum) / (parameterMaximum - parameterMinimum),
        0.0F,
        1.0F);
    return qMinimum + (qMaximum - qMinimum) * amount * amount;
}

float drivePreGain(float requested) noexcept
{
    return 1.0F + 15.0F * std::sqrt(std::clamp(requested, 0.0F, 1.0F));
}

float growlEnvelopeDrive(float envelope, float inverseReference) noexcept
{
    const float normalized = std::clamp(envelope * inverseReference, 0.0F, 1.5F);
    if (normalized <= 1.0F)
        return normalized * (1.5F - 0.5F * normalized);
    return 1.0F + 0.25F * (normalized - 1.0F);
}

float chorusDepthSamples(float requestedMs, float rateHz, float sampleRate) noexcept
{
    const float requestedSamples = std::max(0.0F, requestedMs) * 0.001F * sampleRate;
    const float publicMaximumSamples = 15.0F * 0.001F * std::max(0.0F, sampleRate);
    if (requestedSamples <= 0.0F || rateHz <= 0.0F || publicMaximumSamples <= 0.0F)
        return 0.0F;
    const float capSamples = 0.06F * sampleRate / (twoPi * rateHz);
    if (capSamples >= publicMaximumSamples)
        return requestedSamples;
    const float amount = std::clamp(requestedMs / 15.0F, 0.0F, 1.0F);
    return capSamples * std::pow(amount, 1.7F);
}

float chorusWetGain(float requested) noexcept
{
    return 0.45F * std::pow(std::clamp(requested, 0.0F, 1.0F), 0.75F);
}

class StageScope final
{
public:
    StageScope(EffectsStageObserver* value, EffectsStage stageValue) noexcept
        : observer(value), stage(stageValue)
    {
        if (observer != nullptr)
            observer->onStageBegin(stage);
    }

    ~StageScope()
    {
        if (observer != nullptr)
            observer->onStageEnd(stage);
    }

private:
    EffectsStageObserver* observer;
    EffectsStage stage;
};

class JuceEffectsChainImplementation final : public EffectsChainImplementation
{
public:
    bool effectsAvailable() const noexcept override { return true; }

    void prepare(
        const PrepareSpecification& specification,
        const EffectsParameters& parameters) override
    {
        sampleRate = static_cast<float>(specification.sampleRate);
        channelCount = specification.channelCount;
        maximumBlockSize = specification.maximumBlockSize;
        chorusDelay.assign(channelCount * static_cast<std::size_t>(chorusRingSize), 0.0F);
        bellScratch.assign(channelCount * maximumBlockSize, 0.0F);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = specification.sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(specification.maximumBlockSize);
        spec.numChannels = static_cast<juce::uint32>(specification.channelCount);
        filter.prepare(spec);
        filterSlope.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filterSlope.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        juce::dsp::Reverb::Parameters initialReverbValues;
        initialReverbValues.roomSize = parameters.reverbSize;
        initialReverbValues.damping = parameters.reverbDamping;
        initialReverbValues.wetLevel = parameters.reverbMix;
        initialReverbValues.dryLevel = 1.0F;
        initialReverbValues.width = 1.0F;
        initialReverbValues.freezeMode = 0.0F;
        reverb.setParameters(initialReverbValues);
        reverb.prepare(spec);
        phaser.setRate(parameters.phaserRateHz);
        phaser.setDepth(0.65F);
        phaser.setCentreFrequency(900.0F);
        phaser.setFeedback(0.20F);
        phaser.prepare(spec);
        for (auto& bell : bellFilters)
            bell.prepare(spec);

        filterCutoff.reset(specification.sampleRate, 0.050);
        filterResonance.reset(specification.sampleRate, 0.050);
        driveAmount.reset(specification.sampleRate, 0.015);
        growlAmount.reset(specification.sampleRate, 0.015);
        tremoloDepth.reset(specification.sampleRate, 0.050);
        tremoloRate.reset(specification.sampleRate, 0.050);
        chorusRate.reset(specification.sampleRate, 0.050);
        chorusDepth.reset(specification.sampleRate, 0.050);
        chorusMix.reset(specification.sampleRate, 0.050);
        chorusCenterDelay.reset(specification.sampleRate, 0.050);
        limiterAttack = std::exp(-1.0F / (0.0005F * sampleRate));
        limiterRelease = std::exp(-1.0F / (0.100F * sampleRate));
        reset(parameters);
        // Preserve the characterized prepare order: initial mix precedes reset.
        phaser.reset();
    }

    void reset(const EffectsParameters& parameters) noexcept override
    {
        const float cutoff = clampedCutoff(parameters.filterFrequencyHz);
        const float q = resonanceToQ(parameters.filterResonance);
        filter.reset();
        filterSlope.reset();
        filter.setCutoffFrequency(cutoff);
        filterSlope.setCutoffFrequency(cutoff);
        filter.setResonance(q);
        filterSlope.setResonance(0.70710678F);
        filterCutoff.setCurrentAndTargetValue(cutoff);
        filterResonance.setCurrentAndTargetValue(q);
        driveAmount.setCurrentAndTargetValue(parameters.driveAmount);
        growlAmount.setCurrentAndTargetValue(parameters.growlAmount);
        tremoloDepth.setCurrentAndTargetValue(parameters.tremoloDepth);
        tremoloRate.setCurrentAndTargetValue(parameters.tremoloRateHz);
        chorusRate.setCurrentAndTargetValue(parameters.chorusRateHz);
        chorusDepth.setCurrentAndTargetValue(parameters.chorusDepth);
        chorusMix.setCurrentAndTargetValue(parameters.chorusMix);
        chorusCenterDelay.setCurrentAndTargetValue(parameters.chorusCenterDelayMs);
        tremoloPhase = 0.0F;
        chorusPhase = 0.0F;
        chorusWritePosition = 0;
        std::fill(chorusDelay.begin(), chorusDelay.end(), 0.0F);
        reverb.reset();
        lastReverbMix = parameters.reverbMix;
        lastReverbSize = parameters.reverbSize;
        lastReverbDamping = parameters.reverbDamping;
        phaser.reset();
        lastPhaserRate = parameters.phaserRateHz;
        lastPhaserAmount = parameters.phaserAmount;
        phaser.setRate(lastPhaserRate);
        phaser.setMix(0.5F * std::clamp(lastPhaserAmount, 0.0F, 1.0F));
        phaserFadeSamples = 0;
        phaserBypassed = lastPhaserAmount <= 0.0F;
        for (auto& bell : bellFilters)
            bell.reset();
        activeBell = 0;
        targetBell = 1;
        bellCrossfadePosition = 0;
        bellCrossfadeLength = std::max(1, static_cast<int>(std::round(sampleRate * 0.020F)));
        bellCrossfadeActive = false;
        configureBell(0, parameters);
        configureBell(1, parameters);
        lastBellCenter = parameters.bellCenterHz;
        lastBellMix = parameters.bellMix;
        growlEnvelope.fill(0.0F);
        growlPostBiteLowpass.fill(0.0F);
        growlPostBodyLowpass.fill(0.0F);
        growlPostBodyHighpass.fill(0.0F);
        growlPostEnvelope.fill(0.0F);
        growlStrideCount = 0;
        growlCutoff = cutoff;
        growlFilterDirty = false;
        limiterGain.fill(1.0F);
        lastFinalGain = 1.0F;
        noisePink = {};
    }

    void release() noexcept override
    {
        chorusDelay.clear();
        bellScratch.clear();
        sampleRate = 0.0F;
        channelCount = 0;
        maximumBlockSize = 0;
    }

    void process(
        AudioBufferView buffer,
        const EffectsParameters& parameters,
        const ProcessingContext& context,
        EffectsStageObserver* observer,
        ProcessReport& report) noexcept override
    {
        juce::dsp::AudioBlock<float> block(buffer.channels, buffer.channelCount, buffer.sampleCount);
        {
            StageScope stage(observer, EffectsStage::tremolo);
            processTremolo(buffer, parameters);
        }
        {
            StageScope stage(observer, EffectsStage::growlControl);
            processGrowlControl(buffer, parameters, context.sourceLevel);
        }
        {
            StageScope stage(observer, EffectsStage::filter);
            processFilter(block, parameters);
        }
        {
            StageScope stage(observer, EffectsStage::growlPostFilter);
            processGrowlPostFilter(buffer, parameters, context.sourceLevel);
        }
        if (buffer.sampleCount > 0 && !std::isfinite(buffer.channels[0][0]))
        {
            filter.reset();
            filterSlope.reset();
            const float restored = std::min(filterCutoff.getCurrentValue(), maximumCutoff());
            filter.setCutoffFrequency(restored);
            filterSlope.setCutoffFrequency(restored);
            for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
                std::fill_n(buffer.channels[channel], buffer.sampleCount, 0.0F);
            ++report.recoveryCount;
            ++report.nonFiniteSampleCount;
        }
        {
            StageScope stage(observer, EffectsStage::drive);
            processDrive(buffer, parameters, context.sourceLevel);
        }
        {
            StageScope stage(observer, EffectsStage::bellPeak);
            processBell(block, buffer, parameters);
        }
        {
            StageScope stage(observer, EffectsStage::phaser);
            processPhaser(block, buffer.sampleCount, parameters);
        }
        {
            StageScope stage(observer, EffectsStage::chorus);
            processChorus(buffer, parameters);
        }
        {
            StageScope stage(observer, EffectsStage::reverb);
            updateReverb(parameters, false);
            juce::dsp::ProcessContextReplacing<float> processContext(block);
            reverb.process(processContext);
        }
        {
            StageScope stage(observer, EffectsStage::noise);
            if (context.generatedNoiseEnabled)
                processNoise(buffer, parameters);
        }
        {
            StageScope stage(observer, EffectsStage::softLimiter);
            processLimiter(buffer);
        }
        {
            StageScope stage(observer, EffectsStage::outputGain);
            processOutputGain(buffer, parameters.outputGainDb, context.outputMuted);
        }
    }

    std::size_t algorithmicLatencySamples() const noexcept override { return 0; }

private:
    using BellFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    float maximumCutoff() const noexcept
    {
        return std::min(filterMaximumHz, sampleRate * 0.40F);
    }

    float clampedCutoff(float value) const noexcept
    {
        return std::clamp(value, filterMinimumHz, maximumCutoff());
    }

    void processTremolo(AudioBufferView buffer, const EffectsParameters& parameters) noexcept
    {
        const float targetRate = std::max(0.0F, parameters.tremoloRateHz);
        const float targetDepth = std::clamp(parameters.tremoloDepth, 0.0F, 1.0F);
        tremoloRate.setTargetValue(targetRate);
        tremoloDepth.setTargetValue(targetDepth);
        if ((targetRate <= 0.0F && tremoloRate.getCurrentValue() <= 0.0F && !tremoloRate.isSmoothing())
            || (targetDepth <= 0.0F && tremoloDepth.getCurrentValue() <= 0.0F && !tremoloDepth.isSmoothing()))
        {
            tremoloPhase = 0.0F;
            return;
        }
        const float startRate = tremoloRate.getCurrentValue();
        const float endRate = tremoloRate.skip(static_cast<int>(buffer.sampleCount));
        const float rate = std::max(0.0F, 0.5F * (startRate + endRate));
        const float stepCycles = rate / sampleRate;
        float sine = std::sin(twoPi * tremoloPhase);
        float cosine = std::cos(twoPi * tremoloPhase);
        const float stepSine = std::sin(twoPi * stepCycles);
        const float stepCosine = std::cos(twoPi * stepCycles);
        for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
        {
            const float depth = 0.5F * tremoloDepth.getNextValue();
            const float gain = 1.0F - depth * (1.0F + sine);
            for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
                buffer.channels[channel][sample] *= gain;
            const float nextSine = sine * stepCosine + cosine * stepSine;
            cosine = cosine * stepCosine - sine * stepSine;
            sine = nextSine;
        }
        tremoloPhase += stepCycles * static_cast<float>(buffer.sampleCount);
        tremoloPhase -= std::floor(tremoloPhase);
    }

    void processGrowlControl(
        AudioBufferView buffer,
        const EffectsParameters& parameters,
        float sourceLevel) noexcept
    {
        growlAmount.setTargetValue(parameters.growlAmount);
        if (!growlAmount.isSmoothing() && growlAmount.getCurrentValue() <= 0.0F)
        {
            growlEnvelope.fill(0.0F);
            growlStrideCount = 0;
            growlPostBiteLowpass.fill(0.0F);
            growlPostBodyLowpass.fill(0.0F);
            growlPostBodyHighpass.fill(0.0F);
            growlPostEnvelope.fill(0.0F);
            if (growlFilterDirty)
            {
                const float restored = std::min(filterCutoff.getCurrentValue(), maximumCutoff());
                growlCutoff = restored;
                filter.setCutoffFrequency(restored);
                filterSlope.setCutoffFrequency(restored);
                growlFilterDirty = false;
            }
            return;
        }
        float amount = growlAmount.getCurrentValue();
        const float reference = 0.055F * sourceLevel;
        const float inverseReference = reference > 0.0F ? 1.0F / reference : 0.0F;
        const float attack = std::exp(-1.0F / (parameters.growlAttackMs * 0.001F * sampleRate));
        const float releaseValue = std::exp(-1.0F / (parameters.growlReleaseMs * 0.001F * sampleRate));
        const float baseCutoff = filterCutoff.getCurrentValue();
        const float maximumModulation = std::max(baseCutoff * 3.0F, 3600.0F);
        for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
        {
            const float absolute = std::abs(buffer.channels[0][sample]);
            const float envelope = growlEnvelope[0];
            growlEnvelope[0] = absolute > envelope
                ? attack * envelope + (1.0F - attack) * absolute
                : releaseValue * envelope;
            if (++growlStrideCount >= controlStride)
            {
                growlStrideCount = 0;
                amount = growlAmount.skip(controlStride);
                const float drive = growlEnvelopeDrive(growlEnvelope[0], inverseReference);
                const float target = std::min(
                    baseCutoff + drive * amount * maximumModulation,
                    std::min(filterMaximumHz, sampleRate * 0.35F));
                if (growlCutoff <= 0.0F)
                    growlCutoff = baseCutoff;
                growlCutoff += 0.18F * (target - growlCutoff);
                filter.setCutoffFrequency(growlCutoff);
                filterSlope.setCutoffFrequency(growlCutoff);
                growlFilterDirty = true;
            }
        }
    }

    void processFilter(
        juce::dsp::AudioBlock<float>& block,
        const EffectsParameters& parameters) noexcept
    {
        filterCutoff.setTargetValue(clampedCutoff(parameters.filterFrequencyHz));
        filterResonance.setTargetValue(resonanceToQ(parameters.filterResonance));
        const bool cutoffRamping = filterCutoff.isSmoothing();
        const bool resonanceRamping = filterResonance.isSmoothing();
        if (!cutoffRamping && !resonanceRamping)
        {
            juce::dsp::ProcessContextReplacing<float> context(block);
            filter.process(context);
            filterSlope.process(context);
            return;
        }
        const bool growlControlsCutoff = growlAmount.getCurrentValue() > 0.0F
            || growlAmount.isSmoothing();
        std::size_t offset = 0;
        while (offset < block.getNumSamples())
        {
            const auto chunk = std::min<std::size_t>(controlStride, block.getNumSamples() - offset);
            if (cutoffRamping)
            {
                const float smoothed = filterCutoff.skip(static_cast<int>(chunk));
                if (!growlControlsCutoff)
                {
                    const float cutoff = std::min(smoothed, maximumCutoff());
                    filter.setCutoffFrequency(cutoff);
                    filterSlope.setCutoffFrequency(cutoff);
                }
            }
            if (resonanceRamping)
                filter.setResonance(filterResonance.skip(static_cast<int>(chunk)));
            auto subBlock = block.getSubBlock(offset, chunk);
            juce::dsp::ProcessContextReplacing<float> context(subBlock);
            filter.process(context);
            filterSlope.process(context);
            offset += chunk;
        }
    }

    void processGrowlPostFilter(
        AudioBufferView buffer,
        const EffectsParameters& parameters,
        float sourceLevel) noexcept
    {
        if (!growlAmount.isSmoothing() && growlAmount.getCurrentValue() <= 0.0F)
            return;
        const float amount = growlAmount.getCurrentValue();
        if (amount <= 0.0F)
            return;
        const float biteCoefficient = std::exp(-twoPi * 180.0F / sampleRate);
        const float bodyLowCoefficient = std::exp(-twoPi * 90.0F / sampleRate);
        const float bodyHighCoefficient = std::exp(-twoPi * 420.0F / sampleRate);
        const float attack = std::exp(-1.0F / (parameters.growlAttackMs * 0.001F * sampleRate));
        const float releaseValue = std::exp(-1.0F / (parameters.growlReleaseMs * 0.001F * sampleRate));
        const float reference = 0.055F * sourceLevel;
        const float inverseReference = reference > 0.0F ? 1.0F / reference : 0.0F;
        for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
        {
            float& lowpass = growlPostBiteLowpass[channel];
            float& bodyLow = growlPostBodyLowpass[channel];
            float& bodyHigh = growlPostBodyHighpass[channel];
            float& envelope = growlPostEnvelope[channel];
            for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
            {
                const float dry = buffer.channels[channel][sample];
                const float absolute = std::abs(dry);
                envelope = absolute > envelope
                    ? attack * envelope + (1.0F - attack) * absolute
                    : releaseValue * envelope;
                const float drive = growlEnvelopeDrive(envelope, inverseReference * 2.2F);
                const float bite = std::clamp(amount * drive * 0.60F, 0.0F, 0.80F);
                const float body = amount * drive * 0.95F;
                const float headroom = std::clamp(1.0F - amount * drive * 0.32F, 0.78F, 1.0F);
                lowpass += (1.0F - biteCoefficient) * (dry - lowpass);
                bodyLow += (1.0F - bodyLowCoefficient) * (dry - bodyLow);
                bodyHigh += (1.0F - bodyHighCoefficient) * (dry - bodyHigh);
                const float biteBand = dry - lowpass;
                const float bodyBand = bodyHigh - bodyLow;
                const float grit = std::tanh(bodyBand * (1.0F + 5.0F * body));
                buffer.channels[channel][sample] =
                    (dry + bite * biteBand + body * bodyBand + body * 0.020F * grit) * headroom;
            }
        }
    }

    void processDrive(
        AudioBufferView buffer,
        const EffectsParameters& parameters,
        float sourceLevel) noexcept
    {
        driveAmount.setTargetValue(parameters.driveAmount);
        if (!driveAmount.isSmoothing() && driveAmount.getCurrentValue() <= 0.0F)
            return;
        const float reference = 0.14F * sourceLevel;
        std::size_t offset = 0;
        while (offset < buffer.sampleCount)
        {
            const auto chunk = std::min<std::size_t>(controlStride, buffer.sampleCount - offset);
            const float drive = driveAmount.skip(static_cast<int>(chunk));
            if (drive > 0.0F)
            {
                const float preGain = drivePreGain(drive);
                const float normalGain = reference / std::tanh(reference * preGain);
                for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
                    for (std::size_t sample = offset; sample < offset + chunk; ++sample)
                        buffer.channels[channel][sample] =
                            std::tanh(buffer.channels[channel][sample] * preGain) * normalGain;
            }
            offset += chunk;
        }
    }

    void configureBell(std::size_t index, const EffectsParameters& parameters) noexcept
    {
        const auto coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
            sampleRate,
            parameters.bellCenterHz,
            1.3F,
            juce::Decibels::decibelsToGain(parameters.bellMix * 12.0F));
        *bellFilters[index].state = coefficients;
    }

    void processBell(
        juce::dsp::AudioBlock<float>& block,
        AudioBufferView buffer,
        const EffectsParameters& parameters) noexcept
    {
        if (!bellCrossfadeActive
            && (parameters.bellCenterHz != lastBellCenter || parameters.bellMix != lastBellMix))
        {
            lastBellCenter = parameters.bellCenterHz;
            lastBellMix = parameters.bellMix;
            targetBell = 1 - activeBell;
            bellFilters[static_cast<std::size_t>(targetBell)].reset();
            configureBell(static_cast<std::size_t>(targetBell), parameters);
            bellCrossfadePosition = 0;
            bellCrossfadeActive = true;
        }
        if (!bellCrossfadeActive)
        {
            juce::dsp::ProcessContextReplacing<float> context(block);
            bellFilters[static_cast<std::size_t>(activeBell)].process(context);
            return;
        }
        std::array<float*, 2> scratchChannels {};
        for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
        {
            scratchChannels[channel] = bellScratch.data() + channel * maximumBlockSize;
            std::copy_n(buffer.channels[channel], buffer.sampleCount, scratchChannels[channel]);
        }
        juce::dsp::AudioBlock<float> scratchBlock(
            scratchChannels.data(), buffer.channelCount, buffer.sampleCount);
        juce::dsp::ProcessContextReplacing<float> currentContext(block);
        bellFilters[static_cast<std::size_t>(activeBell)].process(currentContext);
        juce::dsp::ProcessContextReplacing<float> targetContext(scratchBlock);
        bellFilters[static_cast<std::size_t>(targetBell)].process(targetContext);
        for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
        {
            const float alpha = std::min(
                1.0F,
                static_cast<float>(bellCrossfadePosition + static_cast<int>(sample))
                    / static_cast<float>(bellCrossfadeLength));
            for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
                buffer.channels[channel][sample] += alpha
                    * (scratchChannels[channel][sample] - buffer.channels[channel][sample]);
        }
        bellCrossfadePosition += static_cast<int>(buffer.sampleCount);
        if (bellCrossfadePosition >= bellCrossfadeLength)
        {
            activeBell = targetBell;
            bellCrossfadeActive = false;
        }
    }

    void processPhaser(
        juce::dsp::AudioBlock<float>& block,
        std::size_t sampleCountValue,
        const EffectsParameters& parameters) noexcept
    {
        if (parameters.phaserRateHz != lastPhaserRate)
        {
            lastPhaserRate = parameters.phaserRateHz;
            phaser.setRate(lastPhaserRate);
        }
        if (parameters.phaserAmount != lastPhaserAmount)
        {
            const bool turningOff = parameters.phaserAmount <= 0.0F && lastPhaserAmount > 0.0F;
            lastPhaserAmount = parameters.phaserAmount;
            phaser.setMix(0.5F * std::clamp(lastPhaserAmount, 0.0F, 1.0F));
            if (turningOff)
                phaserFadeSamples = std::max(1, static_cast<int>(std::round(sampleRate * 0.050F)));
            if (lastPhaserAmount > 0.0F)
                phaserBypassed = false;
        }
        if (phaserBypassed)
            return;
        juce::dsp::ProcessContextReplacing<float> context(block);
        phaser.process(context);
        if (lastPhaserAmount <= 0.0F && phaserFadeSamples > 0)
        {
            phaserFadeSamples -= static_cast<int>(sampleCountValue);
            if (phaserFadeSamples <= 0)
            {
                phaser.reset();
                phaserBypassed = true;
            }
        }
    }

    void processChorus(AudioBufferView buffer, const EffectsParameters& parameters) noexcept
    {
        const float targetRate = std::max(0.0F, parameters.chorusRateHz);
        const float targetDepth = std::max(0.0F, parameters.chorusDepth);
        chorusRate.setTargetValue(targetRate);
        chorusDepth.setTargetValue(targetDepth);
        chorusMix.setTargetValue(std::clamp(parameters.chorusMix, 0.0F, 1.0F));
        chorusCenterDelay.setTargetValue(parameters.chorusCenterDelayMs);
        if ((targetDepth <= 0.0F && chorusDepth.getCurrentValue() <= 0.0F && !chorusDepth.isSmoothing())
            || (targetRate <= 0.0F && chorusRate.getCurrentValue() <= 0.0F && !chorusRate.isSmoothing()))
        {
            chorusPhase = 0.0F;
            return;
        }
        float sine = std::sin(twoPi * chorusPhase);
        float cosine = std::cos(twoPi * chorusPhase);
        float phaseAccumulator = chorusPhase;
        std::size_t offset = 0;
        while (offset < buffer.sampleCount)
        {
            const auto chunk = std::min<std::size_t>(controlStride, buffer.sampleCount - offset);
            const float rate = chorusRate.skip(static_cast<int>(chunk));
            const float depth = chorusDepth.skip(static_cast<int>(chunk));
            const float center = chorusCenterDelay.skip(static_cast<int>(chunk)) * 0.001F * sampleRate;
            const float wet = chorusWetGain(chorusMix.skip(static_cast<int>(chunk)));
            const float dry = 1.0F - wet;
            const float requestedDepth = chorusDepthSamples(depth, rate, sampleRate);
            const float effectiveDepth = std::min(
                requestedDepth,
                std::min(std::max(0.0F, center - 1.0F),
                         std::max(0.0F, static_cast<float>(chorusRingSize - 2) - center)));
            const float phaseStep = rate / sampleRate;
            const float stepSine = std::sin(twoPi * phaseStep);
            const float stepCosine = std::cos(twoPi * phaseStep);
            for (std::size_t sample = offset; sample < offset + chunk; ++sample)
            {
                const int writePosition = (chorusWritePosition + static_cast<int>(sample))
                    & (chorusRingSize - 1);
                for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
                {
                    const float input = buffer.channels[channel][sample];
                    float* ring = chorusDelay.data() + channel * static_cast<std::size_t>(chorusRingSize);
                    ring[writePosition] = input;
                    float readPosition = static_cast<float>(writePosition)
                        - (center + effectiveDepth * (channel == 1 ? cosine : sine));
                    if (readPosition < 0.0F)
                        readPosition += static_cast<float>(chorusRingSize);
                    const int position0 = static_cast<int>(readPosition) & (chorusRingSize - 1);
                    const int position1 = (position0 + 1) & (chorusRingSize - 1);
                    const float fraction = readPosition - static_cast<float>(static_cast<int>(readPosition));
                    const float delayed = ring[position0] * (1.0F - fraction) + ring[position1] * fraction;
                    buffer.channels[channel][sample] = dry * input + wet * delayed;
                }
                const float nextSine = sine * stepCosine + cosine * stepSine;
                cosine = cosine * stepCosine - sine * stepSine;
                sine = nextSine;
            }
            phaseAccumulator += phaseStep * static_cast<float>(chunk);
            offset += chunk;
        }
        chorusWritePosition = (chorusWritePosition + static_cast<int>(buffer.sampleCount))
            & (chorusRingSize - 1);
        chorusPhase = phaseAccumulator - std::floor(phaseAccumulator);
    }

    void updateReverb(const EffectsParameters& parameters, bool force) noexcept
    {
        if (!force && parameters.reverbMix == lastReverbMix
            && parameters.reverbSize == lastReverbSize
            && parameters.reverbDamping == lastReverbDamping)
            return;
        lastReverbMix = parameters.reverbMix;
        lastReverbSize = parameters.reverbSize;
        lastReverbDamping = parameters.reverbDamping;
        juce::dsp::Reverb::Parameters values;
        values.roomSize = parameters.reverbSize;
        values.damping = parameters.reverbDamping;
        values.wetLevel = parameters.reverbMix;
        values.dryLevel = 1.0F - parameters.reverbMix;
        values.width = 1.0F;
        values.freezeMode = 0.0F;
        reverb.setParameters(values);
    }

    void processNoise(AudioBufferView buffer, const EffectsParameters& parameters) noexcept
    {
        const float amount = parameters.noiseAmount * 0.01F;
        if (amount <= 0.0F)
            return;
        for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
        {
            auto& random = noiseRandom[channel];
            auto& pink = noisePink[channel];
            for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
            {
                random ^= random << 13;
                random ^= random >> 17;
                random ^= random << 5;
                const float white = static_cast<float>(static_cast<std::int32_t>(random))
                    * (1.0F / 2147483648.0F);
                pink[0] = 0.99886F * pink[0] + white * 0.0555179F;
                pink[1] = 0.99332F * pink[1] + white * 0.0750759F;
                pink[2] = 0.96900F * pink[2] + white * 0.1538520F;
                pink[3] = 0.86650F * pink[3] + white * 0.3104856F;
                pink[4] = 0.55000F * pink[4] + white * 0.5329522F;
                pink[5] = -0.7616F * pink[5] - white * 0.0168980F;
                const float pinkValue = (pink[0] + pink[1] + pink[2] + pink[3]
                    + pink[4] + pink[5] + white * 0.5362F) * 0.11F;
                const float noise = white + parameters.noiseColor * (pinkValue - white);
                buffer.channels[channel][sample] += noise * amount;
            }
        }
    }

    void processLimiter(AudioBufferView buffer) noexcept
    {
        constexpr float threshold = 0.9F;
        constexpr float knee = 0.1F;
        for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
        {
            float gain = limiterGain[channel];
            for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
            {
                const float input = buffer.channels[channel][sample];
                const float absolute = std::abs(input);
                const float target = absolute > threshold ? threshold / absolute : 1.0F;
                gain = target < gain
                    ? limiterAttack * gain + (1.0F - limiterAttack) * target
                    : limiterRelease * gain + (1.0F - limiterRelease);
                float output = input * gain;
                float outputAbsolute = std::abs(output);
                if (outputAbsolute > threshold)
                {
                    outputAbsolute = threshold + knee * std::tanh((outputAbsolute - threshold) / knee);
                    if (outputAbsolute >= 1.0F)
                        outputAbsolute = 0.9999F;
                    output = std::copysign(outputAbsolute, output);
                }
                buffer.channels[channel][sample] = output;
            }
            limiterGain[channel] = gain;
        }
    }

    void processOutputGain(AudioBufferView buffer, float gainDb, bool muted) noexcept
    {
        const float target = muted ? 0.0F : juce::Decibels::decibelsToGain(gainDb);
        const float increment = buffer.sampleCount > 0
            ? (target - lastFinalGain) / static_cast<float>(buffer.sampleCount)
            : 0.0F;
        float gain = lastFinalGain;
        for (std::size_t sample = 0; sample < buffer.sampleCount; ++sample)
        {
            for (std::size_t channel = 0; channel < buffer.channelCount; ++channel)
                buffer.channels[channel][sample] *= gain;
            gain += increment;
        }
        lastFinalGain = target;
    }

    float sampleRate = 0.0F;
    std::size_t channelCount = 0;
    std::size_t maximumBlockSize = 0;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::StateVariableTPTFilter<float> filterSlope;
    juce::dsp::Reverb reverb;
    juce::dsp::Phaser<float> phaser;
    std::array<BellFilter, 2> bellFilters;
    juce::SmoothedValue<float> filterCutoff;
    juce::SmoothedValue<float> filterResonance;
    juce::SmoothedValue<float> driveAmount;
    juce::SmoothedValue<float> growlAmount;
    juce::SmoothedValue<float> tremoloDepth;
    juce::SmoothedValue<float> tremoloRate;
    juce::SmoothedValue<float> chorusRate;
    juce::SmoothedValue<float> chorusDepth;
    juce::SmoothedValue<float> chorusMix;
    juce::SmoothedValue<float> chorusCenterDelay;
    std::vector<float> chorusDelay;
    std::vector<float> bellScratch;
    float tremoloPhase = 0.0F;
    float chorusPhase = 0.0F;
    int chorusWritePosition = 0;
    float lastPhaserAmount = 0.0F;
    float lastPhaserRate = 0.35F;
    int phaserFadeSamples = 0;
    bool phaserBypassed = true;
    int activeBell = 0;
    int targetBell = 1;
    int bellCrossfadePosition = 0;
    int bellCrossfadeLength = 1;
    bool bellCrossfadeActive = false;
    float lastBellCenter = 0.0F;
    float lastBellMix = 0.0F;
    float lastReverbMix = -1.0F;
    float lastReverbSize = -1.0F;
    float lastReverbDamping = -1.0F;
    std::array<float, 2> growlEnvelope {};
    std::array<float, 2> growlPostBiteLowpass {};
    std::array<float, 2> growlPostBodyLowpass {};
    std::array<float, 2> growlPostBodyHighpass {};
    std::array<float, 2> growlPostEnvelope {};
    int growlStrideCount = 0;
    float growlCutoff = 0.0F;
    bool growlFilterDirty = false;
    std::array<float, 2> limiterGain { 1.0F, 1.0F };
    float limiterAttack = 0.0F;
    float limiterRelease = 0.0F;
    float lastFinalGain = 1.0F;
    std::array<std::uint32_t, 2> noiseRandom { 0x12345678U, 0x87654321U };
    std::array<std::array<float, 6>, 2> noisePink {};
};
} // namespace

std::unique_ptr<EffectsChainImplementation> createEffectsChainImplementation()
{
    return std::make_unique<JuceEffectsChainImplementation>();
}

} // namespace gigante
