#include "FakeEndpointFactSource.h"

#include "gigante/EndpointMonitor.h"

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

int failureCount = 0;

void expect(const bool condition, const char* const message)
{
    if (condition)
        return;

    ++failureCount;
    std::cerr << "FAIL: " << message << '\n';
}

class FakeEndpointChangeSource final : public gigante::EndpointChangeSource
{
public:
    [[nodiscard]] gigante::EndpointChangeStartResult start(
        ChangeSignal signalIn) override
    {
        ++startCount;
        signal = std::move(signalIn);
        staleSignal = signal;
        active = startResult.status
            == gigante::EndpointChangeStartStatus::started;
        return startResult;
    }

    void stop() noexcept override
    {
        ++stopCount;
        active = false;
        signal = {};
    }

    void emit()
    {
        if (active && signal)
            signal();
    }

    void emitStale()
    {
        if (staleSignal)
            staleSignal();
    }

    gigante::EndpointChangeStartResult startResult{
        gigante::EndpointChangeStartStatus::started, {}};
    int startCount{0};
    std::atomic<int> stopCount{0};

private:
    ChangeSignal signal;
    ChangeSignal staleSignal;
    bool active{false};
};

class ThrowingEndpointFactSource final : public gigante::EndpointFactSource
{
public:
    [[nodiscard]] gigante::EndpointInventory snapshot() const override
    {
        throw std::runtime_error{"inventory read failed: fake source"};
    }
};

gigante::AudioEndpoint makeOutput(std::string endpointId)
{
    gigante::AudioEndpoint endpoint;
    endpoint.endpointId = std::move(endpointId);
    endpoint.direction = gigante::EndpointDirection::output;
    endpoint.bus = gigante::AudioBus::usb;
    endpoint.channelCount = 2U;
    endpoint.sampleRates = {48000.0};
    return endpoint;
}

void testInitialAndChangedSnapshots()
{
    FakeEndpointFactSource facts;
    facts.replace({makeOutput("usb-a")});
    FakeEndpointChangeSource changes;
    gigante::EndpointMonitor monitor{facts, changes};
    std::vector<gigante::EndpointMonitorEvent> events;

    const auto result = monitor.start(
        [&events](const gigante::EndpointMonitorEvent& event)
        {
            events.push_back(event);
        });

    expect(result.requestedRunning,
           "start must retain the requested running state");
    expect(result.effectiveRunning,
           "successful start must report effective running state");
    expect(result.status == gigante::EndpointMonitorStartStatus::started,
           "successful start must have the started status");
    expect(monitor.running(), "monitor must be running after start");
    expect(events.size() == 1U,
           "start must publish exactly one initial snapshot");
    if (!events.empty())
    {
        expect(events[0].cause
                   == gigante::EndpointMonitorEventCause::initialSnapshot,
               "first event must identify the initial snapshot cause");
        expect(events[0].status
                   == gigante::EndpointMonitorEventStatus::inventory,
               "initial snapshot must report inventory status");
        expect(events[0].inventory.has_value()
                   && events[0].inventory->generation == 1U,
               "initial event must retain the inventory generation");
    }

    facts.replace({makeOutput("usb-b")});
    changes.emit();
    expect(events.size() == 2U,
           "one change signal must publish one replacement snapshot");
    if (events.size() >= 2U)
    {
        expect(events[1].cause
                   == gigante::EndpointMonitorEventCause::changeSignal,
               "changed inventory must identify the signal cause");
        expect(events[1].inventory.has_value()
                   && events[1].inventory->generation == 2U,
               "changed inventory must retain its new generation");
    }
}

void testRepeatedStartAndStopAreIdempotent()
{
    FakeEndpointFactSource facts;
    FakeEndpointChangeSource changes;
    gigante::EndpointMonitor monitor{facts, changes};
    int firstCallbackCount = 0;
    int replacementCallbackCount = 0;

    const auto first = monitor.start(
        [&firstCallbackCount](const gigante::EndpointMonitorEvent&)
        {
            ++firstCallbackCount;
        });
    const auto repeated = monitor.start(
        [&replacementCallbackCount](const gigante::EndpointMonitorEvent&)
        {
            ++replacementCallbackCount;
        });

    expect(first.status == gigante::EndpointMonitorStartStatus::started,
           "first start must succeed");
    expect(repeated.status
               == gigante::EndpointMonitorStartStatus::alreadyRunning,
           "repeated start must report alreadyRunning");
    expect(repeated.effectiveRunning,
           "repeated start must report the existing effective state");
    expect(changes.startCount == 1,
           "repeated start must not register the source twice");

    changes.emit();
    expect(firstCallbackCount == 2,
           "repeated start must retain the original callback");
    expect(replacementCallbackCount == 0,
           "repeated start must not replace the active callback implicitly");

    monitor.stop();
    monitor.stop();
    expect(!monitor.running(), "repeated stop must leave the monitor stopped");
    expect(changes.stopCount == 1,
           "repeated stop must unregister the source exactly once");
}

void testInvalidAndFailedStartsPreserveCause()
{
    FakeEndpointFactSource facts;
    FakeEndpointChangeSource changes;
    gigante::EndpointMonitor monitor{facts, changes};

    const auto invalid = monitor.start({});
    expect(invalid.status
               == gigante::EndpointMonitorStartStatus::invalidCallback,
           "empty callback must be a specific terminal start error");
    expect(!invalid.effectiveRunning,
           "invalid callback must not produce an effective running state");
    expect(!invalid.context.empty(),
           "invalid callback must retain diagnostic context");
    expect(changes.startCount == 0,
           "invalid callback must fail before source registration");

    changes.startResult = {
        gigante::EndpointChangeStartStatus::permissionDenied,
        "android.permission.MODIFY_AUDIO_ROUTING denied"};
    const auto denied = monitor.start(
        [](const gigante::EndpointMonitorEvent&) {});
    expect(denied.status
               == gigante::EndpointMonitorStartStatus::changeSourcePermissionDenied,
           "source permission failure must remain specific");
    expect(!denied.effectiveRunning && !monitor.running(),
           "failed source start must leave a terminal stopped state");
    expect(denied.context
               == "android.permission.MODIFY_AUDIO_ROUTING denied",
           "source failure context must be preserved verbatim");
    expect(changes.stopCount == 1,
           "failed registration must execute safe source cleanup");
}

void testSnapshotFailureIsObservable()
{
    ThrowingEndpointFactSource facts;
    FakeEndpointChangeSource changes;
    gigante::EndpointMonitor monitor{facts, changes};
    std::vector<gigante::EndpointMonitorEvent> events;

    const auto result = monitor.start(
        [&events](const gigante::EndpointMonitorEvent& event)
        {
            events.push_back(event);
        });
    expect(result.status == gigante::EndpointMonitorStartStatus::started,
           "snapshot failure must not be confused with registration failure");
    expect(events.size() == 1U,
           "initial snapshot failure must produce one observable event");
    if (!events.empty())
    {
        expect(events[0].status
                   == gigante::EndpointMonitorEventStatus::snapshotFailed,
               "snapshot exception must have a specific event status");
        expect(!events[0].inventory.has_value(),
               "failed snapshot must not expose an effective inventory");
        expect(events[0].context == "inventory read failed: fake source",
               "snapshot exception cause must be preserved verbatim");
    }
}

void testStopWaitsForInFlightCallbackAndDropsLateEvents()
{
    FakeEndpointFactSource facts;
    FakeEndpointChangeSource changes;
    gigante::EndpointMonitor monitor{facts, changes};
    std::mutex gateMutex;
    std::condition_variable gateChanged;
    bool callbackEntered = false;
    bool releaseCallback = false;
    std::atomic<bool> stopReturned{false};
    int callbackCount = 0;

    const auto result = monitor.start(
        [&](const gigante::EndpointMonitorEvent& event)
        {
            ++callbackCount;
            if (event.cause
                != gigante::EndpointMonitorEventCause::changeSignal)
            {
                return;
            }

            std::unique_lock<std::mutex> lock(gateMutex);
            callbackEntered = true;
            gateChanged.notify_all();
            gateChanged.wait(lock, [&releaseCallback] { return releaseCallback; });
        });
    expect(result.status == gigante::EndpointMonitorStartStatus::started,
           "in-flight test monitor must start");

    std::thread eventThread{[&changes] { changes.emit(); }};
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        gateChanged.wait(lock, [&callbackEntered] { return callbackEntered; });
    }

    std::thread stopThread{
        [&]
        {
            monitor.stop();
            stopReturned.store(true, std::memory_order_release);
        }};

    while (changes.stopCount == 0)
        std::this_thread::yield();
    expect(!stopReturned.load(std::memory_order_acquire),
           "stop must wait while a delivered callback remains in flight");

    {
        std::lock_guard<std::mutex> lock(gateMutex);
        releaseCallback = true;
    }
    gateChanged.notify_all();
    eventThread.join();
    stopThread.join();
    expect(stopReturned.load(std::memory_order_acquire),
           "stop must return after the in-flight callback completes");

    const int countAfterStop = callbackCount;
    changes.emitStale();
    expect(callbackCount == countAfterStop,
           "a stale platform signal must not deliver a callback after stop");
}

void testDestructorStopsTheSource()
{
    FakeEndpointFactSource facts;
    FakeEndpointChangeSource changes;
    {
        gigante::EndpointMonitor monitor{facts, changes};
        const auto result = monitor.start(
            [](const gigante::EndpointMonitorEvent&) {});
        expect(result.status == gigante::EndpointMonitorStartStatus::started,
               "destructor test monitor must start");
    }
    expect(changes.stopCount == 1,
           "monitor destructor must stop an active change source exactly once");
    changes.emitStale();
}

void testCallbackCanStopTheMonitor()
{
    FakeEndpointFactSource facts;
    FakeEndpointChangeSource changes;
    gigante::EndpointMonitor monitor{facts, changes};
    int callbackCount = 0;

    const auto result = monitor.start(
        [&](const gigante::EndpointMonitorEvent& event)
        {
            ++callbackCount;
            if (event.cause
                == gigante::EndpointMonitorEventCause::changeSignal)
            {
                monitor.stop();
            }
        });
    expect(result.status == gigante::EndpointMonitorStartStatus::started,
           "reentrant-stop test monitor must start");

    changes.emit();
    expect(!monitor.running(),
           "callback-triggered stop must return in a stopped state");
    expect(changes.stopCount == 1,
           "callback-triggered stop must unregister exactly once");
    expect(callbackCount == 2,
           "callback-triggered stop must retain initial and change events");

    changes.emitStale();
    expect(callbackCount == 2,
           "callback-triggered stop must reject later stale events");
}

void testStableNames()
{
    expect(std::string{gigante::endpointChangeStartStatusName(
               gigante::EndpointChangeStartStatus::permissionDenied)}
               == "permissionDenied",
           "change-source status name must be stable");
    expect(std::string{gigante::endpointMonitorEventCauseName(
               gigante::EndpointMonitorEventCause::changeSignal)}
               == "changeSignal",
           "event cause name must be stable");
    expect(std::string{gigante::endpointMonitorEventStatusName(
               gigante::EndpointMonitorEventStatus::snapshotFailed)}
               == "snapshotFailed",
           "event status name must be stable");
    expect(std::string{gigante::endpointMonitorStartStatusName(
               gigante::EndpointMonitorStartStatus::alreadyRunning)}
               == "alreadyRunning",
           "monitor start status name must be stable");
}

} // namespace

int main()
{
    testInitialAndChangedSnapshots();
    testRepeatedStartAndStopAreIdempotent();
    testInvalidAndFailedStartsPreserveCause();
    testSnapshotFailureIsObservable();
    testStopWaitsForInFlightCallbackAndDropsLateEvents();
    testDestructorStopsTheSource();
    testCallbackCanStopTheMonitor();
    testStableNames();

    if (failureCount != 0)
    {
        std::cerr << failureCount << " endpoint monitor test(s) failed\n";
        return 1;
    }

    std::cout << "endpoint monitor tests passed\n";
    return 0;
}
