#include "AudioSessionImplementation.h"
#import <AVFoundation/AVFoundation.h>

#include <utility>
#include <atomic>
#include <vector>

namespace gigante
{
namespace
{
// AVAudioSession is process-global. Multiple consumers must not deactivate
// one another; the first prepared shared session holds an explicit lease.
std::atomic<bool> sessionClaimed{false};

std::string text(NSString* value)
{
    return value == nil ? std::string{} : std::string{value.UTF8String};
}
void failure(SessionReport& report, const char* operation, NSError* error)
{
    report.status = SessionStatus::failed;
    report.error = SessionError{operation, text(error.domain),
        static_cast<std::int64_t>(error.code), text(error.localizedDescription)};
}
void observe(SessionReport& report)
{
    auto session = [AVAudioSession sharedInstance];
    report.observedCategory = text(session.category);
    report.observedMode = text(session.mode);
}
AudioTransport transport(NSString* type)
{
    if ([type isEqualToString:AVAudioSessionPortBuiltInSpeaker]
        || [type isEqualToString:AVAudioSessionPortBuiltInReceiver]) return AudioTransport::builtIn;
    if ([type isEqualToString:AVAudioSessionPortHeadphones]
        || [type isEqualToString:AVAudioSessionPortLineOut]) return AudioTransport::wired;
    if ([type isEqualToString:AVAudioSessionPortUSBAudio]) return AudioTransport::usb;
    if ([type isEqualToString:AVAudioSessionPortBluetoothA2DP]) return AudioTransport::bluetoothA2dp;
    if ([type isEqualToString:AVAudioSessionPortBluetoothHFP]) return AudioTransport::bluetoothHfp;
    if ([type isEqualToString:AVAudioSessionPortBluetoothLE]) return AudioTransport::bluetoothLe;
    return AudioTransport::unknown;
}
class IosAudioSession final : public AudioSessionImplementation
{
public:
    ~IosAudioSession() override
    {
        (void) release();
        if (ownsClaim) sessionClaimed.store(false);
    }
    SessionReport prepare(SessionRequest request, AudioSession::Callback sink) override
    {
        @autoreleasepool
        {
            report = {};
            report.requested = request;
            bool expected = false;
            if (!sessionClaimed.compare_exchange_strong(expected, true))
            {
                report.status = SessionStatus::failed;
                report.error = SessionError{"prepare", "gigante", 0,
                    "another AudioSession owns the process audio session"};
                return report;
            }
            ownsClaim = true;
            auto session = [AVAudioSession sharedInstance];
            NSError* error = nil;
            if (![session setCategory:AVAudioSessionCategoryPlayback
                mode:AVAudioSessionModeDefault options:0 error:&error])
            {
                failure(report, "setCategory", error);
                observe(report);
                return report;
            }
            report.categoryApplied = true;
            if (![session setActive:YES error:&error])
            {
                failure(report, "setActive", error);
                observe(report);
                return report;
            }
            ownsActivation = true;
            report.activationApplied = true;
            report.status = SessionStatus::active;
            observe(report);
            auto center = [NSNotificationCenter defaultCenter];
            // queue:nil delivers on the native posting thread. The shared wrapper
            // serializes callbacks and rejects stale generations after release.
            for (NSString* name in @[AVAudioSessionInterruptionNotification,
                AVAudioSessionRouteChangeNotification, AVAudioSessionMediaServicesWereLostNotification,
                AVAudioSessionMediaServicesWereResetNotification])
            {
                id token = [center addObserverForName:name object:session queue:nil
                    usingBlock:^(NSNotification* notification)
                {
                    SessionEvent event;
                    if ([notification.name isEqualToString:AVAudioSessionInterruptionNotification])
                    {
                        const auto type = [notification.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
                        event.kind = type == AVAudioSessionInterruptionTypeBegan
                            ? SessionEventKind::interruptionBegan : SessionEventKind::interruptionEnded;
                        NSNumber* options = notification.userInfo[AVAudioSessionInterruptionOptionKey];
                        if (event.kind == SessionEventKind::interruptionEnded && options != nil)
                            event.shouldResume = (options.unsignedIntegerValue & AVAudioSessionInterruptionOptionShouldResume) != 0;
                    }
                    else if ([notification.name isEqualToString:AVAudioSessionRouteChangeNotification])
                    {
                        event.kind = SessionEventKind::routeChanged;
                        NSNumber* reason = notification.userInfo[AVAudioSessionRouteChangeReasonKey];
                        if (reason != nil) event.routeChangeReason = reason.unsignedLongLongValue;
                    }
                    else if ([notification.name isEqualToString:AVAudioSessionMediaServicesWereLostNotification])
                        event.kind = SessionEventKind::mediaServicesLost;
                    else event.kind = SessionEventKind::mediaServicesReset;
                    sink(event);
                }];
                observers.push_back([token retain]);
            }
            return report;
        }
    }
    OutputRoute observeOutputRoute() override
    {
        @autoreleasepool
        {
            OutputRoute route;
            if (!ownsActivation)
            {
                (void) route.markUnavailable(1, "iOS audio session is not activated");
                return route;
            }
            @try
            {
                auto outputs = [AVAudioSession sharedInstance].currentRoute.outputs;
                if (outputs == nil || outputs.count == 0)
                {
                    (void) route.markUnavailable(1, "AVAudioSession currentRoute has no output ports");
                    return route;
                }
                std::vector<AudioLink> links;
                for (AVAudioSessionPortDescription* port in outputs)
                {
                    AudioLink link;
                    link.endpoint.endpointId = text(port.UID);
                    link.endpoint.platformId = link.endpoint.endpointId;
                    link.endpoint.deviceName = text(port.portName);
                    link.endpoint.direction = EndpointDirection::output;
                    link.transport = transport(port.portType);
                    link.nativeTransport = text(port.portType);
                    link.endpoint.bus = link.transport == AudioTransport::builtIn ? AudioBus::builtIn
                        : link.transport == AudioTransport::usb ? AudioBus::usb : AudioBus::unknown;
                    link.availability = AudioLinkAvailability::available;
                    link.selected = true;
                    links.push_back(std::move(link));
                }
                (void) route.updateLinks(1, std::move(links), OutputRouteBasis::effective);
            }
            @catch (NSException* error)
            {
                (void) route.markFailed(1, "AVAudioSession currentRoute: " + text(error.reason));
            }
            return route;
        }
    }
    SessionReport release() noexcept override
    {
        @autoreleasepool
        {
            for (id observer : observers)
            {
                [[NSNotificationCenter defaultCenter] removeObserver:observer];
                [observer release];
            }
            observers.clear();
            if (!ownsActivation)
            {
                if (ownsClaim) sessionClaimed.store(false);
                ownsClaim = false;
                return {};
            }
            report.error.reset();
            NSError* error = nil;
            if (![[AVAudioSession sharedInstance] setActive:NO
                withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:&error])
                failure(report, "deactivate", error);
            else
            {
                ownsActivation = false;
                ownsClaim = false;
                sessionClaimed.store(false);
                report.status = SessionStatus::inactive;
                report.activationApplied = false;
            }
            observe(report);
            return report;
        }
    }
private:
    std::vector<id> observers;
    SessionReport report;
    bool ownsActivation{false};
    bool ownsClaim{false};
};
} // namespace
std::unique_ptr<AudioSessionImplementation> makeIosAudioSession()
{
    return std::make_unique<IosAudioSession>();
}
} // namespace gigante
