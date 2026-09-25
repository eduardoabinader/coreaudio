#pragma once

#include "gigante/AudioBufferView.h"

#include <cstddef>
#include <cstdint>

namespace gigante
{

enum class EffectsStage : std::uint8_t
{
    input,
    tremolo,
    growlControl,
    filter,
    growlPostFilter,
    drive,
    bellPeak,
    phaser,
    chorus,
    reverb,
    noise,
    softLimiter,
    outputGain,
    output
};

inline constexpr std::size_t effectsStageCount =
    static_cast<std::size_t>(EffectsStage::output) + 1U;

enum class EffectsSignalPoint : std::uint8_t
{
    input,
    postChain,
    output
};

struct BlockObservation
{
    std::uint64_t firstSamplePosition = 0;
    std::size_t channelCount = 0;
    std::size_t sampleCount = 0;
    std::uint64_t nonFiniteSampleCount = 0;
    std::uint64_t invalidParameterCount = 0;
    std::uint64_t recoveryCount = 0;
    std::size_t algorithmicLatencySamples = 0;
    bool bypassed = true;
};

class EffectsStageObserver
{
public:
    virtual ~EffectsStageObserver() = default;

    virtual void onBlockBegin(const BlockObservation& observation) noexcept = 0;
    virtual void onSignal(
        EffectsSignalPoint,
        AudioBufferReadView) noexcept
    {
    }
    virtual void onStageBegin(EffectsStage stage) noexcept = 0;
    virtual void onStageEnd(EffectsStage stage) noexcept = 0;
    virtual void onBlockEnd(const BlockObservation& observation) noexcept = 0;
    virtual void onReset() noexcept {}
};

} // namespace gigante
