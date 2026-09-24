#pragma once

#include "gigante/AudioBufferView.h"
#include "gigante/EffectsStageObserver.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace gigante
{

enum class LifecycleState : std::uint8_t
{
    unprepared,
    prepared
};

enum class ErrorCode : std::uint8_t
{
    none,
    invalidSampleRate,
    invalidChannelCount,
    invalidMaximumBlockSize,
    notPrepared,
    nullChannelArray,
    nullChannelData,
    channelCountMismatch,
    blockExceedsMaximum,
    effectsNotImplemented
};

struct Status
{
    ErrorCode code = ErrorCode::none;
    LifecycleState state = LifecycleState::unprepared;
    std::size_t channelIndex = 0;
    std::size_t requestedValue = 0;
    std::size_t effectiveValue = 0;

    constexpr bool ok() const noexcept
    {
        return code == ErrorCode::none;
    }
};

struct PrepareSpecification
{
    double sampleRate = 0.0;
    std::size_t channelCount = 0;
    std::size_t maximumBlockSize = 0;
};

struct EffectiveConfiguration
{
    double sampleRate = 0.0;
    std::size_t channelCount = 0;
    std::size_t maximumBlockSize = 0;
};

enum class ProcessingMode : std::uint8_t
{
    neutralBypass,
    effects
};

struct EffectsParameters
{
    float tremoloDepth = 0.0F;
    float tremoloRateHz = 0.0F;
    float growlAmount = 0.0F;
    float growlAttackMs = 5.0F;
    float growlReleaseMs = 150.0F;
    float filterFrequencyHz = 1000.0F;
    float filterResonance = 0.2F;
    float driveAmount = 0.0F;
    float bellCenterHz = 2800.0F;
    float bellMix = 0.0F;
    float phaserAmount = 0.0F;
    float phaserRateHz = 0.35F;
    float chorusDepth = 0.0F;
    float chorusRateHz = 0.3F;
    float chorusMix = 0.0F;
    float chorusCenterDelayMs = 15.0F;
    float reverbMix = 0.0F;
    float reverbSize = 0.5F;
    float reverbDamping = 0.5F;
    float noiseAmount = 0.0F;
    float noiseColor = 1.0F;
    float outputGainDb = 0.0F;
};

struct ProcessingContext
{
    ProcessingMode mode = ProcessingMode::neutralBypass;
    std::uint64_t firstSamplePosition = 0;
    float sourceLevel = 1.0F;
    bool generatedNoiseEnabled = false;
    bool outputMuted = false;
};

struct ProcessReport
{
    Status status {};
    std::uint64_t nonFiniteSampleCount = 0;
    std::uint64_t invalidParameterCount = 0;
    std::uint64_t recoveryCount = 0;
    std::size_t algorithmicLatencySamples = 0;
    bool bypassed = true;
};

class EffectsChainImplementation;

class EffectsChain final
{
public:
    EffectsChain();
    ~EffectsChain();

    EffectsChain(const EffectsChain&) = delete;
    EffectsChain& operator=(const EffectsChain&) = delete;
    EffectsChain(EffectsChain&&) = delete;
    EffectsChain& operator=(EffectsChain&&) = delete;

    Status prepare(const PrepareSpecification& requested) noexcept;
    Status prepare(
        const PrepareSpecification& requested,
        const EffectsParameters& initialParameters) noexcept;
    void reset() noexcept;
    void reset(const EffectsParameters& parameters) noexcept;
    void release() noexcept;

    ProcessReport process(
        AudioBufferView buffer,
        const EffectsParameters& parameters,
        const ProcessingContext& context,
        EffectsStageObserver* observer = nullptr) noexcept;

    LifecycleState lifecycleState() const noexcept;
    const PrepareSpecification& requestedConfiguration() const noexcept;
    const EffectiveConfiguration& effectiveConfiguration() const noexcept;
    const EffectsParameters& requestedParameters() const noexcept;
    const EffectsParameters& effectiveParameters() const noexcept;
    std::size_t algorithmicLatencySamples() const noexcept;
    std::uint64_t totalNonFiniteSampleCount() const noexcept;
    std::uint64_t totalInvalidParameterCount() const noexcept;
    std::uint64_t totalRecoveryCount() const noexcept;
    bool effectsAvailable() const noexcept;

private:
    Status status(ErrorCode code) const noexcept;
    EffectsParameters resolveParameters(
        const EffectsParameters& requested,
        std::uint64_t& invalidParameterCount) noexcept;

    PrepareSpecification requestedConfigurationValue {};
    EffectiveConfiguration effectiveConfigurationValue {};
    EffectsParameters requestedParameterValues {};
    EffectsParameters effectiveParameterValues {};
    LifecycleState currentState = LifecycleState::unprepared;
    std::uint64_t nonFiniteSampleTotal = 0;
    std::uint64_t invalidParameterTotal = 0;
    std::uint64_t recoveryTotal = 0;
    std::unique_ptr<EffectsChainImplementation> implementation;
};

} // namespace gigante
