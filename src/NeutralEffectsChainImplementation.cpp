#include "EffectsChainImplementation.h"

namespace gigante
{
namespace
{

class NeutralEffectsChainImplementation final : public EffectsChainImplementation
{
public:
    bool effectsAvailable() const noexcept override { return false; }

    void prepare(
        const PrepareSpecification&,
        const EffectsParameters&) override
    {
    }

    void reset(const EffectsParameters&) noexcept override {}
    void release() noexcept override {}

    void process(
        AudioBufferView,
        const EffectsParameters&,
        const ProcessingContext&,
        EffectsStageObserver*,
        ProcessReport&) noexcept override
    {
    }

    std::size_t algorithmicLatencySamples() const noexcept override { return 0; }
};

} // namespace

std::unique_ptr<EffectsChainImplementation> createEffectsChainImplementation()
{
    return std::make_unique<NeutralEffectsChainImplementation>();
}

} // namespace gigante
