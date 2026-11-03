#include "AndroidAudioFocusDriver.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace gigante;
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
class Focus final : public AndroidAudioFocusDriver
{
public:
    explicit Focus(std::vector<int>& traceIn) : trace(traceIn) {}
    AndroidFocusOperationResult request(const SessionAudioFocusReport&, Callback) override { return {1, {}}; }
    AndroidFocusOperationResult release() noexcept override { trace.push_back(3); return {1, {}}; }
    std::vector<int>& trace;
};
class Resources final : public AndroidSessionResourceDriver
{
public:
    explicit Resources(std::vector<int>& traceIn) : trace(traceIn) {}
    SessionResourceReport request(SessionResource resource) override
    {
        if (throws) throw std::runtime_error("resource request exception");
        auto& value = reports[resource == SessionResource::cpuAwake ? 1U : 0U];
        value.requestedActive = true;
        value.operationAccepted = true;
        value.supported = true;
        value.owned = true;
        if (partialFailure) value.error = SessionError{"request", "fake.native", 9, "partial request failure"};
        value.observed = resource == SessionResource::cpuAwake ? SessionResourceState::active : SessionResourceState::pending;
        return value;
    }
    SessionResourceReport release(SessionResource resource) override
    {
        trace.push_back(resource == SessionResource::cpuAwake ? 1 : 2);
        auto& value = reports[resource == SessionResource::cpuAwake ? 1U : 0U];
        value.requestedActive = false;
        value.operationAccepted = !failRelease;
        if (failRelease) value.error = SessionError{"release", "fake.native", 7, "cleanup denied"};
        else
        {
            value.error.reset();
            value.observed = resource == SessionResource::cpuAwake ? SessionResourceState::inactive : SessionResourceState::stopPending;
            value.owned = resource != SessionResource::cpuAwake;
        }
        return value;
    }
    SessionResourceReport observe(SessionResource resource) override
    { return reports[resource == SessionResource::cpuAwake ? 1U : 0U]; }
    SessionResourceReports reports{initialSessionResources()};
    std::vector<int>& trace;
    bool throws{false};
    bool failRelease{false};
    bool partialFailure{false};
};
void lifecycle()
{
    std::vector<int> trace;
    auto resources = std::make_unique<Resources>(trace);
    auto* native = resources.get();
    auto session = makeAndroidAudioSession(std::make_unique<Focus>(trace), std::move(resources));
    require(session->requestResource(SessionResource::backgroundAudio).observed == SessionResourceState::pending,
            "accepted service start invented running state");
    require(session->requestResource(SessionResource::cpuAwake).observed == SessionResourceState::active,
            "wake observation lost");
    auto report = session->prepare({}, [](const auto&) {});
    require(report.status == SessionStatus::active && report.resources[0].owned && report.resources[1].owned,
            "focus preparation lost independent resources");
    report = session->release();
    require(trace == std::vector<int>({1, 2, 3}), "cleanup order changed or independent resource not released");
    require(report.resources[0].observed == SessionResourceState::stopPending && report.resources[0].owned,
            "stop acceptance reported as observed stopped");
    native->reports[0].observed = SessionResourceState::inactive;
    native->reports[0].owned = false;
    require(!session->observeResource(SessionResource::backgroundAudio).owned, "observation failed to refresh pending stop");
    trace.clear();
    (void) session->release();
    require(trace.empty(), "released resources or focus released twice");
}
void independentFailure()
{
    std::vector<int> trace;
    auto resources = std::make_unique<Resources>(trace);
    auto* native = resources.get();
    auto session = makeAndroidAudioSession(std::make_unique<Focus>(trace), std::move(resources));
    (void) session->prepare({}, [](const auto&) {});
    (void) session->requestResource(SessionResource::cpuAwake);
    native->throws = true;
    auto report = session->requestResource(SessionResource::backgroundAudio);
    require(report.error && report.error->message == "resource request exception", "resource exception escaped or lost cause");
    require(session->report().activationApplied && session->report().resources[1].owned && trace.empty(),
            "resource failure silently released focus or wake lock");
    native->throws = false;
    native->failRelease = true;
    report = session->releaseResource(SessionResource::cpuAwake);
    require(report.owned && report.error && report.error->code == 7, "cleanup failure lost ownership/cause");
    native->failRelease = false;
    report = session->releaseResource(SessionResource::cpuAwake);
    require(!report.owned && !report.error, "explicit cleanup retry failed");
    // Externally active observations are not session ownership or teardown work.
    native->reports[0].owned = false;
    native->reports[0].observed = SessionResourceState::active;
    (void) session->observeResource(SessionResource::backgroundAudio);
    trace.clear();
    (void) session->release();
    require(trace == std::vector<int>({3}), "session stopped an externally owned resource");
}
void partialFailureCause()
{
    std::vector<int> trace;
    auto resources = std::make_unique<Resources>(trace);
    auto* native = resources.get();
    auto session = makeAndroidAudioSession(std::make_unique<Focus>(trace), std::move(resources));
    native->partialFailure = true;
    auto report = session->requestResource(SessionResource::cpuAwake);
    require(report.owned && report.error->code == 9, "partial ownership or request cause lost");
    native->failRelease = true;
    report = session->releaseResource(SessionResource::cpuAwake);
    require(report.error->code == 9 && report.cleanupError && report.cleanupError->code == 7,
            "cleanup failure replaced the partial request cause");
    native->failRelease = false;
    require(!session->releaseResource(SessionResource::cpuAwake).owned, "partial cleanup retry leaked ownership");
}
void unsupportedAndDestruction()
{
    auto unsupported = AudioSession::create();
    auto report = unsupported->requestResource(SessionResource::cpuAwake);
    require(!report.supported && !report.owned && report.error
            && report.observed == SessionResourceState::unavailable, "unsupported resource reported success");
    require(report.requestedActive == true && report.operationAccepted == false, "unsupported request lost requested/accepted facts");
    std::vector<int> trace;
    {
        auto session = makeAndroidAudioSession(std::make_unique<Focus>(trace), std::make_unique<Resources>(trace));
        (void) session->requestResource(SessionResource::cpuAwake);
    }
    require(trace == std::vector<int>({1}), "resource-only session destructor leaked or abandoned unowned focus");
}
}
int main()
{
    try { lifecycle(); independentFailure(); partialFailureCause(); unsupportedAndDestruction(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Android resource contract tests passed\n";
}
