#include "FakeSessionPreparationImplementation.h"
#include <atomic>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

namespace
{
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
struct Fixture
{
    Fixture()
    {
        auto owned = std::make_unique<FakeSessionPreparationImplementation>();
        backend = owned.get();
        session = gigante::makeSessionPreparation(std::move(owned));
    }
    FakeSessionPreparationImplementation* backend{};
    std::unique_ptr<gigante::SessionPreparation> session;
};
void lifecycle()
{
    using namespace gigante;
    Fixture f;
    int events = 0;
    expect(f.session->prepare({}, {}).status == SessionStatus::failed, "empty callback rejected");
    expect(f.backend->prepareCount == 0, "invalid request never calls backend");
    expect(f.session->prepare({false}, [](const auto&) {}).status == SessionStatus::failed,
           "unsupported intent rejected");
    auto initial = f.session->prepare({}, [&](const auto&) { ++events; });
    expect(initial.status == SessionStatus::active && initial.activationApplied
           && initial.categoryApplied && initial.observedCategory == "playback",
           "requested/applied/observed preparation report");
    (void) f.session->prepare({}, [](const auto&) { throw std::runtime_error("replaced callback"); });
    expect(f.backend->prepareCount == 1, "repeated prepare is idempotent");
    f.backend->signal({SessionEventKind::interruptionBegan, {}, {}});
    expect(events == 1 && f.session->report().status == SessionStatus::interrupted,
           "original callback retained and interruption recorded");
    f.backend->signal({SessionEventKind::interruptionEnded, false, {}});
    expect(f.session->report().status == SessionStatus::interrupted,
           "interruption end does not assert recovery");
    const auto stale = f.backend->signal;
    (void) f.session->release();
    (void) f.session->release();
    expect(f.backend->releaseCount == 1, "release idempotent");
    stale({});
    expect(events == 2, "late event rejected after release");
    (void) f.session->prepare({}, [&](const auto&) { ++events; });
    stale({});
    expect(events == 2, "previous generation rejected after new prepare");
    f.backend->signal({SessionEventKind::mediaServicesLost, {}, {}});
    expect(!f.session->report().activationApplied
           && f.session->report().status == SessionStatus::unavailable,
           "media services loss invalidates activation");
    f.backend->signal({SessionEventKind::mediaServicesReset, {}, {}});
    expect(f.session->report().status == SessionStatus::unavailable,
           "media services reset does not assert activation");
    (void) f.session->release();
    f.session.reset();
    stale({}); // weak callback remains safe after destruction
}
void failuresAndEvents()
{
    using namespace gigante;
    Fixture f;
    f.backend->prepareResult.status = SessionStatus::failed;
    f.backend->prepareResult.activationApplied = false;
    f.backend->prepareResult.error = SessionError{"activate", "native", 42, "denied"};
    const auto result = f.session->prepare({}, [](const auto&) {});
    expect(result.categoryApplied && !result.activationApplied && result.error
           && result.error->code == 42 && result.error->domain == "native",
           "partial failure and native cause preserved");
    expect(f.backend->releaseCount == 1, "partial prepare failure removes backend observers");
    f.backend->throwOnPrepare = true;
    expect(f.session->prepare({}, [](const auto&) {}).error->message == "native prepare failed",
           "backend exception becomes specific failure report");
    expect(f.backend->releaseCount == 2, "throwing preparation also cleaned up");
    Fixture hints;
    bool resumeObserved = false;
    bool reasonObserved = false;
    (void) hints.session->prepare({}, [&](const SessionEvent& event)
    {
        resumeObserved = resumeObserved || event.shouldResume == true;
        reasonObserved = reasonObserved || event.routeChangeReason == 7;
    });
    hints.backend->signal({SessionEventKind::interruptionEnded, true, {}});
    hints.backend->signal({SessionEventKind::routeChanged, {}, 7});
    expect(resumeObserved && reasonObserved, "resume hint and route reason forwarded unchanged");
    Fixture self;
    int calls = 0;
    (void) self.session->prepare({}, [&](const auto&)
    {
        ++calls;
        (void) self.session->release();
    });
    self.backend->signal({});
    self.backend->signal({});
    expect(calls == 1 && self.backend->releaseCount == 1,
           "callback-side release cannot deadlock or deliver another event");
}
void cleanupFailuresAndDestruction()
{
    using namespace gigante;
    Fixture f;
    (void) f.session->prepare({}, [](const auto&) {});
    f.backend->releaseResult.status = SessionStatus::failed;
    f.backend->releaseResult.error = SessionError{"deactivate", "native", 9, "busy"};
    expect(f.session->release().error->code == 9, "deactivation failure remains explicit");
    (void) f.session->prepare({}, [](const auto&) {});
    expect(f.backend->prepareCount == 1, "failed release prevents implicit reactivation");
    f.backend->releaseResult = {};
    expect(f.session->release().status == SessionStatus::inactive, "explicit release retry succeeds");
    Fixture cleanup;
    cleanup.backend->prepareResult.status = SessionStatus::failed;
    cleanup.backend->prepareResult.error = SessionError{"activate", "native", 1, "primary"};
    cleanup.backend->releaseResult.status = SessionStatus::failed;
    cleanup.backend->releaseResult.error = SessionError{"deactivate", "native", 2, "cleanup"};
    const auto failed = cleanup.session->prepare({}, [](const auto&) {});
    expect(failed.error->code == 1 && failed.cleanupError->code == 2,
           "cleanup failure does not hide original preparation error");
    cleanup.backend->releaseResult = {};
    (void) cleanup.session->release();
    Fixture throwing;
    (void) throwing.session->prepare({}, [](const auto&) { throw std::runtime_error("consumer failed"); });
    throwing.backend->signal({});
    expect(throwing.session->report().callbackError->message == "consumer failed",
           "callback exception is contained and observable");
    Fixture destroyed;
    int events = 0;
    (void) destroyed.session->prepare({}, [&](const auto&)
    {
        ++events;
        destroyed.session.reset();
    });
    auto late = destroyed.backend->signal;
    late({});
    late({});
    expect(events == 1 && !destroyed.session, "callback-side destruction invalidates late events");
}
void externalRelease()
{
    Fixture f;
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false;
    bool finish = false;
    (void) f.session->prepare({}, [&](const auto&)
    {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        cv.notify_all();
        cv.wait(lock, [&] { return finish; });
    });
    auto signal = f.backend->signal;
    std::thread event([&] { signal({}); });
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return entered; });
    }
    std::promise<void> started;
    auto stopped = std::async(std::launch::async, [&]
    {
        started.set_value();
        return f.session->release();
    });
    started.get_future().wait();
    expect(stopped.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout,
           "external release waits for in-flight callback");
    {
        std::lock_guard<std::mutex> lock(mutex);
        finish = true;
    }
    cv.notify_all();
    event.join();
    expect(stopped.get().status == gigante::SessionStatus::inactive,
           "external release completes after callback exits");
    signal({});
}
}
int main()
{
    lifecycle();
    failuresAndEvents();
    externalRelease();
    cleanupFailuresAndDestruction();
#if !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    auto unsupported = gigante::SessionPreparation::create();
    expect(unsupported->prepare({}, [](const auto&) {}).status
           == gigante::SessionStatus::unavailable, "desktop never reports successful no-op");
#endif
    return failures == 0 ? 0 : 1;
}
