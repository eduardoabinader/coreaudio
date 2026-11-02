#pragma once
#include "gigante/SessionPreparation.h"

namespace gigante
{
// Private injection boundary. stop must remove observers without waiting for an
// event invocation, including when called from that invocation. No synchronous
// event during prepare. Native callback closures must retain only the sink.
class SessionPreparationImplementation
{
public:
    virtual ~SessionPreparationImplementation() = default;
    virtual SessionReport prepare(SessionRequest request,
                                  SessionPreparation::Callback sink) = 0;
    virtual SessionReport release() noexcept = 0;
};
std::unique_ptr<SessionPreparation> makeSessionPreparation(
    std::unique_ptr<SessionPreparationImplementation> implementation);
#if defined(GIGANTE_IOS_SESSION)
std::unique_ptr<SessionPreparationImplementation> makeIosSessionPreparation();
#endif
} // namespace gigante
