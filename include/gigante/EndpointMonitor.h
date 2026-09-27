#pragma once

#include "gigante/EndpointPolicy.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace gigante
{

enum class EndpointChangeStartStatus
{
    started,
    unavailable,
    permissionDenied,
    failed
};

struct EndpointChangeStartResult
{
    EndpointChangeStartStatus status{EndpointChangeStartStatus::failed};
    std::string context;
};

class EndpointChangeSource
{
public:
    using ChangeSignal = std::function<void()>;

    virtual ~EndpointChangeSource() = default;

    // Implementations register the signal but must not invoke it synchronously
    // from start(). Signals are non-realtime notifications that facts changed;
    // EndpointMonitor obtains the complete replacement snapshot.
    [[nodiscard]] virtual EndpointChangeStartResult start(
        ChangeSignal signal) = 0;
    // Must be idempotent and safe when called from the signal callback thread;
    // it must not wait for that same signal invocation to return.
    virtual void stop() noexcept = 0;
};

enum class EndpointMonitorEventCause
{
    initialSnapshot,
    changeSignal
};

enum class EndpointMonitorEventStatus
{
    inventory,
    snapshotFailed
};

struct EndpointMonitorEvent
{
    EndpointMonitorEventCause cause{
        EndpointMonitorEventCause::initialSnapshot};
    EndpointMonitorEventStatus status{
        EndpointMonitorEventStatus::snapshotFailed};
    std::optional<EndpointInventory> inventory;
    std::string context;
};

enum class EndpointMonitorStartStatus
{
    started,
    alreadyRunning,
    invalidCallback,
    changeSourceUnavailable,
    changeSourcePermissionDenied,
    changeSourceFailed
};

struct EndpointMonitorStartResult
{
    bool requestedRunning{true};
    bool effectiveRunning{false};
    EndpointMonitorStartStatus status{
        EndpointMonitorStartStatus::changeSourceFailed};
    std::string context;
};

class EndpointMonitor final
{
public:
    using Callback = std::function<void(const EndpointMonitorEvent&)>;

    // Both injected sources must outlive the monitor. Their platform work and
    // this callback execute only outside realtime audio callbacks.
    EndpointMonitor(EndpointFactSource& factSource,
                    EndpointChangeSource& changeSource);
    ~EndpointMonitor();

    EndpointMonitor(const EndpointMonitor&) = delete;
    EndpointMonitor& operator=(const EndpointMonitor&) = delete;
    EndpointMonitor(EndpointMonitor&&) = delete;
    EndpointMonitor& operator=(EndpointMonitor&&) = delete;

    [[nodiscard]] EndpointMonitorStartResult start(Callback callback);
    void stop() noexcept;
    [[nodiscard]] bool running() const noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation;
};

[[nodiscard]] const char* endpointChangeStartStatusName(
    EndpointChangeStartStatus status) noexcept;
[[nodiscard]] const char* endpointMonitorEventCauseName(
    EndpointMonitorEventCause cause) noexcept;
[[nodiscard]] const char* endpointMonitorEventStatusName(
    EndpointMonitorEventStatus status) noexcept;
[[nodiscard]] const char* endpointMonitorStartStatusName(
    EndpointMonitorStartStatus status) noexcept;

} // namespace gigante
