#pragma once
#include "SessionPreparationImplementation.h"
#include "AndroidSessionResourceDriver.h"
#include <functional>
#include <optional>

namespace gigante
{
struct AndroidFocusOperationResult
{
    std::optional<int> nativeResult;
    std::optional<SessionError> error;
};
// Private JNI boundary, injected by desktop tests. Callbacks retain independent
// sinks, never this backend or consumer pointers. No callback synchronously in
// request(). release invalidates tokens without waiting for native callbacks.
class AndroidAudioFocusDriver
{
public:
    using Callback = std::function<void(int)>;
    virtual ~AndroidAudioFocusDriver() = default;
    virtual AndroidFocusOperationResult request(const SessionAudioFocusReport& request,
                                               Callback callback) = 0;
    virtual AndroidFocusOperationResult release() noexcept = 0;
};
std::unique_ptr<SessionPreparation> makeAndroidSessionPreparation(
    std::unique_ptr<AndroidAudioFocusDriver> driver,
    std::unique_ptr<AndroidSessionResourceDriver> resources = {});
} // namespace gigante
