# Core Audio Requirements

## Authority and status model

This repository is the authoritative implementation of AHIO Group N and of the
product-neutral portions of Groups O and P. Their stable IDs are retained so
Kuaru, AHIO, phases, and tests refer to one contract. Normative wording shared
with AHIO is kept compatible; repository-local status and evidence remain in
the separate columns and do not imply AHIO adoption.

Each requirement tracks three independent states:

- **Specified**: the normative contract is approved.
- **Implemented**: repository or consumer code implements the contract.
- **Verified**: named acceptance evidence demonstrates the behavior.

A build can support **Implemented**, but cannot by itself establish
**Verified**. Requirement IDs are never renumbered or reused.

## N.1 — Ownership, repository, and naming

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N01 | Shared realtime audio lives in the independent `coreaudio` repository and contains cohesive audio processing, realtime observation, and deterministic test support—not general product utilities. | Yes | Yes | Yes | P8G core chain plus standalone/core and Kuaru golden verification |
| REQ-N02 | Kuaru and AHIO consume an exact pinned revision of the same authoritative core; copies, translations, and product forks are prohibited. | Yes | Partial | No | Kuaru records coreaudio `8ee15a7`, while its working tree currently checks out `236630a`; the next approved core change must restore an exact gitlink before clean-checkout verification. P8I AHIO pin remains |
| REQ-N03 | Public C++ code in the core shall use the single namespace `gigante`; common types describe their responsibility without repeating `Gigante` in every name. | Yes | Yes | Yes | P8F public-header isolation build and review |
| REQ-N04 | Controlled types use `UpperCamelCase`; functions, variables, members, and constants use `lowerCamelCase`; no controlled symbol begins with `_` or gains snake-case aliases. Externally imposed ABI names stay in adapters. | Yes | Yes | Yes | P8F naming review, strict build |
| REQ-N05 | No new module, class, target, configuration, or controlled symbol uses `Sinu`. Shared names use neutral ownership; piano-only names remain Kuaru-owned and AHIO-only names remain Ahio-owned. Existing names migrate by ownership in reviewable phases. | Yes | Partial | Partial | P8F core audit passed; consumer migration remains P8G |
| REQ-N06 | The core depends on no product class/configuration, MIDI, voice, instrument, device, platform lifecycle, codec, socket, encoder/decoder role, transport, or UI. Product/platform adapters depend on the core only. | Yes | Yes | Yes | P8F JUCE-free target and header-isolation build |
| REQ-N07 | Kuaru consumes only common processing and observation APIs; AHIO packet, link, endpoint, network-clock, encoder/decoder, and transport lifecycle types never enter Kuaru through this extraction. | Yes | Yes | Yes | P8G source/dependency audit |

## N.2 — Processing and product adapters

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N08 | The core exposes `prepare`, `reset`, and `process` over granted sample rate, channels, and maximum block size, consuming PCM plus immutable neutral parameter/context snapshots. It is not a synthesizer, device manager, or transport session. | Yes | Yes | Yes | `giganteCoreTests`: lifecycle, validation, reconfigure, PCM views |
| REQ-N09 | Initial extraction preserves Kuaru's chain order: tremolo, growl control, filter, growl post-filter, drive, bell peak, phaser, chorus, reverb, noise, soft limiter, and output gain. Algorithms and defaults are not redesigned during extraction. | Yes | Yes | Yes | P8G five-vector bit-exact parity |
| REQ-N10 | Core parameter snapshots are plain neutral values and know no product enum, JUCE parameter ownership, JSON parser, or stored product identifier. Each product owns its adapter. | Yes | Yes | Yes | P8G Kuaru adapter and core dependency audit; AHIO adapter remains P8I |
| REQ-N11 | Kuaru and AHIO own independent configuration and ranges. Kuaru configuration migration requires an explicit compatibility plan; AHIO owns `ahio.json`. Shared algorithms do not imply shared defaults, guards, presets, or calibrated levels. | Yes | No | No | Consumer integration evidence |
| REQ-N12 | AHIO's first DSP release has one fixed topology/current parameter set; scenes, dynamic graph ordering, and send buses remain outside initial scope. | Yes | No | No | P8J AHIO evidence |
| REQ-N13 | AHIO source conditioning remains outside the core and before lossy encoding, owning input mapping, trim, metering, and capture/codec protection. | Yes | No | No | P8I/P8J AHIO boundary audit |
| REQ-N14 | Musical preamp colour and shared effects may run after decoding; endpoint correction, volume, and protection remain local output policy. USB preserves this boundary without pretending PCM is compressed. | Yes | No | No | P8I/P8J AHIO evidence |
| REQ-N15 | Kuaru keeps voices, MIDI, packs, instrument calibration, active-voice count, polyphony headroom, `sqrt(2/N)` scaling, instrument switching, and headset policy outside the core. | Yes | Yes | Yes | P8G boundary audit and bit-exact Kuaru vectors |
| REQ-N16 | AHIO keeps device discovery/selection, routes, lifecycle, USB/SCO/Wi-Fi transport, codec, packets, jitter, clocks, group synchronization, and endpoint policy outside the core. | Yes | No | No | P8I boundary audit |
| REQ-N17 | Kuaru drive/growl level assumptions become explicit neutral source-level context or adapter output. AHIO uses independently calibrated voice/input values and never silently inherits a piano-pack reference. | Yes | Partial | Partial | P8G explicit `sourceLevel` verified; AHIO calibration remains P8J |
| REQ-N18 | Generated noise may live in the core, but enable policy does not depend on Kuaru active-voice concepts inside it. Kuaru supplies effective policy; AHIO initially disables generated noise. | Yes | Partial | Partial | P8G explicit Kuaru policy and bit parity; AHIO policy remains P8I/P8J |
| REQ-N19 | Supported layouts and mono/stereo conversions are explicit. Core/adapters never silently discard, merge, or duplicate channels; an initial mono-only AHIO route is declared and tested. | Yes | Partial | Partial | P8F core rejects layouts other than explicit mono/stereo; adapter/AHIO evidence remains |
| REQ-N20 | Neutral/bypass preserves sample order, channel identity, and unity gain within documented tolerance while retaining lifecycle, continuity, and observation evidence. | Yes | Yes | Yes | P8F bit-exact mono/stereo variable-block tests |

## N.3 — Realtime, latency, and observability

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N21 | Core processing/observers perform no allocation, lock, blocking call, device/network/file I/O, log formatting, or mutable configuration parsing on an audio callback. Publication uses bounded realtime-safe hand-off. | Yes | Yes | Yes | P8H fixed SPSC publication, lock-free index assertion, source audit, deterministic queue-overflow test |
| REQ-N22 | The core adds no FIFO, whole-block accumulation, callback-size conversion, thread hop, or full-buffer copy merely for abstraction. Any unavoidable conversion has measured ownership, cost, and latency. | Yes | Yes | Yes | P8F non-owning synchronous PCM implementation and source audit |
| REQ-N23 | The core reports algorithmic latency in samples. Bypass/no-look-ahead stages report zero; delay/tail and transport/device buffering remain distinct evidence. | Yes | Yes | Yes | P8G production and bypass report zero samples; no hidden adaptation |
| REQ-N24 | Smoothing, delay lines, envelopes, filter/limiter memory, and deterministic noise state belong to the core. `prepare`/`reset` initialize them deterministically; live changes preserve current Kuaru smoothing until separately approved. | Yes | Yes | Yes | P8G state relocation and live-change bit-exact vectors at blocks 7/512 |
| REQ-N25 | Non-finite samples, invalid coefficients, recovery resets, and safety activation follow explicit bounded policies. The callback records counters/state only; explanatory/persistent logs occur outside it. | Yes | Yes | Yes | P8G/P8H counters and invalid golden; allocation-instrumented neutral and JUCE production tests report zero allocations across 32 prepared blocks |
| REQ-N26 | The core exposes neutral per-stage observation through an `EffectsStageObserver`-equivalent contract and depends on neither `ProfileCollector` nor AHIO metrics. | Yes | Yes | Yes | P8F observer callbacks/tests and dependency audit |
| REQ-N27 | Kuaru adapts observation to `ProfileCollector`; AHIO adapts it to AHIO metrics/reports. A no-op observer changes neither processing behavior nor product conditionals in the core. | Yes | Partial | Partial | P8H Kuaru stage adapter/null disabled path and five-vector bit parity; P8I AHIO adapter remains |
| REQ-N28 | Observation is preallocated/bounded and uses no owning `std::function`, allocating dispatch, formatting, or wall-clock conversion on the callback. Disabled observation has measured negligible overhead. | Yes | Yes | Yes | P8H fixed storage/raw-tick audit; 10-run release medians versus pre-P8H were +2.51% at block 7 and +0.38% at block 512, under the reviewed 5% budget |
| REQ-N29 | Stable snapshots expose input/post-chain/output peak and RMS, per-stage calls/time, maximum callback contribution, budget exceedance, algorithmic latency, resets, and non-finite counts. Products retain requested/effective configuration evidence. | Yes | Yes | Yes | P8H fake-clock level/stage/budget/reset/recovery/overflow tests; existing requested/effective API tests |
| REQ-N30 | Debug WAV, JSON sidecars, console, and report-file writing stay outside the realtime core. A future shared tap is a separate bounded target, never hidden chain I/O. | Yes | Yes | Yes | P8H core include/dependency/I/O source audit |

## N.4 — Migration and acceptance

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N31 | Before moving an effect, capture deterministic Kuaru reference vectors for representative presets, transients, sustain, tails, live changes, reset, and invalid recovery; record all random seeds. | Yes | Yes | Yes | P8F reviewed float32 goldens, manifest, two complete identical captures |
| REQ-N32 | Migrate Kuaru alone first. AHIO waits for bit equivalence where practical and reviewed tolerance elsewhere, with no unexplained stage-budget regression. | Yes | Yes | Yes | P8G five-vector bit parity plus 15-pair controlled render budget: median delta +2.92% at block 7 and +1.59% at block 512, within reviewed 5% tolerance |
| REQ-N33 | Tests cover supported rates, mono/stereo, fixed/varying/small blocks, bypass, smoothing, tails, reset, malformed parameters, non-finite recovery, deterministic metrics, and repeated prepare/reconfigure. | Yes | Yes | Yes | P8H core suite plus five reviewed bit-exact Kuaru vectors and reset repeat |
| REQ-N34 | AHIO first consumes the verified core on USB/JUCE in neutral bypass and compares audio, conservation, added buffering, xruns, CPU, and physical latency with the existing USB baseline. | Yes | No | No | P8I AHIO baseline report |
| REQ-N35 | AHIO enables effects incrementally only after N34, using voice/input-calibrated `ahio.json` values rather than piano values. Wi-Fi use waits for Kuaru parity and USB adoption. | Yes | No | No | P8J then Wi-Fi gates |

## N.5 — Deferred vocoder shared by Kuaru and AHIO

These requirements remain specified but explicitly deferred. They are not part
of P8F–P8J and do not authorize implementation.

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N36 | A future optional vocoder is a separate cohesive processor and does not delay or widen initial core extraction/USB gates. | Yes | No | No | Deferred |
| REQ-N37 | Captured voice is modulator; the first carrier is locally generated and requires no piano voices, packs, entitlements, or network carrier stream. | Yes | No | No | Deferred |
| REQ-N38 | The vocoder accepts explicit modulator/carrier PCM views and an output view; it does not hide its second input or distort the one-input `EffectsChain` API. | Yes | No | No | Deferred |
| REQ-N39 | The first carrier is deterministic, realtime-safe, harmonically rich, and may add bounded noise; tracking, external, sample, and piano carriers are separate decisions. | Yes | No | No | Deferred |
| REQ-N40 | Fixed topology is conditioned voice plus carrier to vocoder to `EffectsChain` to endpoint; spatial/reverb processing does not implicitly enter modulator analysis. | Yes | No | No | Deferred |
| REQ-N41 | AHIO Wi-Fi initially encodes conditioned voice only and generates carrier at the decoder; USB uses the same logical topology without network/codec stages. | Yes | No | No | Deferred |
| REQ-N42 | Initial implementation prefers a bounded low-latency time-domain filter bank; FFT/STFT, look-ahead, or whole-window accumulation requires a separate measured proposal. | Yes | No | No | Deferred |
| REQ-N43 | Kuaru/AHIO own independent vocoder ranges/defaults, gain staging, UI, presets, and activation; vocoder defaults disabled in both. | Yes | No | No | Deferred |
| REQ-N44 | Grouped AHIO rendering resets carrier state from stream epoch and applies changes at identified presentation frames; Kuaru remains unaware of group/network clocks. | Yes | No | No | Deferred |
| REQ-N45 | Acceptance measures path levels, CPU/callback budget by band count, algorithmic/physical latency, intelligibility, clipping/limiter activity, feedback risk, reset determinism, and USB/Wi-Fi stability per product. | Yes | No | No | Deferred |

## Group O — Platform layer

These requirements are the product-neutral part of AHIO Group O. They are separate
from `giganteCore` and clarify REQ-N06; they do not change it. Status columns
are tracked independently as for Group N.

### O.1 — Ownership and scope

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-O01 | The platform layer shall live in the `coreaudio` repository as a separate target `gigantePlatform`, in the single namespace `gigante`, next to and independent of `giganteCore`. `giganteCore` shall never depend on it, which keeps REQ-N06 true for the core. Kuaru and AHIO shall consume the same pinned revision of both targets, with no copies or forks (REQ-N02). | Yes | Partial | Partial | P11B target independence verified; consumer adoption remains P11E/P11H and AHIO is unauthorized |
| REQ-O02 | The platform layer shall contain no Kuaru or AHIO type and no encoder/decoder role, stream plan, transport, protocol, UI or DSP. Names follow REQ-N03 to REQ-N05: `UpperCamelCase` types, `lowerCamelCase` members, no leading underscore and no `Sinu` or `Ahio` in shared names. | Yes | Yes | Yes | P11B source/header audit and public-header isolation; POSIX/Apple duplication closeout verified 2026-09-27 |
| REQ-O03 | One concrete implementation per target platform (Android, iOS, macOS, Linux) shall be selected at link time, following the precedent of `SinuPlatform`. No public header and no caller shall contain a platform conditional (REQ-M12). An unsupported target platform shall fail at configure time with a message naming the platform. | Yes | Yes | Partial | P11B Linux runtime, Android arm64 cross-build, Generic configure rejection; macOS/iOS not locally verified |
| REQ-O04 | Kuaru's existing `SinuPlatform` responsibilities shall migrate into the shared `gigantePlatform` target. Moving those responsibilities together with `SinuHeadsetMonitor`, `SinuAudioFocus`, `SinuWakeLock`, `SinuForegroundService` and the shared `android_audio_runtime` sources is a separate later phase with its own `go`, parity evidence and pinned-revision change. Until it is approved, Kuaru behavior shall not change. | Yes | Partial | Partial | P11H host facts implemented; Linux runtime and Android product build verified; Android runtime facts, exact post-commit gitlink and later mechanisms pending |

### O.2 — JUCE as the only audio backend

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-O05 | JUCE (`AudioIODevice`, `AudioIODeviceType`, `AudioDeviceManager`) shall be the only audio streaming backend of AHIO and of the shared platform layer, on every platform. No AHIO code shall open streams through Oboe, OpenSL ES, AAudio, CoreAudio or ALSA directly, and no second device model shall be added. This supersedes the `AhioAudioDevice` wrapper role of REQ-L13 and the Oboe boundary of REQ-L16; JUCE is consumed through the single pinned installation (REQ-L15, GLC-GOV-06). | Yes | No | No | Not started |
| REQ-O06 | Bluetooth (HFP/SCO, A2DP, LE Audio) is removed from AHIO scope. The related code, UI, `AhioTransport` and device entries shall be removed together, and requirement group C, the Bluetooth and LE Audio gates, P10 and REQ-L16 shall be marked superseded, never deleted or renumbered. Bluetooth results shall remain identified as historical evidence. | Yes | No | No | Not started |
| REQ-O07 | Before the AHIO Oboe path is deleted, the JUCE Android path shall be measured on the Pixel 9 Pro and on the Samsung tablet against the recorded USB baseline: acoustic latency, xruns, and the granted sharing and performance mode as requested, applied and granted values. A regression against the baseline blocks the removal until it is explained and accepted. Equality shall not be assumed from the source code. | Yes | No | No | Not started |

### O.3 — Facts and policy

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-O08 | The platform layer shall supply facts only: per direction the JUCE device name, the platform id where the platform has one, the bus, product name, channel counts and sample rates, plus change events and host facts. It shall make no selection decision. | Yes | Partial | Partial | Host facts and the neutral snapshot/policy/monitor contracts are verified; no concrete endpoint fact source is in current core scope |
| REQ-O09 | Endpoint eligibility and selection policy shall be shared C++ and declarative: for each role the required direction, the allowed buses and the minimum rate and channels. It shall behave identically on every platform and shall be testable with fake inventories and no hardware. Android Java or Kotlin shall contain no selection policy. | Yes | Yes | Yes | P11C hardware-free fake-inventory policy suite |
| REQ-O10 | Selection shall keep requested and effective values distinct and shall treat zero, one and several candidates explicitly. It shall never infer a choice or fall back silently, and it shall retain the reason for every rejected candidate. | Yes | Yes | Yes | P11C explicit statuses, per-entry decisions, loss/substitution and invalid-policy tests |
| REQ-O11 | The bus shall be a bounded attribute of an endpoint (`BUILT_IN`, `USB`, `OTHER`, `UNKNOWN`), not a device subclass. `UNKNOWN` shall mean the platform could not tell, and policy shall treat it as ineligible unless the user selected that device explicitly. It replaces the Kotlin `AudioDeviceKind`, renamed `AudioBus` on 2026-09-25 (values unchanged until the Bluetooth removal), and a session's transport shall be derived from its endpoints and link, never stored as a second copy. | Yes | Partial | Partial | P11C `AudioBus` and explicit-unknown behavior verified; transport derivation awaits consumer session work |
| REQ-O12 | Direction shall be a property of an endpoint. Where JUCE presents one duplex device, the two directional endpoints shall keep separate identities, and a session shall freeze both for its lifetime (REQ-L55). | Yes | Partial | Partial | P11C directional identity/duplex test verified; session freeze awaits consumer integration |
| REQ-O13 | JUCE shall be used first: its enumeration everywhere, and its change notification where it exists (CoreAudio, iOS). Where JUCE lacks a fact or a notification (bus and hot-plug on Android and Linux), a platform fact source or a bounded rescan on a non-realtime thread shall supply it. Platform code shall be added only for a requirement that needs it, and the polling interval and cost shall be bounded and recorded. | Yes | No | No | Deferred by the 2026-09-28 scope decision: products confirm device presence before starting their JUCE encoder/decoder process; no concrete shared source has a current consumer |

### O.4 — Monitoring

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-O14 | Monitoring shall belong to the platform fact source and not to device subclasses. Change events shall arrive on a documented non-realtime thread. Loss or substitution of an effective endpoint shall be reported within a bounded time, with evidence retained, for every bus (generalizing REQ-L12). | Yes | Partial | Partial | Neutral injected monitor lifecycle remains verified; no concrete platform monitor or product loss/substitution handling is in current scope |
| REQ-O15 | Starting and stopping a monitor shall be idempotent, no callback shall be delivered after stop returns, and teardown shall be safe during an in-flight event. | Yes | Yes | Yes | P11H monitor-lifecycle suite: repeated start/stop, original callback retention, external and callback-triggered in-flight teardown, stale late event rejection, destructor cleanup and failure terminal states; Debug/Release and ThreadSanitizer pass 2026-09-27 |

### O.5 — Platform fact sources and session preparation

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-O16 | Android: the inventory and monitor shall be C++ over JNI to `AudioManager`, following `SinuHeadsetMonitor_android.cpp`. Java or Kotlin shall be limited to the Android components that require it (foreground service, manifest, permissions) and to the small forwarding shim for the abstract `AudioDeviceCallback`. The current Kotlin resolver and inventory shall be ported with their tests, and shall be removed only after the same cases pass in the shared policy. | Yes | No | No | Not started |
| REQ-O17 | iOS: the inventory and monitor shall use `AVAudioSession` route ports and route-change notifications through Objective-C++, and the session category and mode shall be set explicitly and reported. | Yes | Partial | Partial | iOS session phase A: explicit playback/default category/mode, operation-time observations and route-change reason notifications implemented; arm64 SDK cross-build passed. Route-port inventory, concrete EndpointMonitor source and physical behavior remain deferred |
| REQ-O18 | macOS: the bus shall come from the CoreAudio transport-type property, and change events from JUCE's own device-list notification. | Yes | No | No | Not started |
| REQ-O19 | Linux: the bus shall come from ALSA card and sysfs information. Hot-plug shall use udev, inotify or bounded polling, the chosen mechanism recorded with its evidence. It shall run headless, with no display or session bus. | Yes | No | No | Deferred by explicit scope decision: there is no Linux interface or automatic hot-plug requirement; the caller starts `ahioenc`/`ahiodec` only when its requested device is present |
| REQ-O20 | Session preparation shall be one interface with a platform implementation each: Android foreground service, wake and multicast locks and audio focus; iOS audio session and background audio; macOS App Nap prevention; Linux scheduling policy. It hosts the platform levers of REQ-M14, REQ-M111 and REQ-M122, and shall report requested, applied and observed results. | Yes | Partial | Partial | Android C1 (2026-11-02): shared focus backend, explicit JNI context factory, requested/native/observed facts, lifetime tests and Kuaru adoption implemented. Pixel baseline/migrated request facts match and user reports note/background/focus-takeover parity. iOS session backend remains present. C2A (2026-11-02): service/wake resource contract, JNI leases, snapshots, host/JVM tests and Android/iOS compile checks implemented; Kuaru C2B adoption is implemented; ordinary Pixel startup/power-button shutdown/restart with the pending-stop repair passes in the consumer ledger. Physical denial/exception/pending-start race qualification remains separate. C3A compares existing versus JUCE-only Android scheduling without a new shared scheduler API; consumer acceptance records observed policy/performance separately. C3B consumer retirement is implemented: Kuaru removes its custom scheduler integration and uses JUCE scheduling; qualification is tracked in consumer retirement-acceptance.md. No shared scheduler API or AHIO change is introduced. Multicast, broader scheduling qualification, iOS interruption qualification and other OS implementations remain deferred |
| REQ-O21 | Host facts (logical cores, RAM, architecture, platform name, build fingerprint and the outcome of the realtime scheduling request) shall be provided by the platform layer and recorded with every measurement (REQ-M13). | Yes | Yes | Partial | P11B Linux facts/lifecycle tests and Android cross-build; consumer measurement recording and Apple verification remain |

### O.6 — Dependencies and verification

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-O22 | The platform target shall not pull in the DSP. It shall consume JUCE through the same consumer-provided target as the core (GLC-GOV-06) and shall build with the core's warning level and `-Werror`. | Yes | Yes | Yes | P11B independent link graph, strict Linux/Android builds, and Kuaru-supplied JUCE interface-target build |
| REQ-O23 | Policy tests with a fake fact source shall cover zero, one and several candidates, loss, substitution, duplicates, an unknown bus, a direction mismatch and the rejection reasons. They shall run on a desktop with no device. | Yes | Yes | Yes | P11C deterministic desktop suite, no device/JUCE runtime required |
| REQ-O24 | Each platform fact source shall pass one shared contract test for its fields. Verification shall be tracked per platform: Android on a device including a physical detach and attach, macOS and Linux locally, and iOS only when hardware is available. A result on one platform shall not be reported as verified for another. | Yes | No | No | Deferred with concrete platform fact sources; no platform verification is claimed |


## Group P — Logger and shared build policy

Product-neutral part of AHIO Group P. `giganteLog` is separate from `giganteCore`.

### P.1 — Logger (`gigante::Logger`)

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-P01 | The AHIO `AhioLogger` shall be generalized as `gigante::Logger` in the `coreaudio` repository, in its own small target `giganteLog`, in the single namespace `gigante` and free of JUCE. `giganteCore` shall not depend on it (REQ-N06, and the realtime contract forbids logging in a callback). `gigantePlatform`, AHIO and Kuaru may depend on `giganteLog` when they have concrete call sites. The target split is provisional and may be revisited when the code is written. | Yes | Partial | Partial | P12B provides the independent JUCE-free target; P12E-K1 verifies Kuaru links it while `giganteCore` remains independent. AHIO and `gigantePlatform` adoption remain open |
| REQ-P02 | Log lines shall keep the AHIO format: `session=<id> event=<name> key=value ...`, fields sorted by key, and space, carriage return and line feed in event, key and value replaced by `_` so that one event is exactly one line. Existing host parsers and the `AhioLoggerTest` cases shall be preserved as golden vectors, so migration does not change any emitted line. | Yes | Yes | Yes | P12B byte-exact AHIO golden plus ordering and sanitization tests |
| REQ-P03 | The logger shall have ordered levels `ERROR`, `WARN`, `INFO`, `DEBUG` and `TRACE`. Every call site names its level. Two ceilings apply: a compile-time ceiling set by the build policy, above which the call is compiled out and its arguments are not evaluated, and a runtime level that may lower but never raise the output above the compile-time ceiling. | Yes | Yes | Yes | P12B TRACE/INFO/OFF configurations, macro side-effect proof and requested/compiled/effective matrix |
| REQ-P04 | The compile-time ceiling and the default runtime level shall come from the build policy (REQ-P13 to REQ-P15), not from each call site. Provisional values, confirmed by Kuaru P12E-K1: `store` allows `WARN` and `ERROR` only; `demo` allows up to `INFO`; `dev` and `profiling` allow all levels. No build policy shall depend on a call site's own conditional. | Yes | Partial | Partial | P12B exposes validated `GIGANTE_LOG_COMPILED_LEVEL`; Kuaru maps all four release types before adding coreaudio, with dev and Android store artifacts verified. Shared `BuildPolicy` remains P12D |
| REQ-P05 | Output backends shall be selected per target platform, with no platform conditional in callers (REQ-O03): Android logcat, macOS `os_log`, and Linux standard error or the system journal. A capture sink for tests shall be available in every build that has tests. Each backend shall map the five levels to its native priorities and report any level it cannot represent. The supported product focus is Linux, macOS and Android; iOS is outside the current scope by the 2026-09-28 decision. | Yes | Partial | Partial | P12B Linux colored stderr lifecycle and callback capture verified; Android `android_sink_mt` cross-build passed; macOS stderr compiles by design but `os_log` and Apple verification remain open |
| REQ-P06 | The logger shall be callable from any non-realtime thread, shall not interleave the text of two events, and shall format outside any lock. It shall provide no call for a realtime callback in its first version: realtime code keeps `RealtimeMetrics` and the observation hooks. A bounded lock-free enqueue drained by a non-realtime thread is a possible later addition and needs its own requirement, with dropped events counted. | Yes | Yes | Yes | P12B synchronous multi-threaded spdlog sink, pre-sink canonical formatting and concurrent non-interleaving test; no async/realtime API |
| REQ-P07 | A session id shall be a process-unique, monotonically increasing value obtained from the logger. Events without a session shall use an explicit value that cannot be confused with a real session. | Yes | Yes | Yes | P12B explicit shared `SessionIdSequence`, cross-logger monotonic test and reserved zero no-session value |
| REQ-P08 | No line shall contain a raw pointer or component address, a user identifier or other personal data. Field values shall be sanitized deterministically, and values that a `store` build must not emit shall be `DEBUG` or `TRACE` only. | Yes | Partial | Partial | Deterministic single-line sanitization plus Kuaru C++/Java field classification are verified; AHIO adoption remains open |
| REQ-P09 | Kotlin and Java code shall log through a thin binding that produces byte-identical lines to the C++ logger, or keep `AhioLogger` delegating to it. `AhioLogger` shall remain unchanged until its replacement passes the golden vectors of REQ-P02. Java `Log.*` calls in Kuaru are covered only by the later Kuaru adoption. | Yes | Partial | Partial | P12E-K2 adds Kuaru's thin Java/JNI binding, AHIO golden format test, and canonical session-zero pre-context fallback; AHIO remains unchanged and separately gated |
| REQ-P10 | Tests shall cover the golden format vectors, sanitization, field ordering, level filtering at both ceilings, proof that arguments of a compiled-out call are not evaluated, the capture sink, per-platform level mapping, and concurrent logging without interleaving. Store and dev configurations shall each be built and tested. | Yes | Partial | Partial | P12B Linux TRACE/INFO/OFF suites pass 8/8; P12E-K1/K2/K3 add Kuaru adapter and log-site inventory tests, Desktop dev, Android dev, clean/incremental Store APK/native/Java/R8 builds, `verifyStoreApk`, and DEX/native ceiling audits. Device logcat and macOS remain unverified |
| REQ-P11 | Product adoption is separately authorized and traced. Kuaru may adopt `gigante::Logger` without changing the AHIO boundary, replacing `SINU_LOG_INFO`, direct `juce::Logger::writeToLog`, direct native Android calls and later Java `Log.*` while preserving store-build silence. AHIO replacement of `AhioLogger` remains a separate phase after AHIO requirements close and requires its own roadmap and `go`. | Yes | Partial | Partial | P12E-K1/K2/K3 complete Kuaru source adoption and classify every JUCE diagnostic; runtime Store logcat remains. No AHIO production file changed |
| REQ-P16 | `gigante::Logger` shall use spdlog as its logging engine unless the evaluation below fails; `gigante::Logger` remains the only API products see, so the engine can be replaced without changing a call site. The engine supplies levels, sinks, color, flush and synchronization; facade macros enforce compile-time removal without exposing spdlog headers. The line format, field ordering, sanitization and session id stay in the facade (REQ-P02, P07, P08), and only the finished line is passed to spdlog. P12B shall evaluate and record: (a) the informative size delta on Linux, macOS and Android, including bundled `fmt`; (b) a macOS `os_log` sink built over the callback sink, since spdlog has none; (c) an immutable spdlog pin through the shared FetchContent cache, with bundled or separately pinned `fmt` and no second copy; (d) no realtime reachability and no asynchronous thread pool without a later requirement; and (e) MIT notices for spdlog and `fmt` in every containing artifact. | Yes | Partial | Partial | spdlog 1.17.0 commit `79524ddd08a4ec981b7fea76afd08ee05f83755d`, bundled fmt 12.1.0, synchronous sinks and MIT notices verified; Linux/Android archive sizes recorded in `Memory.md`; macOS size, `os_log` and Apple build remain open |

### P.2 — Build policy

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-P12 | The release types and the generic build-derived facts of `SinuBuildPolicy` are candidates for the shared repository, because AHIO may adopt several release types. Product-specific features (Kuaru keybed, preset choice, demo UI, voice controls, sample data sources) shall stay in Kuaru. The shared part shall be named `gigante::BuildPolicy`, contain no `Sinu` or `Ahio` name, and be specified after `Source/SinuBuildPolicy.h` and `SinuReleaseType.h` are audited for which members are truly product-neutral. | Yes | No | No | Deferred 2026-09-30: only Kuaru has a concrete release matrix and the user chose to leave shared build policy until later. `SinuBuildPolicy` remains product-owned; the referenced `SinuReleaseType.h` does not currently exist and must be reconciled if P12D resumes |
| REQ-P13 | The shared build policy shall expose, as `constexpr` values derived from compile definitions, at least the release type and the generic facts the logger and diagnostics need: log ceiling, whether diagnostic capture is compiled in, whether the test-automation bridge is compiled in, and whether store hardening applies. A capability that a `store` build excludes shall be excluded at compile time, so no runtime state can enable it (following Kuaru's `kCaptureCompiledIn`). | Yes | No | No | Deferred with P12D on 2026-09-30; Kuaru's existing compile-time gates remain authoritative for the product |
| REQ-P14 | Each product shall declare its own release types and map them onto the shared facts in one place. Adding a release type to one product shall not change the behavior of another. An unknown release-type string shall be rejected, never defaulted. | Yes | No | No | Deferred with P12D on 2026-09-30; no shared parser or fallback is introduced meanwhile |
| REQ-P15 | Until P12 to P14 are approved and implemented, the logger shall take its ceiling from a small log-policy value supplied by the product at build time, so the logger can be delivered without waiting for the shared build policy. Moving `SinuBuildPolicy` out of Kuaru is a separate later phase with its own `go`, parity evidence and a pinned-revision change. | Yes | Yes | Yes | P12E-K1 supplies store→WARN, demo→INFO and dev/profiling→TRACE from Kuaru before adding coreaudio; Desktop dev, Android dev and Store artifacts verify the effective definitions. This provisional product mapping remains the approved policy while P12D is deferred |


## Group Q — Render capability

Product-neutral platform layer, alongside Group O. Unlike Group O (audio
endpoint facts), this group is not derived from AHIO scope: it is an
anticipated-reuse addition, approved 2026-10-16 as an explicit exception to
the default "demonstrably required by this library today" bar, because no
current Kuaru or AHIO consumer needs it yet. Its first and only consumer so
far is `gst-harness` (`gstools` repository, a separate product), which needs
to know whether the host can actually render video before choosing how to
initialize its own windowing toolkit.

### Q.1 — Generic capability contract

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-Q01 | The platform layer shall expose a generic, extensible capability-detection contract rather than a one-off flag: a capability domain is an enum, and `HostFacts` carries the detected set for that domain as a `std::vector`. A later capability domain is added by extending or adding an enum and one probe function per `*HostFactsSupport` platform family, following the existing `PosixHostFactsSupport`/`AppleHostFactsSupport` pattern, not by inventing a parallel mechanism. | Yes | Yes | Yes | P13B: `RenderCapability` enum plus `HostFacts::renderCapabilities` |
| REQ-Q02 | Render capability (`audio`, `video`) is the first concrete instance. Detection shall be observed at read time, never assumed from platform identity: a VM or container may run macOS or Linux and still lack video. Linux/POSIX: video is detected from a reachable Wayland or X11 display-server socket (file presence, not a full protocol handshake, so no client-library dependency is added). macOS: video is detected from `CGSessionCopyCurrentDictionary`'s `kCGSessionOnConsoleKey` (a real, logged-in console/GUI session), linking `ApplicationServices` only for the macOS build target. iOS reports both unconditionally, consistent with a handheld device always having a built-in screen and speaker. Audio is always reported on POSIX and Apple hosts, since a null/dummy audio sink is always available even without real hardware. | Yes | Yes | Partial | P13B: POSIX and macOS probes implemented and unit-tested; macOS probe additionally confirmed by a standalone run on a real interactive console session (`audio video` both detected); Linux socket-presence path exercised only by the empty-vector/no-display-set case locally, not against a real Wayland/X11 session; iOS and Android have no device verification |
| REQ-Q03 | No public header or caller shall contain a platform conditional (REQ-O03/REQ-M12). An unsupported, undetectable, or unprobed case shall report that capability absent from the vector, never guessed as present. | Yes | Yes | Yes | P13B: `detectPosixRenderCapabilities`/`detectAppleRenderCapabilities` are the only platform-conditional code, confined to their own `*HostFactsSupport` translation units; `testUnprobedRenderCapabilitiesStayEmpty` guards the default-constructed case |
| REQ-Q04 | This phase adds the fact only. It shall not add Kuaru/AHIO consumer wiring, Android or iOS device-verified probes, or any other capability domain beyond render capability. | Yes | Yes | Yes | P13B: no Kuaru/AHIO call site changed; `AndroidHostFacts` unmodified (empty vector, explicit scope decision, not an oversight) |

## Group R — REST engine (`restcore`)

**Status: specified and approved by the user on 2026-11-02; R0 and R1 complete
within the coverage they record.** `gigante::restcore` is implemented and tested on
macOS, and compile-only on Android and iOS. No project uses it yet. Partial in the
Verified column means no device or Linux run. Each phase needs its own roadmap and
`go` (GLC-GOV-09). This group replaces the deferred specification in
`modules/grest-cpp`.

`restcore` is the HTTP/HTTPS client and server engine for any C or C++ project,
most likely those already using this repository. It defines a common endpoint
set and leaves every domain API to the consuming project. Python
`modules/tools/grest` is the reference for route declaration, thin handlers and
the consumer-owned domain boundary. The IDs use `REQ-RC` because Kuaru already
uses `REQ-R`.

cpp-httplib is the selected engine by user decision on 2026-11-02, replacing the
earlier libhv selection. The designed capacity is 100 simultaneous clients and
MQTT is not needed, so the event-loop scale that favored libhv does not apply,
and cpp-httplib's safer TLS defaults, request limits and security process carry
more weight. Its production suitability is a hypothesis that R0 must test: a
failed criterion is reported with reproducible evidence and a mitigation. libhv
is the fallback if a concrete blocker or a later scale requirement appears; its
source observations remain in this file's history at commit `0269cac`.

### R.1 — Scope and ownership

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC01 | The engine shall be a product-neutral static library, CMake target `giganteRestCore` with alias `gigante::restcore`, providing server, client, common endpoints and consumer route registration. It shall contain no knowledge of any consumer domain. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC02 | The target shall be opt-in through `GIGANTE_BUILD_REST`, default `OFF`. `gigante::core`, `gigante::platform` and `gigante::log` shall gain no network or HTTP library dependency, and a consumer that does not link `restcore` shall build unchanged and fetch nothing new. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC03 | Consumers own domain routes, state machines, authorization decisions, resource identities and every side effect. A handler shall be a thin delegate to the consumer's controller. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC04 | The engine shall not use SSH, discover hosts, configure firewalls or start the process that hosts it. Bringing the first process up stays with the consumer's deployment tooling (Python `executor` REQ-EX13/EX14 is one such tool). The in-band `copy` and `deploy` families act only on an already-running server. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC05 | The engine is a control and evidence plane. It shall not claim media-transport properties: HTTP streaming does not provide WebRTC-equivalent latency or congestion control, and consumers may run RTP/UDP beside it. | Yes | No | No | R2 |
| REQ-RC06 | Engine APIs are non-realtime and shall not be called from an audio callback. A consumer that streams audio-thread data shall bridge it through bounded, non-blocking publication outside the callback, keeping the Realtime contract in `CLAUDE.md` intact. | Yes | No | No | R2: call-site audit; consumer xrun evidence belongs to its adoption phase |

### R.2 — Transport security and the development default

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC10 | Server and client configuration shall require an explicit security profile, `development` or `production`, with no implicit value. The consumer maps its own release types: core shall not depend on Kuaru's `SinuBuildPolicy` (P12D remains deferred). | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC11 | `development`: HTTP is the default scheme and the default bind is loopback. HTTPS remains available. A non-loopback plaintext listener requires a separate explicit setting and shall emit a warning naming the address. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC12 | `production`: HTTPS is mandatory. A plaintext listener, an `http://` client URL or a missing certificate shall fail with a specific error. `GIGANTE_REST_ALLOW_PLAINTEXT=OFF` shall remove plaintext support at build time. | Yes | Yes | Partial | R1: profile refusals tested. The build flag compiles, but its refusal has no test |
| REQ-RC13 | A TLS initialization failure shall never downgrade to HTTP. Start shall fail and report the backend, the operation and the cause. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC14 | `GIGANTE_REST_TLS_BACKEND` shall accept `AUTO`, `OPENSSL`, `MBEDTLS` or `NONE`. Defaults: OpenSSL on Linux and macOS; mbedTLS on embedded Linux, Android and iOS, subject to R0. `AUTO` shall never resolve to `NONE`, upstream's use-if-available detection shall be disabled in favor of an explicit choice, and configure shall fail when no permitted backend is found. cpp-httplib's wolfSSL backend is not offered (RC75). `NONE` is valid only with development-profile builds. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC15 | TLS 1.2 is the minimum and TLS 1.3 is preferred where the selected backend supports it. The engine shall enforce the minimum itself rather than relying on backend defaults, and shall report the negotiated version. Server certificate and key are validated at start, and mutual TLS is optional. | Yes | Partial | Partial | R1 relies on the library's TLS 1.2 minimum; R0 confirmed rejection on the probes. The engine does not yet enforce it itself or report the negotiated version |
| REQ-RC16 | The client shall verify the peer certificate and hostname by default in both profiles. Disabling either requires an explicit per-client setting, is refused in `production`, and is logged. Pinning a development certificate is the supported alternative. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC17 | Authentication is a required decision per route: each route declares a scheme or an explicit `none`. In `production`, every route other than minimal `health` and `heartbeat` requires authentication unless the consumer overrides that route by name. Credentials shall not be hardcoded, logged or returned. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC18 | Every listener address shall be set explicitly from validated configuration, never inherited from an engine default. An unauthenticated wildcard listener is non-conforming in `production`. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |

### R.3 — Common endpoints

Common routes live under the reserved prefix `/core/v1/`. A consumer route under
that prefix, or a duplicate route, shall fail registration. Consumers version
their own routes under their own prefixes, for example `/api/v1/`.

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC20 | Every server shall provide `health` (`GET /core/v1/health`): readiness from consumer-registered named checks, returning 200 or 503. It shall not expose secrets or filesystem paths. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC21 | Every server shall provide `heartbeat` (`GET /core/v1/heartbeat`): liveness, uptime, instance identity and optional consumer build identity, as one cheap call the client polls. Failure thresholds are client policy. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC22 | Every server shall provide `status` (`GET /core/v1/status`): engine state, bound addresses, scheme, TLS state, connection and limit usage, active streams and transfers, service states, and an optional consumer detail object. Requested and effective configuration shall remain distinguishable. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC23 | Every server shall provide `getBuildVersion` and `getCapabilities`: API version, build identity, enabled common routes, stream lanes, limits and TLS backend. Host facts come from an injected provider, not from a new detection path. `checkDeps` is a pass-through to a consumer-supplied report. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC24 | Service lifecycle is opt-in: `startService`, `stopService`, `restartService` and service listing, under `/core/v1/services/{name}/`, act only on logical services the host registers through callbacks. They shall respond `202 Accepted` before the callback runs and never run it on a server connection thread. Acceptance means dispatch, not completion, and `status` reports the outcome. Stopping the hosting server shall drain gracefully after the response is sent. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC25 | Live streaming is declared by the consumer: `GET /core/v1/streams` lists descriptors and `GET /core/v1/streams/{id}/live` subscribes. Each descriptor states lane (SSE, chunked, WebSocket or binary), media type, format name and version, encoding, framing, live or finite behavior, cursor semantics and delivery policy (REQ-RC42). The engine treats payloads as opaque. | Yes | No | No | R2 |
| REQ-RC26 | Bounded diagnostics are opt-in: `getLog` over consumer-supplied roots with match expressions, never request paths; `cmdStatus` from a consumer-owned manager, without scanning the process table; and `appendLogMarker` naming a logical command. Output is bounded, and symlinks and non-log files are excluded. | Yes | No | No | R2 |
| REQ-RC27 | The `copy` family is opt-in and authenticated: `copyCreate`, `copyUpload`, `copyDownload`, `copyList`, `copyStatus` and `copyCancel`. Requests name consumer-registered resource IDs and never filesystem paths or URLs. It shall stream without whole-file buffering, enforce size limits, check free space up front, support resume, verify a declared digest, and commit atomically so a failed transfer leaves no partial destination. `copyList` lets a client discover the logs and artifacts it may download. | Yes | No | No | R2: traversal, truncation and digest-mismatch tests |
| REQ-RC28 | The `deploy` family is opt-in and separately gated by `GIGANTE_REST_ENABLE_DEPLOY`, default `OFF`: `deployStage`, `deployActivate`, `deployStatus` and `deployRollback`. It consumes completed `copy` transfers and a manifest. Pre-flight checks (architecture, ABI, dependencies, digest) reject an incompatible payload before commit. The engine never executes a payload: activation and rollback are consumer callbacks. `production` additionally requires a verified manifest signature. | Yes | No | No | R6: separately approved, with its own security review |
| REQ-RC29 | `deploy` and `execCmd` shall not be available on iOS or in store-distributed Android artifacts, because both platforms restrict downloaded executable code. The exact policy text shall be checked in R6 and whenever a store release is prepared. | Yes | No | No | R6: artifact symbol audit |
| REQ-RC30 | `execCmd` (grest's remote shell escape hatch) is development-profile only, requires an explicit build option and registration, and shall not compile into a `production`-capable artifact. | Yes | No | No | R6 |
| REQ-RC31 | Every common route shall use a versioned structured error containing a stable code, safe message, request identity and retryability. Internal exceptions, paths, credentials and backend errors shall not leak. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |

### R.4 — Consumer routes and REST behavior

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC33 | Route registration shall support GET, POST, PUT, PATCH, DELETE, OPTIONS and HEAD with dynamic path parameters written `{id}` in the public API and translated to the engine's own syntax, plus query parameters, headers, JSON and binary bodies. Path parameters are validated explicitly at run time: no compile-time typed checking is claimed. Consumers shall not depend on the engine's route syntax or regular-expression patterns. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC34 | The server shall provide ordered middleware, authentication hooks, configurable CORS (off by default), appropriate status codes and request-size limits enforced before the body is buffered. | Yes | Partial | Partial | R1: fixed chain with auth, CORS and limits. Project-supplied middleware is not implemented, and chunked bodies are limited only by the server-wide limit |
| REQ-RC35 | Handlers run on the connection's server thread by default, and may run concurrently, so they shall be thread-safe. A project may inject a dispatcher that hands them to its own thread, such as a Qt or JUCE thread, and which shall not be an audio thread. The server thread waits with a deadline: a full queue returns a structured 503 and a missed deadline a structured 504, and a later result is discarded. A dispatcher adds safe handoff, not capacity, because the server thread stays held. Long work shall start, reply 202 and report through `status`. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC36 | Deferred by user decision on 2026-11-02, with no phase assigned. Route declarations should eventually drive a deterministic generated OpenAPI 3.1 document and project-route clients. Until then, `RestClient` is typed only for common endpoints, and projects call their own routes through `call`. | Yes | No | No | Deferred |
| REQ-RC37 | Mutating routes shall declare idempotency. Requests carry request and correlation IDs, and these appear in errors and logs. The client shall retry automatically only where the route declares it safe. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |

### R.5 — Concurrency, streaming and binary transfer

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC40 | The designed capacity is 100 simultaneous clients, including long-lived streams and transfers, configurable downward per deployment. The server uses cpp-httplib's thread pool with explicit maximum thread and queue values: its unbounded default queue shall not be used. Capacity planning shall count one thread per open stream, transfer or kept-alive connection and two per WebSocket connection, and idle keep-alive time shall be short. Short-lived and keep-alive connections are served over IPv4 and IPv6, with limits on requests, headers, idle time and operation deadlines. HTTP/1.1 is the baseline, and HTTP/2 is not required. The architecture brief's event-loop and 1,000+ connection requirements are deliberately not adopted. | Yes | Partial | Partial | R1: explicit pool and queue limits, short keep-alive, HTTP/1.1. The 100-client capacity was measured in R0 only, and on the Mac only |
| REQ-RC41 | Overload shall be bounded and observable. Streams and transfers have separate caps enforced at the route, which return a structured 503 and increment a counter before capacity is consumed. Those caps shall leave enough pool capacity that `health`, `heartbeat` and `status` keep answering with streams and transfers at their limits. A full pool queue closes the socket without an HTTP response (upstream behavior), so it shall be counted and treated as the last-resort path. Slow and idle clients are bounded by read and write timeouts and a per-stream output limit. | Yes | No | No | R0, R2: all-streams-busy and slow-reader tests, with bounded RSS and responsive `health` |
| REQ-RC42 | Every stream shall declare one delivery policy. `reliable` never drops silently: the producer is told to wait, or the stream ends with an explicit error. `realtime` may drop stale data and shall count and report drops. `broadcast` shares source data between subscribers, with per-subscriber limits and slow-subscriber eviction. No policy shall block a producer's domain operation. | Yes | No | No | R2: each policy under a stalled subscriber |
| REQ-RC43 | Streams shall support incremental production, cancellation, disconnect propagation, optional heartbeats, slow-client detection and deterministic cleanup. No callback shall run after cancellation completes, including during shutdown. | Yes | No | No | R0, R2: sanitizer runs with abrupt disconnects |
| REQ-RC44 | Binary transfer shall read and write incrementally, without loading whole files, and support `application/octet-stream`, multipart where required, Range resume, Content-Length or chunked framing, cancellation, progress callbacks, bandwidth limits, size limits and optional integrity checks. `sendfile` is an optimization only where verified with the active TLS mode, with a portable buffered fallback. | Yes | No | No | R2: file larger than the configured memory limit |
| REQ-RC45 | cpp-httplib's mount points shall be unused, `set_file_content` shall never receive a client-supplied path, and zlib, Brotli and zstd support, which upstream's CMake enables by default when available, shall be disabled unless a later requirement enables them. | Yes | No | No | R0: build-option and route audit |

### R.6 — Client

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC48 | The engine shall provide an HTTP/HTTPS client built on cpp-httplib, with synchronous calls, asynchronous calls through an engine-owned bounded executor, connect and operation deadlines, cancellation, typed common-endpoint calls, stream subscription and incremental upload and download. Client capability is an R0 hypothesis. libcurl is the fallback only if R0 reports a concrete blocker. | Yes | Partial | Partial | R1: synchronous and asynchronous calls, deadlines, cancellation and common-endpoint calls. Stream subscription and incremental transfer are R2 |
| REQ-RC49 | libcurl shall be used by the functional test client as an independent interoperability check and shall not be linked into `restcore`. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |

### R.7 — MQTT client

Removed by user decision on 2026-11-02: MQTT is not needed. The IDs stay retired
and shall not be reused. A later need requires a new requirement and engine review.

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC50 | ~~Optional MQTT client~~ | Removed | No | No | Removed 2026-11-02 |
| REQ-RC51 | ~~MQTT protocol conformance~~ | Removed | No | No | Removed 2026-11-02 |

### R.8 — Public API, lifetime and logging

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC55 | The public interface shall be C++: `gigante::RestServer`, `gigante::RestClient` and plain value types, with no cpp-httplib type in a public header. A test shall compile the public headers without cpp-httplib on the include path. By user decision on 2026-11-02, there is no C API until a plain-C project needs one, so plain C projects are not yet supported, and JNI or Swift use needs a project-written wrapper. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC56 | The API shall provide create, configure, start, stop, destroy, running state, bound address and port, and error retrieval, and shall document thread safety, ownership, callback lifetime and shutdown behavior for each. `destroy` shall guarantee that no callback runs after it returns. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC57 | Multiple independent server and client instances shall coexist in one process with separate certificates, limits and logging. Each server shall own its own thread pool. Any remaining process-wide effect, such as the `Server` constructor ignoring `SIGPIPE`, shall be documented, and R0 shall audit the library's static state. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC58 | Names follow this repository: camelCase with `Rest` type names in namespace `gigante`, public headers under `include/gigante` and private sources under `src/rest`, extending GLC-GOV-10. Uppercase `GET`, `POST` and similar methods are an agreed exception, because they name HTTP methods. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC59 | Logging goes through an injected callback, which a project may forward to its `gigante::Logger`, and metrics through an observable snapshot. restcore shall not link `gigante::log`, because `Logger.h` needs C++20 and restcore is C++17. Logs shall not record credentials. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |

### R.9 — Platforms and mobile

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC62 | The library build matrix covers Linux x86_64 and ARM64; embedded Linux ARM64, and ARMv7 where supported; macOS arm64 and x86_64; Android arm64-v8a and x86_64; iOS arm64; and iOS Simulator arm64 and x86_64 where supported. An unexecuted platform shall be recorded as unverified, with its exact commands. | Yes | No | No | R0, R5 |
| REQ-RC63 | restcore's baseline is C++17 and CMake 3.22, the existing core minimum. Static linking is the embedding path. The rest of this repository currently needs C++20 for `Logger.h`, so a build of the whole repository sets `CMAKE_CXX_STANDARD=20`. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC64 | Android: a minimal JNI wrapper with lifecycle and thread-attachment rules, documented permission and foreground-service restrictions. Foreground-service ownership stays with `SessionPreparation` and is not duplicated. | Yes | No | No | R4 |
| REQ-RC65 | iOS: device and simulator builds, XCFramework packaging, Swift and Objective-C use, documented local-network privacy configuration, and an explicit stop or rebind path on suspension. Permanent background availability shall not be promised. | Yes | No | No | R4 |

### R.10 — Dependency trust and supply chain

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC70 | cpp-httplib shall be pinned to an exact reviewed commit through CMake and compiled in private translation units, so that no consumer includes its header. Updating the pin is a reviewed change with its own diff review and full R5 rerun, never automatic. Upstream's pace makes this a required control, not a formality. | Yes | No | No | R0 |
| REQ-RC71 | Trust is established by evidence on the exposed surface, not by origin: authorship, human or AI, neither qualifies nor disqualifies code. Project age is not evidence either. R0 shall review the pinned HTTP parsing, multipart, TLS and streaming paths, with specific attention to the code from 2025 onward: the mbedTLS backend, SSE client and WebSocket support. | Yes | No | No | R0: review record per file |
| REQ-RC72 | The adapter shall have its own fuzz targets under ASan and UBSan for HTTP requests, multipart, SSE parsing and, if enabled, WebSocket frames, run against the pinned commit. A crash blocks the phase until it is fixed or the path is removed. Upstream's fuzzing is useful but shall not be counted as this evidence. | Yes | No | No | R0: initial run, R5: sustained run |
| REQ-RC73 | Upstream's published advisories (23 on 2026-11-02) and open security issues shall be reviewed with a disposition per pin. A confirmed, unmitigated vulnerability on an exposed path is a stop condition. | Yes | No | No | R0 |
| REQ-RC74 | The project shall be able to carry a security patch in a maintained fork or patch set, because upstream response time is not a guarantee. | Yes | No | No | R0 |
| REQ-RC75 | Dependencies shall be limited to MIT, BSD, Apache-2.0 or BSL-1.0, with optional and transitive ones verified. OpenSSL shall be 3.0 or later, because earlier releases use a different license, and wolfSSL is excluded because it is GPL-licensed. Test-only tools such as curl are recorded separately and not linked. | Yes | No | No | R0: license audit |
| REQ-RC76 | Kuaru's minimal `MBEDTLS_CONFIG_FILE` is a directory-wide crypto-only configuration without TLS modules, so `restcore` shall not rely on it. An integration that needs mbedTLS TLS beside it shall state how the two configurations coexist, and shall prove no symbol conflict. | Yes | No | No | R0, consumer adoption phase |

### R.11 — Verification

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-RC80 | Default CTest entries shall be deterministic and hardware-independent, using loopback sockets on ephemeral ports only. This is the single permitted exception to the "no network" core test rule, and applies only with `GIGANTE_BUILD_REST=ON`. Stress and benchmark runs use separate labels outside the default run. | Yes | Yes | Partial | R1: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). macOS tests; Android and iOS compile only |
| REQ-RC81 | The stress run shall hold 100 simultaneous HTTPS clients with mixed REST and streaming traffic, long-lived connections, slow clients, abrupt disconnects and reconnection storms, long enough to detect memory growth. An overload run at 150 clients shall show deterministic rejection, with `health` still answering. It reports requests per second, p50/p95/p99 latency, CPU, RSS, thread count, memory per client, throughput, connection errors and shutdown duration. Runs shall include a phone-class device, where 100 threads may need a lower configured capacity. No target is claimed without a recorded result, with its host. | Yes | No | No | R5 |
| REQ-RC82 | ASan, UBSan and TSan shall cover lifecycle, streaming, cancellation and shutdown where supported. Negative tests shall cover each failure and cancellation path named in this group. | Yes | No | No | R1–R5 |
| REQ-RC83 | Acceptance is per platform and backend actually run, and shall list failed or unexecuted criteria explicitly. A failed criterion shall not be hidden to preserve the library decision. | Yes | No | No | R5 |

### R.12 — cpp-httplib source observations

Read from commit `a18895f` (2026-10-31, version 0.64.0) on 2026-11-02. **Nothing
was compiled or run**, so these are starting points for R0, not verified
behavior. Line numbers refer to `httplib.h` unless another file is named.

| Topic | Observed | Effect |
|---|---|---|
| License and age | MIT; first commit 2012-09-21; 2,335 commits | License fits; transitive audit remains (RC75) |
| Line age | 16% of lines last changed in 2022 or earlier, 5% in 2023–2024, 18% in 2025 and 59% in 2026. Blame was not filtered for reformatting or moves, so the 2026 share may be overstated | Project age is not evidence (RC71) |
| New features | First appearance: SSE client 2025-12-15, mbedTLS backend 2026-02-01, WebSocket 2026-02-14 | Closest review (RC71) |
| Churn and AI | Owner made 160 commits in October 2026. 39 commits carry AI markers, 35 of them from 2026, covering about 1% of lines. Unmarked AI assistance cannot be measured | RC70, RC71 |
| Security process | CIFuzz workflow and fuzz targets; 23 published advisories; no open issues matched a security keyword search | RC72, RC73 |
| TLS verification | Certificate and hostname verification default to on (`:3198-3199`, `:4874-4875`) | Supports RC16; test per backend |
| TLS version | TLS 1.2 minimum set in the OpenSSL and mbedTLS backends (`:20167`, `:21379`) | Supports RC15; test per backend |
| TLS backends | OpenSSL below 3.0 rejected at compile time (`:501`); mbedTLS 2.x–4.x (`README.md:70`); wolfSSL also available | RC14, RC75, RC76 |
| Thread pool | Default at least 8 threads, maximum 4 times that (`:206-213`); queue unbounded unless set (`:2076`, `:12622`); a full queue closes the socket and logs an error (`:14739-14744`) | RC40, RC41 |
| WebSocket | Thread per connection plus a heartbeat thread (`README-websocket.md:6`) | RC40 |
| Request size | Configurable limit, default 100 MB (`:142`, `:2370`) | Supports RC34; set a lower value |
| Bind address | Host is an explicit argument (`:2386`) | Supports RC18 |
| Global state | `Server` constructor ignores `SIGPIPE` process-wide (`:13639`); some function-level statics exist (`:10941`, `:19616`) | RC57 |
| Build defaults | CMake uses OpenSSL, zlib, Brotli and zstd when available (`CMakeLists.txt:4-9`) | RC14, RC45 |
| Mobile builds | Not confirmed as covered by upstream CI | R0 must build the matrix itself |

The pin is `v0.64.0` (`24e58ce`), one commit before the commit read here. That
commit's changes to `httplib.h` are small, so line numbers differ slightly.

R0 results replace these observations where they differ. See
[feasibility](docs/devreq/rest-engine/feasibility.md).

### R.13 — Decisions

Decisions 1–4 were confirmed by the user on 2026-11-02. Decisions 5–7 were not
answered individually and use the listed defaults until the user says otherwise.

| # | Question | Decision |
|---|---|---|
| 1 | `modules/grest-cpp` is a deferred specification of the same engine | Superseded by Group R, keeping gst-harness as a consumer |
| 2 | `copy` and `deploy` were moved out of Python grest as a layering correction, and `startService` cannot start its own process | They are REST operations on a running server, with first-process bootstrap outside (RC04, RC24, RC27, RC28) |
| 3 | `execCmd` gives remote shell access | Development builds only, and only if a consumer needs it (RC30) |
| 4 | The architecture brief names the library `restcore` with `rest_*` C symbols, while this repository requires `gigante` camelCase naming | `gigante::restcore`, with `Rest` class names now that decision 9 removed the C API (RC01, RC58) |
| 5 | The Scope boundary in `CLAUDE.md` says RPC and Android JNI are not core infrastructure | Default: narrow exception for this requested engine only |
| 6 | Kuaru's iOS plan has no Simulator phase, while the brief lists Simulator builds | Default: library build only, with no Kuaru Simulator qualification |
| 7 | Kuaru excludes its test bridge from `store` artifacts (Kuaru REQ-L03) | Default: no change; Kuaru store adoption is a separate consumer decision |
| 8 | Where the code lives | `src/rest` and `include/gigante` in this repository, target off by default. User decision after the architecture discussion |
| 9 | Public API | C++ only for now (RC55). User decision |
| 10 | Layers and classes | The smaller set in the [server class diagram](docs/devreq/rest-engine/server-class.html), keeping the `HttpTransport` interface. User decision |
| 11 | Handler threads | Concurrent on server threads, with an optional injected dispatcher (RC35). User decision |
| 12 | Common endpoints | Always-on base, opt-in groups, `deploy*` off at build time, `execCmd` not built. User decision |
| 13 | Logs, artifacts and live streams | Stay in R2, with `copyList` added. User decision |

### R.14 — Findings from R0 that refine this group

These apply when the listed requirements are implemented, and do not change their IDs.

| Requirement | Refinement |
|---|---|
| RC14, RC76 | Mbed TLS needs threading on, self-test off, ROM AES tables and its cipher list built before concurrent use. One process can link only one Mbed TLS build. Use 3.6.7 or later |
| RC40, RC41 | The thread pool assigns a thread at accept, so reserved capacity protects `health` from streams and transfers, not from idle connections. Size pool and queue for bursts, keep read timeouts short, and do not expose the server to untrusted networks until this is mitigated |
| RC41 | A stalled TLS client can hold a thread for several times the write timeout. Choose the write timeout with this in mind |
| RC44 | The transport shall set status 206 for ranged provider responses |
| RC45 | Compile only `httplib.h` from upstream, without running its CMake |
| RC48 | The client shall protect a client-only process from `SIGPIPE`, and shall enforce its own reconnect attempts and minimum interval |
| RC72 | Fuzzing did not run in R0. Its first run, and the sustained run, belong to R5 |

## Repository governance requirements

These IDs cover repository practice not owned by the product-facing Group N
contract.

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| GLC-GOV-01 | Maintain `CLAUDE.md`, `Memory.md`, `requirements.md`, and `architecture.md`; update affected documents at each phase boundary. | Yes | Yes | Yes | G0 file/consistency checks passed 2026-09-24 |
| GLC-GOV-02 | Work one approved phase at a time and await `go` before the next phase. | Yes | Yes | Yes | G0 stops before P8F and awaits `go` |
| GLC-GOV-03 | Never commit; provide a detailed proposed commit message. | Yes | Yes | Yes | Agent handoffs contain no assistant-created commit; commits present were created externally |
| GLC-GOV-04 | Run `git diff --check` in every changed repository. | Yes | Yes | Yes | G0 checks passed 2026-09-24 |
| GLC-GOV-05 | Use modern target-based CMake; separate library, tests, and adapters; apply strict project warnings while isolating external warnings. | Yes | Yes | Yes | P8F target build with GCC 16.2.1 and `-Werror` |
| GLC-GOV-06 | Do not vendor a second JUCE. Reuse a consumer-provided exact pin through explicit CMake integration if the extracted DSP still requires JUCE. | Yes | Yes | Yes | P8F core is JUCE-free; repository dependency audit |
| GLC-GOV-07 | Fixtures record revision, parameters, rate, channels, block size, seed, origin, tolerance, and comparison mode; goldens are never automatically updated and every difference requires a documented decision. | Yes | Yes | Yes | P8F fixture plan, reviewed manifest, immutable capture workflow |
| GLC-GOV-08 | The Kuaru consumer provides repeatable standalone Debug and Release entry points that keep build trees outside the core checkout, retain strict warnings and CTest, and pass an exact core commit plus tracked-source digest. This convenience tooling must not inject JUCE or make the core depend on Kuaru's checkout layout. | Yes | Yes | Yes | G1 shell syntax, option/error checks, and simulated Debug/Release CMake/CTest command verification |
| GLC-GOV-09 | Before implementation begins, present a roadmap naming objective, requirement IDs, affected repositories/files, non-goals, acceptance checks and documentation updates; implementation requires a new `go` after that roadmap. | Yes | Yes | Yes | G2 roadmap presented and separately approved on 2026-09-28 |
| GLC-GOV-10 | Public headers shall remain together under `include/gigante`; implementation files and private headers shall be grouped under `src/dsp`, `src/logger`, `src/metrics`, or `src/platform` according to architectural ownership. Physical grouping shall not change the single public namespace, public include paths, target boundaries, dependency direction, or runtime behavior. | Yes | Yes | Yes | G3 layout audit, strict Linux/Android builds, full CTest and Kuaru consumer build passed 2026-09-29 |

## Phase traceability

| Phase | Scope | Requirement IDs | Exit evidence | Status |
|---|---|---|---|---|
| G0 | Establish the core repository as Group N authority and import its governance/architecture context | N01–N45; GOV-01–04 | Required documents, source audit, consistency checks, `git diff --check` | Complete 2026-09-24 |
| P8F | Build skeleton plus deterministic Kuaru characterization; no production DSP moves | N01–N08, N21–N23, N26, N31, N33; GOV-01–07 | Configure/build/CTest, header isolation, dependency decision, fixture schema, reviewed pre-migration vectors | Complete 2026-09-24 |
| P8G | Extract fixed-order chain and migrate Kuaru alone | N09–N11, N15, N17–N25, N32–N33; GOV-01–07 | Bit-exact/tolerance parity, lifecycle/realtime suite, stage-budget comparison | Complete locally: exact Kuaru gitlink, five-vector parity, lifecycle suite, controlled budget, and zero-allocation production test verified; remote reachability not reverified |
| P8H | Neutral observation plus Kuaru `ProfileCollector` adapter | N26–N30, N32–N33; GOV-01–07 | Metrics parity and disabled-observer overhead | Complete locally 2026-09-25; allocation closeout and later platform phases followed. Current Kuaru gitlink reconciliation is tracked by REQ-N02; remote reachability was not reverified |
| P8K | AHIO-owned stream roles and local identity plan; no core implementation ownership | L32–L54 in AHIO | AHIO evidence; prerequisite for P8I | Not started in AHIO |
| P8I | AHIO consumes pinned verified core in USB neutral bypass | N02, N06–N08, N13–N16, N19–N23, N27, N29, N34 | AHIO conservation/buffering/xrun/CPU/physical-latency comparison | Not started in AHIO |
| P8J | AHIO enables independently calibrated effects incrementally | N10–N14, N17–N20, N24–N25, N35 | AHIO per-effect quality/stability/latency evidence | Not started in AHIO |
| P11A | Specify the platform layer (Group O); no code | O01–O24 | Requirements review and consistency check | Specified 2026-09-25 |
| P11B | `gigantePlatform` skeleton, host facts, platform selection reproducing `SinuPlatform` | O01–O03, O21–O22 | Configure/build on each available platform, header isolation, facts test | Complete; Linux/Android verified and internal OS-family duplication removed 2026-09-27; Apple verification pending |
| P11C | Shared endpoint policy and fake fact source | O08–O12, O23 | Fake-inventory policy suite on desktop | Complete 2026-09-25; consumer inventories/monitoring remain later phases |
| P11D | Measure the JUCE Android path against the USB baseline (AHIO-owned evidence) | O05, O07 | Latency, xruns, granted modes on two devices | Not started in AHIO |
| P11E | Android fact source and AHIO adoption | O05–O06, O13–O16, O20, O24 | Device contract test including physical detach/attach | Not started |
| P11F | Linux and macOS fact sources | O13–O15, O18–O19, O24 | Contract tests per platform | Deferred 2026-09-28: no Linux/macOS interface or concrete shared-source consumer; callers gate `ahioenc`/`ahiodec` startup on known device presence |
| P11G | iOS fact source | O17, O24 | Contract test on hardware | Not started |
| P11H | Kuaru migrates `SinuPlatform` responsibilities into `gigantePlatform`, then migrates other mechanisms only where neutral | O04, O14–O15; Kuaru AM01–AM04 | Host-fact parity, lifecycle tests, product builds, exact gitlink and regression evidence | Host-facts consumer step and neutral monitor lifecycle complete 2026-09-27; Kuaru headset adoption, platform change sources, Android device parity and exact post-commit gitlink remain pending; later mechanisms require separate review |
| P12A | Specify the logger and shared build policy (Group P); no code | P01–P15 | Requirements review | Specified 2026-09-25 |
| P12B | `giganteLog` with format, levels, backends, tests; spdlog evaluation first | P01–P10, P15–P16 | Golden vectors, level-gating and per-platform builds | Partial 2026-09-28: neutral facade, Linux/capture sinks, Android cross-build and OFF dependency removal verified; macOS `os_log`, Apple build and consumer adoption remain open |
| P12C | AHIO adopts the logger | P09, P11 | AHIO tests; `AhioLogger` retired | Not started in AHIO |
| P12D | Audit `SinuBuildPolicy`; `gigante::BuildPolicy` | P12–P14 | Audit record, constexpr tests | Deferred 2026-09-30 by user decision; `SinuBuildPolicy` remains Kuaru-owned and resumption requires a new roadmap and `go` |
| P12E | Kuaru adopts logger in separately approved C++/Java/DBG steps; shared build-policy migration remains P12D | P11, P15 | Kuaru adapter test, call-site/realtime audits, dev/store builds, store artifact and device-logcat checks | Source adoption complete locally 2026-09-29: P12E-K1/K2/K3, tests and dev/Store artifact audits pass. Runtime Store device logcat remains the only Kuaru logging evidence open |
| G1 | Add Kuaru-owned standalone core build/test helpers; no DSP or AHIO change | GOV-05–08 | Shell syntax, help/error paths, Debug/Release command propagation, `git diff --check` | Complete 2026-09-25 |
| G2 | Remove premature concrete endpoint sources and establish the pre-implementation roadmap gate | O13–O19, O24; GOV-09 | Source/dependency audit, strict Debug/Release CTest, Kuaru consumer build, documentation and `git diff --check` | Complete 2026-09-28; strict Debug/Release CTest 6/6, Kuaru `sinukaru` target and source audit passed |
| G3 | Group private implementation by DSP, logger, metrics and platform while preserving the flat public header API and existing targets | GOV-05, GOV-10 | Strict Linux matrices, Android target builds, full CTest, Kuaru consumer build, path audit and `git diff --check` | Complete 2026-09-29; no public include, namespace, target or behavior change |
| P13A | Specify render capability (Group Q); anticipated-reuse exception to the default scope bar, approved explicitly, no Kuaru/AHIO consumer | Q01–Q04 | Requirements review | Specified 2026-10-16 |
| P13B | Generic capability-detection contract plus render capability: `RenderCapability` enum, `HostFacts::renderCapabilities`, POSIX (Wayland/X11 socket presence) and Apple (`CGSessionCopyCurrentDictionary`, iOS unconditional) probes | Q01–Q04 | `gigantePlatformTests` build/run, standalone real-session probe run, `git diff --check` | Complete 2026-10-16 on macOS: `gigantePlatformTests` passes (including new duplicate-free/audio-always/name-stability/empty-default cases), and a standalone probe on a real interactive console session reported `audio video` both detected. Linux build/probe and Android/iOS device verification not performed locally |
| R0 | cpp-httplib feasibility: pin, security review, fuzz start, TLS and platform builds, capacity, slow-client and lifetime probes; no public API | RC14–15, RC40–41, RC43, RC45, RC48, RC57, RC59, RC62, RC70–76 | Written go/no-go with reproducible evidence per risk | Closed 2026-11-02 with partial coverage, decision go: [feasibility](docs/devreq/rest-engine/feasibility.md). macOS only; Android, iOS, phone capacity and fuzzing not completed |
| R1 | Core server and client, security profiles, HTTPS, routing, errors, common status routes, service lifecycle, C++ API | RC01–04, RC10–18, RC20–24, RC31, RC33–35, RC37, RC40, RC48–49, RC55–59, RC63, RC80 | Android and iOS builds first; loopback and negative tests, sanitizers, header isolation, curl interoperability | Complete 2026-11-02 on macOS: [acceptance](docs/devreq/rest-engine/r1-acceptance.md). Android and iOS compile only |
| R2 | Streaming, delivery policies, binary transfer, `copy*` including `copyList`, diagnostics | RC05–06, RC25–27, RC41–44 | Slow-client and bounded-memory tests, large-file and cancellation tests | Specified 2026-11-02; not started |
| R3 | ~~MQTT client~~ | RC50–51 | None | Removed 2026-11-02 by user decision |
| R4 | Android JNI and iOS integration | RC64–65 | Device builds and lifecycle evidence, recorded per device | Specified 2026-11-02; not started |
| R5 | Stress, sanitizer, fuzz and platform validation | RC62, RC72, RC81–83 | Benchmarks with host details, per-platform acceptance list | Specified 2026-11-02; not started |
| R6 | `deploy` and `execCmd`, only if separately approved | RC28–30 | Dedicated security review, signature tests, artifact audit | Specified 2026-11-02; not started |

The vocoder and every DSP optimization/redesign require a later explicit phase
and `go`; they are not implied by completing P8F–P8J.


## REQ-O20 — C2A resource acceptance boundary

The shared session interface shall expose independent background-audio and
CPU-awake requests, explicit release and read-only observation refresh outside
realtime callbacks. Reports shall distinguish requested intent, native operation
acceptance, observed pending/active/stop-pending/inactive/unavailable/failure
state, ownership and specific operation/cleanup errors. Acceptance alone shall
not assert resource or audio-device readiness. Unimplemented providers shall
report unsupported rather than a successful no-op.

The Android implementation shall refuse adoption of externally active/pending
resources and release only session-owned resources, preserving stop-during-start
handling, idempotence and explicit cleanup retry. Teardown order is CPU-awake,
background-audio, then audio focus; one resource failure shall not silently
abandon other independent resources or impose consumer startup policy.

C2A implementation is host/JVM tested and cross-built. REQ-O20 remains partial:
consumer adoption, physical-device resource acceptance and other platform levers
are separately gated. See [C2A acceptance](docs/devreq/android-session-resources/acceptance.md).
