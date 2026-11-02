#pragma once
#include "SessionPreparationImplementation.h"
#include <stdexcept>
#include <utility>

class FakeSessionPreparationImplementation final
    : public gigante::SessionPreparationImplementation
{
public:
    gigante::SessionReport prepare(gigante::SessionRequest request,
                                   gigante::SessionPreparation::Callback sink) override
    {
        ++prepareCount;
        signal = std::move(sink);
        if (throwOnPrepare) throw std::runtime_error("native prepare failed");
        auto result = prepareResult;
        result.requested = request;
        return result;
    }
    gigante::SessionReport release() noexcept override
    {
        ++releaseCount;
        return releaseResult;
    }
    gigante::SessionReport prepareResult{ {}, true, true,
        gigante::SessionStatus::active, "playback", "default", {}, {}, {} };
    gigante::SessionReport releaseResult;
    gigante::SessionPreparation::Callback signal;
    int prepareCount{0};
    int releaseCount{0};
    bool throwOnPrepare{false};
};
