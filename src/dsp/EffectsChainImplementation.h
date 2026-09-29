#pragma once

#include "gigante/EffectsChain.h"

#include <memory>

namespace gigante
{

class EffectsChainImplementation
{
public:
    virtual ~EffectsChainImplementation() = default;

    virtual bool effectsAvailable() const noexcept = 0;
    virtual void prepare(
        const PrepareSpecification& specification,
        const EffectsParameters& parameters) = 0;
    virtual void reset(const EffectsParameters& parameters) noexcept = 0;
    virtual void release() noexcept = 0;
    virtual void process(
        AudioBufferView buffer,
        const EffectsParameters& parameters,
        const ProcessingContext& context,
        EffectsStageObserver* observer,
        ProcessReport& report) noexcept = 0;
    virtual std::size_t algorithmicLatencySamples() const noexcept = 0;
};

std::unique_ptr<EffectsChainImplementation> createEffectsChainImplementation();

} // namespace gigante
