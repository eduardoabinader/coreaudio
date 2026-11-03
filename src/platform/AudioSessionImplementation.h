#pragma once
#include "gigante/AudioSession.h"

namespace gigante
{
// Private injection boundary. stop must remove observers without waiting for an
// event invocation, including when called from that invocation. No synchronous
// event during prepare. Native callback closures must retain only the sink.
class AudioSessionImplementation
{
public:
    virtual ~AudioSessionImplementation() = default;
    virtual SessionReport prepare(SessionRequest request,
                                  AudioSession::Callback sink) = 0;
    virtual SessionReport release() noexcept = 0;
    virtual OutputRoute observeOutputRoute()
    {
        OutputRoute route;
        (void) route.markUnavailable(1, "output route backend is unavailable");
        return route;
    }
    virtual SessionResourceReport requestResource(SessionResource resource)
    { return unsupportedSessionResource(resource, true); }
    virtual SessionResourceReport releaseResource(SessionResource resource)
    { return unsupportedSessionResource(resource, false); }
    virtual SessionResourceReport observeResource(SessionResource resource)
    { return unsupportedSessionResource(resource); }
};
std::unique_ptr<AudioSession> makeAudioSession(
    std::unique_ptr<AudioSessionImplementation> implementation);
#if defined(GIGANTE_IOS_SESSION)
std::unique_ptr<AudioSessionImplementation> makeIosAudioSession();
#endif
} // namespace gigante
