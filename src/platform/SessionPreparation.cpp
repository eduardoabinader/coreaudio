#include "SessionPreparationImplementation.h"

#include <exception>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace gigante
{
namespace
{
struct SessionState
{
    std::recursive_mutex mutex;
    SessionReport report;
    SessionPreparation::Callback callback;
    bool accepting{false};
    std::uint64_t generation{0};
};

class ManagedSession final : public SessionPreparation
{
public:
    explicit ManagedSession(std::unique_ptr<SessionPreparationImplementation> backendIn)
        : backend(std::move(backendIn)), state(std::make_shared<SessionState>()) {}
    ~ManagedSession() override { (void) release(); }

    SessionReport prepare(SessionRequest request, Callback callback) override
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        if (state->accepting)
            return state->report;
        if (needsRelease)
            return state->report; // Explicit release retry is required after failed cleanup.
        state->report = {};
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
            state->report = backend->prepare(request, [weak, generation](const SessionEvent& event)
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
                    callbackCopy(event);
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
        if (!needsRelease) return state->report;
        state->accepting = false;
        ++state->generation;
        state->callback = {};
        const auto requested = state->report.requested;
        const auto observedFocus = state->report.audioFocus
            ? state->report.audioFocus->observedChange : std::optional<int>{};
        state->report = backend->release();
        if (state->report.audioFocus) state->report.audioFocus->observedChange = observedFocus;
        state->report.requested = requested;
        needsRelease = state->report.status == SessionStatus::failed;
        return state->report;
    }

    SessionReport report() const override
    {
        std::lock_guard<std::recursive_mutex> lock(state->mutex);
        return state->report;
    }
private:
    std::unique_ptr<SessionPreparationImplementation> backend;
    std::shared_ptr<SessionState> state;
    bool needsRelease{false};
};

#if !defined(GIGANTE_IOS_SESSION)
class UnsupportedSession final : public SessionPreparationImplementation
{
    SessionReport prepare(SessionRequest request, SessionPreparation::Callback) override
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

std::unique_ptr<SessionPreparation> makeSessionPreparation(
    std::unique_ptr<SessionPreparationImplementation> implementation)
{
    if (!implementation) throw std::invalid_argument("session implementation is null");
    return std::make_unique<ManagedSession>(std::move(implementation));
}

std::unique_ptr<SessionPreparation> SessionPreparation::create()
{
#if defined(GIGANTE_IOS_SESSION)
    return makeSessionPreparation(makeIosSessionPreparation());
#else
    return makeSessionPreparation(std::make_unique<UnsupportedSession>());
#endif
}
} // namespace gigante
