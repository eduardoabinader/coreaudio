#pragma once
#include "AudioSessionImplementation.h"
#include <stdexcept>
#include <utility>

class FakeAudioSessionImplementation final
    : public gigante::AudioSessionImplementation
{
public:
    gigante::SessionReport prepare(gigante::SessionRequest request,
                                   gigante::AudioSession::Callback sink) override
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
    gigante::OutputRoute observeOutputRoute() override
    {
        ++routeQueries;
        if (throwOnObserve) throw std::runtime_error("native route query failed");
        return route;
    }
    gigante::OutputRoute route;
    int routeQueries{0};
    bool throwOnObserve{false};
    gigante::SessionReport prepareResult{ {}, true, true,
        gigante::SessionStatus::active, "playback", "default", {}, {}, {} };
    gigante::SessionReport releaseResult;
    gigante::AudioSession::Callback signal;
    int prepareCount{0};
    int releaseCount{0};
    bool throwOnPrepare{false};
};
