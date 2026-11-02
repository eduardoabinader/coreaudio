#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace gigante
{
enum class SessionStatus { inactive, active, interrupted, unavailable, failed };
enum class SessionEventKind
{
    interruptionBegan, interruptionEnded, routeChanged,
    mediaServicesLost, mediaServicesReset, audioFocusChanged
};

// Initial supported intent. Additional platform levers require explicit API work.
struct SessionRequest
{
    bool playback{true};
};

struct SessionError
{
    std::string operation;
    std::string domain;
    std::int64_t code{0};
    std::string message;
};

// Android-only focus facts; absent on iOS. Numeric values retain the native
// AudioManager/AudioAttributes identity without exposing Android headers.
struct SessionAudioFocusReport
{
    int requestedUsage{1};
    int requestedContentType{2};
    int requestedGain{1};
    bool acceptsDelayedGain{false};
    std::optional<int> requestResult;
    std::optional<int> observedChange;
    std::optional<int> abandonResult;
};

struct SessionReport
{
    SessionRequest requested;
    bool categoryApplied{false}; // iOS only; false is not an Android failure.
    bool activationApplied{false};
    // Activation/focus grant is an API outcome, not a running audio device.
    SessionStatus status{SessionStatus::inactive};
    std::string observedCategory;
    std::string observedMode;
    std::optional<SessionError> error;
    std::optional<SessionError> cleanupError;
    std::optional<SessionError> callbackError;
    std::optional<SessionAudioFocusReport> audioFocus{};
};

struct SessionEvent
{
    SessionEventKind kind{SessionEventKind::routeChanged};
    std::optional<bool> shouldResume;
    std::optional<std::uint64_t> routeChangeReason;
    std::optional<int> audioFocusChange{};
};

class SessionPreparation
{
public:
    using Callback = std::function<void(const SessionEvent&)>;
    virtual ~SessionPreparation() = default;

    // Non-realtime only. Serialize prepare/release on the consumer control thread.
    // Events run serially on their native posting thread (never an audio callback).
    // prepare must not be called from an event callback. release is safe there.
    // Repeated prepare while active retains the original callback and request.
    // Callback exceptions are contained and recorded in report().callbackError.
    // iOS permits one prepared instance; the AVAudioSession itself is process-global.
    [[nodiscard]] virtual SessionReport prepare(SessionRequest request,
                                               Callback callback) = 0;
    // Idempotent. External release waits for an in-flight callback; callback-side
    // release does not wait on itself. No subsequent callback after it returns.
    [[nodiscard]] virtual SessionReport release() = 0;
    [[nodiscard]] virtual SessionReport report() const = 0;

    // iOS backend, explicit unavailable result elsewhere. Android requires the
    // context-taking factory in AndroidSessionPreparation.h. No JUCE dependency.
    [[nodiscard]] static std::unique_ptr<SessionPreparation> create();
};
} // namespace gigante
