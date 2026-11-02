#include "AndroidAudioFocusDriver.h"
#include <stdexcept>
#include <utility>

namespace gigante
{
namespace
{
class AndroidSessionPreparation final : public SessionPreparationImplementation
{
public:
    AndroidSessionPreparation(std::unique_ptr<AndroidAudioFocusDriver> driverIn,
                              std::unique_ptr<AndroidSessionResourceDriver> resourcesIn)
        : driver(std::move(driverIn)), resources(std::move(resourcesIn)) {}
    SessionReport prepare(SessionRequest request, SessionPreparation::Callback sink) override
    {
        current = {};
        current.requested = request;
        current.audioFocus = SessionAudioFocusReport{};
        const auto result = driver->request(*current.audioFocus,
            [sink = std::move(sink)](int change)
            {
                SessionEvent event;
                event.kind = SessionEventKind::audioFocusChanged;
                event.audioFocusChange = change;
                sink(event);
            });
        current.audioFocus->requestResult = result.nativeResult;
        current.error = result.error;
        current.activationApplied = !result.error && result.nativeResult == 1;
        current.status = current.activationApplied ? SessionStatus::active : SessionStatus::failed;
        if (!current.activationApplied && !current.error)
            current.error = SessionError{"requestAudioFocus", "Android.AudioManager",
                result.nativeResult.value_or(-1), "audio focus was not granted (delayed grants are not accepted)"};
        return current;
    }
    SessionReport release() noexcept override
    {
        const auto result = driver->release();
        if (current.audioFocus) current.audioFocus->abandonResult = result.nativeResult;
        current.error = result.error;
        if (!current.error && result.nativeResult && result.nativeResult != 1)
            current.error = SessionError{"abandonAudioFocusRequest", "Android.AudioManager",
                *result.nativeResult, "audio focus abandon was not granted"};
        current.status = current.error ? SessionStatus::failed : SessionStatus::inactive;
        if (!current.error) current.activationApplied = false;
        return current;
    }
    SessionResourceReport requestResource(SessionResource resource) override
    { return resources ? resources->request(resource) : unsupportedSessionResource(resource, true); }
    SessionResourceReport releaseResource(SessionResource resource) override
    { return resources ? resources->release(resource) : unsupportedSessionResource(resource, false); }
    SessionResourceReport observeResource(SessionResource resource) override
    { return resources ? resources->observe(resource) : unsupportedSessionResource(resource); }
private:
    std::unique_ptr<AndroidAudioFocusDriver> driver;
    SessionReport current;
    std::unique_ptr<AndroidSessionResourceDriver> resources;
};
}
std::unique_ptr<SessionPreparation> makeAndroidSessionPreparation(
    std::unique_ptr<AndroidAudioFocusDriver> driver,
    std::unique_ptr<AndroidSessionResourceDriver> resources)
{
    if (!driver) throw std::invalid_argument("Android audio focus driver is null");
    return makeSessionPreparation(std::make_unique<AndroidSessionPreparation>(std::move(driver), std::move(resources)));
}
} // namespace gigante
