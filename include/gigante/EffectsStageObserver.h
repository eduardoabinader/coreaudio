#pragma once

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

struct BlockObservation
{
    std::uint64_t firstSamplePosition = 0;
    std::size_t channelCount = 0;
    std::size_t sampleCount = 0;
    std::uint64_t nonFiniteSampleCount = 0;
    std::size_t algorithmicLatencySamples = 0;
    bool bypassed = true;
};

class EffectsStageObserver
{
public:
    virtual ~EffectsStageObserver() = default;

    virtual void onBlockBegin(const BlockObservation& observation) noexcept = 0;
    virtual void onStageBegin(EffectsStage stage) noexcept = 0;
    virtual void onStageEnd(EffectsStage stage) noexcept = 0;
    virtual void onBlockEnd(const BlockObservation& observation) noexcept = 0;
};

} // namespace gigante
