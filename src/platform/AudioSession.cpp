#include "AudioSessionImplementation.h"

#include <exception>
#include <mutex>
#include <limits>
#include <stdexcept>
#include <utility>

namespace gigante
{
SessionResourceReports initialSessionResources()
{
    SessionResourceReports reports{};
    reports[1].resource = SessionResource::cpuAwake;
    return reports;
}
SessionResourceReport unsupportedSessionResource(SessionResource resource, std::optional<bool> requestedActive)
{
    SessionResourceReport result;
    result.resource = resource;
    result.requestedActive = requestedActive;
    if (requestedActive) result.operationAccepted = false;
    result.error = SessionError{"sessionResource", "gigante", 0, "resource backend is unavailable"};
    return result;
}
SessionResourceReport AudioSession::requestResource(SessionResource resource)
{ return unsupportedSessionResource(resource, true); }
SessionResourceReport AudioSession::releaseResource(SessionResource resource)
{ return unsupportedSessionResource(resource, false); }
SessionResourceReport AudioSession::observeResource(SessionResource resource)
{ return unsupportedSessionResource(resource); }

OutputRoute AudioSession::observeOutputRoute()
{
    OutputRoute route;
    (void) route.markUnavailable(1, "output route backend is unavailable");
    return route;
}

namespace
{
struct SessionState
{
    std::recursive_mutex mutex;
    SessionReport report;
    AudioSession::Callback callback;
    OutputRoute route;
    bool accepting{false};
    std::uint64_t generation{0};
};

void observeRoute(SessionState& state, AudioSessionImplementation& backend,
                  const char* unavailable = nullptr)
{
    const auto previous = state.route.generation();
    if (previous == std::numeric_limits<std::uint64_t>::max()) return;
    const auto generation = previous + 1;
    if (unavailable != nullptr)
    {
        (void) state.route.markUnavailable(generation, unavailable);
        return;
    }
    try
    {
        const auto observed = backend.observeOutputRoute();
        if (observed.status() == OutputRouteStatus::available)
            (void) state.route.updateLinks(generation, observed.links(), observed.basis());
        else if (observed.status() == OutputRouteStatus::failed)
            (void) state.route.markFailed(generation, observed.context());
        else
            (void) state.route.markUnavailable(generation, observed.context());
    }
    catch (const std::exception& error)
    { (void) state.route.markFailed(generation, error.what()); }
    catch (...)
    { (void) state.route.markFailed(generation, "unknown output route observation exception"); }
}

class ManagedAudioSession final : public AudioSession
{
    enum class ResourceOperation { request, release, observe };
public:
    explicit ManagedAudioSession(std::unique_ptr<AudioSessionImplementation> backendIn)
        : backend(std::move(backendIn)), state(std::make_shared<SessionState>()) {}
    ~ManagedAudioSession() override { (void) release(); }

    SessionReport prepare(SessionRequest request, Callback callback) override
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        if (state->accepting)
            return state->report;
        if (needsRelease)
            return state->report; // Explicit release retry is required after failed cleanup.
        const auto resources = state->report.resources;
        state->report = {};
        state->report.resources = resources;
        state->report.requested = request;
        if (!callback || !request.playback)
        {
            state->report.status = SessionStatus::failed;
            state->report.error = SessionError{"prepare", "gigante", 0,
                "playback intent and a nonempty callback are required"};
            return state->report;
        }
        const auto generation = ++state->generation;
        const std::weak_ptr<SessionState> weak = state;
        try
        {
            state->report = backend->prepare(request, [weak, generation, native = backend.get()](const SessionEvent& event)
            {
                const auto current = weak.lock();
                if (!current) return;
                std::lock_guard<std::recursive_mutex> eventLock(current->mutex);
                if (!current->accepting || current->generation != generation) return;
                if (event.kind == SessionEventKind::interruptionBegan)
                    current->report.status = SessionStatus::interrupted;
                if (event.kind == SessionEventKind::mediaServicesLost
                    || event.kind == SessionEventKind::mediaServicesReset)
                {
                    current->report.status = SessionStatus::unavailable;
                    current->report.activationApplied = false;
                }
                if (event.audioFocusChange && current->report.audioFocus)
                {
                    current->report.audioFocus->observedChange = event.audioFocusChange;
                    if (*event.audioFocusChange < 0)
                        current->report.status = SessionStatus::interrupted;
                }
                // End/gain/reset never assert successful device or session recovery.
                try
                {
                    auto callbackCopy = current->callback;
                    auto delivered = event;
                    if (event.kind == SessionEventKind::routeChanged)
                    {
                        observeRoute(*current, *native,
                            current->report.activationApplied ? nullptr : "audio session activation is unavailable");
                        delivered.outputRoute = current->route;
                    }
                    else if (event.kind == SessionEventKind::mediaServicesLost
                             || event.kind == SessionEventKind::mediaServicesReset)
                    {
                        observeRoute(*current, *native, event.kind == SessionEventKind::mediaServicesLost
                            ? "audio media services lost; activation is unavailable"
                            : "audio media services reset; activation is unavailable");
                        delivered.outputRoute = current->route;
                    }
                    callbackCopy(delivered);
                }
                catch (const std::exception& error)
                {
                    current->report.callbackError = SessionError{
                        "callback", "gigante", 0, error.what()};
                }
                catch (...)
                {
                    current->report.callbackError = SessionError{
                        "callback", "gigante", 0, "unknown callback exception"};
                }
            });
        }
        catch (const std::exception& error)
        {
            state->report.status = SessionStatus::failed;
            state->report.error = SessionError{"prepare", "gigante", 0, error.what()};
        }
        catch (...)
        {
            state->report.status = SessionStatus::failed;
            state->report.error = SessionError{"prepare", "gigante", 0, "unknown backend exception"};
        }
        state->report.resources = resources;
        if (state->report.status == SessionStatus::active)
        {
            state->callback = std::move(callback);
            state->accepting = true;
            needsRelease = true;
        }
        else
        {
            const auto cleanup = backend->release();
            state->report.cleanupError = cleanup.error;
            if (state->report.audioFocus && cleanup.audioFocus)
                state->report.audioFocus->abandonResult = cleanup.audioFocus->abandonResult;
            needsRelease = cleanup.status == SessionStatus::failed;
        }
        return state->report;
    }

    SessionReport release() override
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        // Resource cleanup is independent of focus, including failed/absent focus.
        for (const auto resource : {SessionResource::cpuAwake, SessionResource::backgroundAudio})
        {
            const auto index = resource == SessionResource::cpuAwake ? 1U : 0U;
            if (state->report.resources[index].owned)
                (void) releaseResource(resource);
        }
        observeRoute(*state, *backend, "audio session released");
        if (!needsRelease) return state->report;
        state->accepting = false;
        ++state->generation;
        state->callback = {};
        const auto resources = state->report.resources;
        const auto requested = state->report.requested;
        const auto observedFocus = state->report.audioFocus
            ? state->report.audioFocus->observedChange : std::optional<int>{};
        state->report = backend->release();
        state->report.resources = resources;
        if (state->report.audioFocus) state->report.audioFocus->observedChange = observedFocus;
        state->report.requested = requested;
        needsRelease = state->report.status == SessionStatus::failed;
        return state->report;
    }

    SessionResourceReport requestResource(SessionResource resource) override
    { return resourceOperation(resource, ResourceOperation::request); }
    SessionResourceReport releaseResource(SessionResource resource) override
    { return resourceOperation(resource, ResourceOperation::release); }
    SessionResourceReport observeResource(SessionResource resource) override
    { return resourceOperation(resource, ResourceOperation::observe); }

    OutputRoute observeOutputRoute() override
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        observeRoute(*state, *backend,
            state->accepting && state->report.activationApplied ? nullptr : "audio session is not activated");
        return state->route;
    }

    SessionReport report() const override
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        return state->report;
    }
private:
    SessionResourceReport resourceOperation(SessionResource resource, ResourceOperation operation)
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        if (resource != SessionResource::backgroundAudio && resource != SessionResource::cpuAwake)
            return unsupportedSessionResource(resource);
        auto& previous = state->report.resources[resource == SessionResource::cpuAwake ? 1U : 0U];
        if (operation != ResourceOperation::observe)
        {
            previous.requestedActive = operation == ResourceOperation::request;
            previous.operationAccepted.reset();
        }
        try
        {
            auto result = operation == ResourceOperation::request ? backend->requestResource(resource)
                : operation == ResourceOperation::release ? backend->releaseResource(resource) : backend->observeResource(resource);
            if (operation == ResourceOperation::release && result.error && previous.error)
            {
                result.cleanupError = result.error;
                result.error = previous.error;
            }
            previous = std::move(result);
        }
        catch (const std::exception& error)
        {
            previous.observed = SessionResourceState::failed;
            const SessionError failure{"sessionResource", "gigante", 0, error.what()};
            if (operation == ResourceOperation::release) previous.cleanupError = failure;
            if (!previous.error || operation != ResourceOperation::release) previous.error = failure;
        }
        catch (...)
        {
            previous.observed = SessionResourceState::failed;
            const SessionError failure{"sessionResource", "gigante", 0, "unknown resource exception"};
            if (operation == ResourceOperation::release) previous.cleanupError = failure;
            if (!previous.error || operation != ResourceOperation::release) previous.error = failure;
        }
        return previous;
    }
    std::unique_ptr<AudioSessionImplementation> backend;
    std::shared_ptr<SessionState> state;
    bool needsRelease{false};
};

#if !defined(GIGANTE_IOS_SESSION)
class UnsupportedAudioSession final : public AudioSessionImplementation
{
    SessionReport prepare(SessionRequest request, AudioSession::Callback) override
    {
        SessionReport result;
        result.requested = request;
        result.status = SessionStatus::unavailable;
        result.error = SessionError{"prepare", "gigante", 0,
            "session preparation requires iOS or the explicit Android context-taking factory; other platforms are deferred"};
        return result;
    }
    SessionReport release() noexcept override { return {}; }
};
#endif
} // namespace

std::unique_ptr<AudioSession> makeAudioSession(
    std::unique_ptr<AudioSessionImplementation> implementation)
{
    if (!implementation) throw std::invalid_argument("session implementation is null");
    return std::make_unique<ManagedAudioSession>(std::move(implementation));
}

std::unique_ptr<AudioSession> AudioSession::create()
{
#if defined(GIGANTE_IOS_SESSION)
    return makeAudioSession(makeIosAudioSession());
#else
    return makeAudioSession(std::make_unique<UnsupportedAudioSession>());
#endif
}
} // namespace gigante
