# C3A: existing Android scheduler and JUCE-only comparison

Date: 2026-11-02. Shared REQ-O20/O21, consumer REQ-AM02/AM04.

## Ownership observed in source

Kuaru's SinukaruPianoSynthesizer constructor calls
AndroidPerformanceOptimizer::prepare(). The facade supplies JavaVM/logging policy
to the JUCE-free AndroidAudioThreadScheduler in android_audio_runtime. The first
eligible processBlock callback calls optimizeAudioThread through an atomic guard;
prepareToPlay resets that guard. This is a per-prepare guard, not independent
proof that every replacement native callback thread is identified.

The existing implementation reads TID/policy/nice, attempts maximum FIFO then RR,
then native nice -19 when those requests fail. A process-lifetime detached
PriorityWorker accepts four fixed slots, handles JNI Process urgent-audio fallback
and logs observed scheduling. The publication boolean is currently ignored by
optimizeCurrentThread. Queue saturation, pending TID lifetime/reuse, worker lifetime,
condition-variable wakeup costs and first-use initialization would need an explicit
contract before any shared-mechanism migration. This audit does not claim those
paths meet a new realtime-safety guarantee.

## Existing framework path

The pinned JUCE checkout is 9.0.4. Its Oboe device builder requests LowLatency.
Kuaru's device-ID-aware factory still instantiates JUCE's OboeAudioIODevice; its
device-selection guards are retained in both comparison modes. OpenSLES uses the
existing JUCE implementation independently. Callback scheduling remains an OS/
framework result to observe, not a guarantee inferred from the requested mode.

The iOS branch of Kuaru's compatibility facade is empty; its native audio path
already belongs to JUCE. There is no common Android/iOS optimizer superclass.
The comparison adds no superclass, SessionPreparation scheduling method, native
driver or core scheduling API. SessionPreparation remains a non-realtime resource
mechanism and must not be called from an audio callback to obtain these results.

## C3A experiment boundary

The new Kuaru build selector is legacy (default) or juce. Legacy links the current
scheduler unchanged; juce excludes it and makes the existing facade calls empty.
The callback guard, audio backend, stream policy, DSP, resource ownership and
measurement code are unchanged. Shared code used by AHIO is not removed or
modified. Candidate artifact inspection verifies the custom class/event markers
are absent and lifecycle logs verify the worker is not invoked after reopening.

The consumer ledger records matched optimized profiling builds on the selected
Pixel, native per-track underrun counters, callback-processing snapshots, kernel
policy/RT-priority/nice observations and separate audible feedback. Effective
priority and native counters are independent of the app's timing heuristic.
Short-window timing differences do not establish an improvement, end-to-end
latency or behavior across all devices. Zero unpopulated latency fields do not
mean zero physical latency.

## Decision scope

No measured benefit in a tested pair is not evidence that every Android device
behaves identically. C3A leaves the default legacy path and source intact. A later
approved consumer phase may retire Kuaru's extra scheduling path if its accepted
coverage is sufficient, while preserving other consumers of android_audio_runtime.
Broader hardware/routes/buffers and previously registered Bluetooth policy remain
separate qualification. A new shared scheduler abstraction requires demonstrated
need rather than copying the existing implementation into coreaudio first.

See the consumer docs/devreq/android-thread-scheduling/{comparison,acceptance}.md
for exact protocol, artifact identities, measurements and remaining gaps.


## Observed targeting and comparison outcome — 2026-11-02

On the tested Pixel, both modes/backends had AudioTrack render threads at
SCHED_FIFO/RT priority 2, including replacements after reopen. Legacy Oboe
changed render-thread nice to -19; JUCE-only Oboe and both OpenSLES render
threads were -16. Legacy FIFO/RR attempts were denied; the existing native
policy remained FIFO2. All twelve timed windows had zero native per-track
underrun-counter increases, and sampled callback-processing ranges overlap.
Both JUCE-only backends passed the user's audible idle/Home-return checks.

Legacy OpenSLES recorded a custom scheduling event for main PID/TID 6787,
with nice -10 to -19; render thread 6964 remained nice -16/FIFO2. JUCE-only
OpenSLES main TID 5573 was nice -10. Pinned JUCE's
modules/juce_audio_devices/native/juce_OpenSL_android.cpp, OpenSLSessionT::start,
primes buffers synchronously through doSomeWorkOnAudioThread before starting
the player. A first-processing-call guard is therefore insufficient proof of
render-thread identity. The capture shows mis-targeting risk, not a quantified
UI performance regression. No implementation change to that guard is made here.

Both modes/backends passed ordinary power-button shutdown/fresh launch and
stream reopen checks; candidate logs contain no custom scheduling events.
The consumer restored normal legacy/Oboe dev. C3A qualifies a candidate on
one device/route/buffer, not all Android configurations or shared REQ-O20/O21
as a whole. A separately approved phase must decide retirement/default changes.


## C3B consumer retirement — current architecture

The sections above record the historical C3A audit and experiment. The user
approved Kuaru retirement on 2026-11-02: Performance.h, preparation/callback
calls, the per-prepare first-processing guard and custom source linkage are
removed. The temporary scheduling selector is removed. JUCE and Android own
thread scheduling; the unchanged custom scheduler remains available to other
consumers. Existing session resources and DSP policy are unchanged.

Current class/callback/failure/shutdown SVG/HTML previews describe C3B. See
the consumer retirement.md and retirement-acceptance.md for its own build,
artifact, measured and audible evidence; C3A results alone are not C3B runtime
acceptance and do not establish every-device behavior.

C3B verification on the Pixel repeats six matched windows with zero native
underrun-counter increases. Both render/replacement threads use FIFO/RT2,
nice -16; both backends pass user listening and ordinary lifecycle/restart
checks. The final Oboe dev artifact is restored. Wider device qualification
and the existing Bluetooth/LookAndFeel defects remain deferred; see the
consumer retirement acceptance for metric definitions and temporary artifacts.
