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
| REQ-N02 | Kuaru and AHIO consume an exact pinned revision of the same authoritative core; copies, translations, and product forks are prohibited. | Yes | Partial | No | Kuaru's recorded gitlink is `ec81288`; the checked-out core is ahead during the current phase and needs a later exact gitlink update; P8I AHIO pin remains |
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
| REQ-O17 | iOS: the inventory and monitor shall use `AVAudioSession` route ports and route-change notifications through Objective-C++, and the session category and mode shall be set explicitly and reported. | Yes | No | No | Not started |
| REQ-O18 | macOS: the bus shall come from the CoreAudio transport-type property, and change events from JUCE's own device-list notification. | Yes | No | No | Not started |
| REQ-O19 | Linux: the bus shall come from ALSA card and sysfs information. Hot-plug shall use udev, inotify or bounded polling, the chosen mechanism recorded with its evidence. It shall run headless, with no display or session bus. | Yes | No | No | Deferred by explicit scope decision: there is no Linux interface or automatic hot-plug requirement; the caller starts `ahioenc`/`ahiodec` only when its requested device is present |
| REQ-O20 | Session preparation shall be one interface with a platform implementation each: Android foreground service, wake and multicast locks and audio focus; iOS audio session and background audio; macOS App Nap prevention; Linux scheduling policy. It hosts the platform levers of REQ-M14, REQ-M111 and REQ-M122, and shall report requested, applied and observed results. | Yes | No | No | Not started |
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
| REQ-P08 | No line shall contain a raw pointer or component address, a user identifier or other personal data. Field values shall be sanitized deterministically, and values that a `store` build must not emit shall be `DEBUG` or `TRACE` only. | Yes | Partial | Partial | Deterministic single-line sanitization is verified; product field classification and store call-site audit await adoption |
| REQ-P09 | Kotlin and Java code shall log through a thin binding that produces byte-identical lines to the C++ logger, or keep `AhioLogger` delegating to it. `AhioLogger` shall remain unchanged until its replacement passes the golden vectors of REQ-P02. Java `Log.*` calls in Kuaru are covered only by the later Kuaru adoption. | Yes | No | No | Not started |
| REQ-P10 | Tests shall cover the golden format vectors, sanitization, field ordering, level filtering at both ceilings, proof that arguments of a compiled-out call are not evaluated, the capture sink, per-platform level mapping, and concurrent logging without interleaving. Store and dev configurations shall each be built and tested. | Yes | Partial | Partial | P12B Linux TRACE/INFO/OFF suites pass 8/8; P12E-K1 adds a Kuaru adapter test plus complete Desktop dev and Android store builds; macOS backend/runtime mapping remains unverified |
| REQ-P11 | Product adoption is separately authorized and traced. Kuaru may adopt `gigante::Logger` without changing the AHIO boundary, replacing `SINU_LOG_INFO`, direct `juce::Logger::writeToLog`, direct native Android calls and later Java `Log.*` while preserving store-build silence. AHIO replacement of `AhioLogger` remains a separate phase after AHIO requirements close and requires its own roadmap and `go`. | Yes | Partial | Partial | P12E-K1 migrates Kuaru C++ only; Java and JUCE `DBG` remain P12E-K2/K3. No AHIO production file changed |
| REQ-P16 | `gigante::Logger` shall use spdlog as its logging engine unless the evaluation below fails; `gigante::Logger` remains the only API products see, so the engine can be replaced without changing a call site. The engine supplies levels, sinks, color, flush and synchronization; facade macros enforce compile-time removal without exposing spdlog headers. The line format, field ordering, sanitization and session id stay in the facade (REQ-P02, P07, P08), and only the finished line is passed to spdlog. P12B shall evaluate and record: (a) the informative size delta on Linux, macOS and Android, including bundled `fmt`; (b) a macOS `os_log` sink built over the callback sink, since spdlog has none; (c) an immutable spdlog pin through the shared FetchContent cache, with bundled or separately pinned `fmt` and no second copy; (d) no realtime reachability and no asynchronous thread pool without a later requirement; and (e) MIT notices for spdlog and `fmt` in every containing artifact. | Yes | Partial | Partial | spdlog 1.17.0 commit `79524ddd08a4ec981b7fea76afd08ee05f83755d`, bundled fmt 12.1.0, synchronous sinks and MIT notices verified; Linux/Android archive sizes recorded in `Memory.md`; macOS size, `os_log` and Apple build remain open |

### P.2 — Build policy

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-P12 | The release types and the generic build-derived facts of `SinuBuildPolicy` are candidates for the shared repository, because AHIO may adopt several release types. Product-specific features (Kuaru keybed, preset choice, demo UI, voice controls, sample data sources) shall stay in Kuaru. The shared part shall be named `gigante::BuildPolicy`, contain no `Sinu` or `Ahio` name, and be specified after `Source/SinuBuildPolicy.h` and `SinuReleaseType.h` are audited for which members are truly product-neutral. | Yes | No | No | Not started |
| REQ-P13 | The shared build policy shall expose, as `constexpr` values derived from compile definitions, at least the release type and the generic facts the logger and diagnostics need: log ceiling, whether diagnostic capture is compiled in, whether the test-automation bridge is compiled in, and whether store hardening applies. A capability that a `store` build excludes shall be excluded at compile time, so no runtime state can enable it (following Kuaru's `kCaptureCompiledIn`). | Yes | No | No | Not started |
| REQ-P14 | Each product shall declare its own release types and map them onto the shared facts in one place. Adding a release type to one product shall not change the behavior of another. An unknown release-type string shall be rejected, never defaulted. | Yes | No | No | Not started |
| REQ-P15 | Until P12 to P14 are approved and implemented, the logger shall take its ceiling from a small log-policy value supplied by the product at build time, so the logger can be delivered without waiting for the shared build policy. Moving `SinuBuildPolicy` out of Kuaru is a separate later phase with its own `go`, parity evidence and a pinned-revision change. | Yes | Yes | Partial | P12E-K1 supplies store→WARN, demo→INFO and dev/profiling→TRACE from Kuaru before adding coreaudio; dev and store builds verify the effective definitions. Shared-policy migration remains P12D |


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
| P8H | Neutral observation plus Kuaru `ProfileCollector` adapter | N26–N30, N32–N33; GOV-01–07 | Metrics parity and disabled-observer overhead | Complete locally 2026-09-25; allocation closeout and later platform phases followed; Kuaru's recorded gitlink is `ec81288`, remote reachability not reverified |
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
| P12D | Audit `SinuBuildPolicy`; `gigante::BuildPolicy` | P12–P14 | Audit record, constexpr tests | Not started |
| P12E | Kuaru adopts logger in separately approved C++/Java/DBG steps; shared build-policy migration remains P12D | P11, P15 | Kuaru adapter test, call-site/realtime audits, dev/store builds, store artifact and device-logcat checks | Partial 2026-09-29: P12E-K1 C++ adoption and store artifact audit pass; Java, DBG and device logcat remain open |
| G1 | Add Kuaru-owned standalone core build/test helpers; no DSP or AHIO change | GOV-05–08 | Shell syntax, help/error paths, Debug/Release command propagation, `git diff --check` | Complete 2026-09-25 |
| G2 | Remove premature concrete endpoint sources and establish the pre-implementation roadmap gate | O13–O19, O24; GOV-09 | Source/dependency audit, strict Debug/Release CTest, Kuaru consumer build, documentation and `git diff --check` | Complete 2026-09-28; strict Debug/Release CTest 6/6, Kuaru `sinukaru` target and source audit passed |
| G3 | Group private implementation by DSP, logger, metrics and platform while preserving the flat public header API and existing targets | GOV-05, GOV-10 | Strict Linux matrices, Android target builds, full CTest, Kuaru consumer build, path audit and `git diff --check` | Complete 2026-09-29; no public include, namespace, target or behavior change |

The vocoder and every DSP optimization/redesign require a later explicit phase
and `go`; they are not implied by completing P8F–P8J.
