#include "AndroidAudioFocusDriver.h"
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
class FakeDriver final : public gigante::AndroidAudioFocusDriver
{
public:
    gigante::AndroidFocusOperationResult request(const gigante::SessionAudioFocusReport& options,
                                               Callback callback) override
    {
        ++requests;
        requested = options;
        signal = std::move(callback);
        if (throws) throw std::runtime_error("JNI test failure");
        return requestResult;
    }
    gigante::AndroidFocusOperationResult release() noexcept override
    {
        ++releases;
        return releaseResult;
    }
    gigante::SessionAudioFocusReport requested;
    gigante::AndroidFocusOperationResult requestResult{1, {}};
    gigante::AndroidFocusOperationResult releaseResult{1, {}};
    Callback signal;
    int requests{0};
    int releases{0};
    bool throws{false};
};
struct Fixture
{
    Fixture()
    {
        auto owned = std::make_unique<FakeDriver>();
        driver = owned.get();
        session = gigante::makeAndroidAudioSession(std::move(owned));
    }
    FakeDriver* driver;
    std::unique_ptr<gigante::AudioSession> session;
};
void grantedAndChanges()
{
    using namespace gigante;
    Fixture f;
    int losses = 0;
    int gains = 0;
    int unknown = 0;
    const auto callback = [&](const SessionEvent& event)
    {
        require(event.kind == SessionEventKind::audioFocusChanged, "Android focus disguised as iOS event");
        require(!event.shouldResume && !event.routeChangeReason, "Android invented iOS facts");
        if (*event.audioFocusChange < 0) ++losses;
        else if (*event.audioFocusChange == 1) ++gains;
        else ++unknown;
    };
    auto result = f.session->prepare({}, callback);
    require(result.status == SessionStatus::active && result.activationApplied
            && !result.categoryApplied && result.observedCategory.empty(), "grant reported incorrectly");
    require(result.audioFocus && result.audioFocus->requestResult == 1, "native grant result missing");
    const auto& requested = f.driver->requested;
    require(requested.requestedUsage == 1 && requested.requestedContentType == 2
            && requested.requestedGain == 1 && !requested.acceptsDelayedGain,
            "legacy focus request attributes changed");
    (void) f.session->prepare({}, [](const auto&) {});
    require(f.driver->requests == 1, "repeated prepare requested focus again");
    for (int change : {-1, -2, -3, -99, 1, 0, 42}) f.driver->signal(change);
    require(losses == 4 && gains == 1 && unknown == 2, "raw focus changes lost or guessed");
    require(f.session->report().audioFocus->observedChange == 42
            && f.session->report().status == SessionStatus::interrupted,
            "observation missing or gain asserted recovery");
    const auto old = f.driver->signal;
    result = f.session->release();
    require(result.audioFocus->abandonResult == 1 && result.audioFocus->observedChange == 42,
            "abandon discarded native result or last observation");
    (void) f.session->release();
    require(f.driver->releases == 1, "release not idempotent");
    old(-1);
    require(losses == 4, "late callback after release accepted");
    (void) f.session->prepare({}, callback);
    old(-1);
    require(losses == 4, "old generation accepted after restart");
    (void) f.session->release();
    f.session.reset();
    old(-1);
}
void failures()
{
    using namespace gigante;
    for (int denied : {0, 2})
    {
        Fixture f;
        f.driver->requestResult.nativeResult = denied;
        const auto result = f.session->prepare({}, [](const auto&) {});
        require(result.status == SessionStatus::failed && !result.activationApplied
                && result.error && result.error->code == denied
                && result.audioFocus->requestResult == denied,
                "denied/delayed request misreported");
        require(f.driver->releases == 1, "partial request not cleaned up");
    }
    Fixture missing;
    missing.driver->requestResult = {{}, SessionError{"resolve listener", "java.lang.ClassNotFoundException", 0, "missing listener"}};
    auto report = missing.session->prepare({}, [](const auto&) {});
    require(report.error->domain == "java.lang.ClassNotFoundException", "JNI cause lost");
    Fixture throwing;
    throwing.driver->throws = true;
    require(throwing.session->prepare({}, [](const auto&) {}).error->message == "JNI test failure",
            "throwing driver lost failure cause");
    require(throwing.driver->releases == 1, "throwing request leaked partial state");
    Fixture cleanup;
    cleanup.driver->requestResult = {0, {}};
    cleanup.driver->releaseResult = {{}, SessionError{"abandon", "JNI", 7, "failed cleanup"}};
    report = cleanup.session->prepare({}, [](const auto&) {});
    require(report.error->code == 0 && report.cleanupError->code == 7,
            "cleanup error replaced request denial");
    (void) cleanup.session->prepare({}, [](const auto&) {});
    require(cleanup.driver->requests == 1, "cleanup failure allowed implicit new request");
    cleanup.driver->releaseResult = {1, {}};
    require(cleanup.session->release().status == SessionStatus::inactive, "explicit cleanup retry failed");
    Fixture deniedRelease;
    (void) deniedRelease.session->prepare({}, [](const auto&) {});
    deniedRelease.driver->releaseResult = {0, {}};
    require(deniedRelease.session->release().status == SessionStatus::failed,
            "native abandon failure hidden");
    deniedRelease.driver->releaseResult = {1, {}};
    (void) deniedRelease.session->release();
}
void teardown()
{
    Fixture self;
    int calls = 0;
    (void) self.session->prepare({}, [&](const auto&)
    {
        ++calls;
        (void) self.session->release();
    });
    auto signal = self.driver->signal;
    signal(-1);
    signal(-2);
    require(calls == 1, "callback-side release retained events");
    Fixture destroyed;
    (void) destroyed.session->prepare({}, [&](const auto&) { destroyed.session.reset(); });
    auto late = destroyed.driver->signal;
    late(-1);
    late(-2);
    require(!destroyed.session, "callback-side destruction failed");

    Fixture external;
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool leave = false;
    (void) external.session->prepare({}, [&](const auto&)
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_all();
        cv.wait(lock, [&] { return leave; });
    });
    auto emit = external.driver->signal;
    std::thread event([&] { emit(-1); });
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return entered; });
    }
    std::promise<void> started;
    auto stopped = std::async(std::launch::async, [&]
    {
        started.set_value();
        return external.session->release();
    });
    started.get_future().wait();
    require(stopped.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout,
            "external release returned before callback completed");
    {
        std::lock_guard<std::mutex> lock(mutex);
        leave = true;
    }
    cv.notify_all();
    event.join();
    require(stopped.get().status == gigante::SessionStatus::inactive, "in-flight teardown failed");
    emit(-1);
}
}
int main()
{
    try { grantedAndChanges(); failures(); teardown(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Android session preparation tests passed\n";
}
