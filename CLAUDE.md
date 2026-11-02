# Core Audio — Engineering Context

## Authority

This repository is an independent C++ audio library. This file defines working
rules; `requirements.md` defines normative behavior, `architecture.md` defines
the approved design, and `Memory.md` records durable decisions and verified
results. When documents disagree, stop and resolve the conflict explicitly.

This repository is the authoritative implementation of AHIO Group N
(`REQ-N01`–`REQ-N45`) and the common DSP point for Kuaru and AHIO. Preserve
those IDs and their P8F/P8G/P8H ordering. P8K/P8I/P8J are AHIO-owned consumer
phases; the deferred vocoder does not belong to the initial extraction.

P8F is complete. P8G implementation, bit-exact parity, and controlled
render-budget evidence are complete. On 2026-09-25 the user explicitly
authorized P8H despite the then-open publication gate. P8H began at
`396ce772a7ddb3117638504eee7733ef256d9ff6`; allocation verification and G1
documentation followed. Kuaru currently records core commit `8ee15a7`, while
this checkout is at `236630a`; do not claim exact clean-checkout pinning until
that gitlink is deliberately reconciled. Remote reachability was not reverified
in this workspace. Do not begin P8K/P8I/P8J without a new `go`.
On 2026-09-25 the user further clarified that AHIO requirements are not closed:
until the user explicitly says they are closed, `go` applies only to
Kuaru/coreaudio work and must not start an AHIO stream or DSP integration phase.
P11B is implemented in the core repository: `gigantePlatform` and host facts
exist, with Linux runtime and Android cross-build evidence. The P11H Kuaru
host-facts step is implemented: Kuaru consumes the shared target through an
explicit product adapter, and its old platform singleton is removed. Android
runtime fact parity, Apple implementations, the exact post-commit gitlink, and
all later mechanism candidates remain unverified or separately gated.
P11C added only the neutral endpoint model and declarative selection policy
with fake-inventory tests. The later authorized P11H core-only step implements
the neutral `EndpointMonitor` lifecycle required by REQ-O15. It does not
implement an Android JNI source, migrate Kuaru's headset monitor, or authorize
AHIO adoption; those remain separate steps requiring another `go`.
Concrete endpoint sources are outside the current core scope. Linux/macOS
callers start `ahioenc`/`ahiodec` only after product-owned logic confirms that
the requested JUCE device exists. Do not add discovery, bus classification,
hot-plug monitoring or Apple endpoint code without a separately reviewed
requirement and phase roadmap.
P12B now provides the product-neutral `giganteLog` foundation. It encapsulates
the pinned spdlog engine, keeps requested/build/effective levels distinct,
uses an explicitly injected process session-id sequence, and compiles spdlog
out entirely when the build ceiling is `OFF`. Linux and Android have build
evidence; macOS remains unverified. P12B by itself does not authorize product
adoption, an Apple `os_log` sink, or logging from a realtime callback.
P12E-K1/K2/K3 authorize and implement Kuaru's C++, Java and JUCE-diagnostic adoption:
Kuaru owns the logger/session context, maps its release types to the build
ceiling, links `gigante::log`, and routes its Java lifecycle/UI events through
a product-owned JNI binding. Runtime Store device logcat remains open. On
2026-09-30 the user deferred P12D, so `SinuBuildPolicy` remains product-owned
and the verified provisional log-ceiling mapping stays in force. AHIO adoption,
Apple work and realtime logging are still unauthorized.

## Phase discipline

- Work on one approved phase at a time and stop for `go` before starting the
  next phase.
- Before implementation, present a roadmap naming the objective, requirement
  IDs, affected repositories/files, non-goals, acceptance checks and expected
  documentation updates. Wait for a new `go` after that roadmap; an earlier
  generic authorization does not approve work the user has not yet reviewed.
- At the end of every phase, update all affected documentation and report the
  exact checks that ran.
- Never commit. Provide a detailed proposed commit message instead.
- Preserve stable requirement IDs and trace every phase and acceptance test to
  them.
- Track **specified**, **implemented**, and **verified** independently. A
  successful compile is implementation evidence, not behavioral verification.
- Record durable decisions and reproducible verification evidence in
  `Memory.md`; do not record assumptions as facts.
- Run `git diff --check` in every changed repository before handoff.

## Code and architecture

- Public API uses the single namespace `gigante`.
- Types use `UpperCamelCase`. Functions, methods, members, variables, and
  constants use `lowerCamelCase`.
- No internally controlled symbol begins with `_`; do not add new `Sinu` names.
- Public headers contain no product conditionals or product types.
- Keep public headers together under `include/gigante`. Group implementation
  files and private headers by architectural responsibility under `src/dsp`,
  `src/logger`, `src/metrics`, and `src/platform`. A physical source group does
  not create a nested public namespace or authorize a new target dependency.
- Dependencies flow from products to the core. The core never depends on Kuaru
  or AHIO.
- Prefer small interfaces, explicit responsibilities, composition, RAII, and
  deterministic lifecycle.
- `prepare`, `reset`, and teardown operations must be repeatable and safe.
- Do not infer a selection, silently fall back, or hide global state.
- Requested and effective configuration must remain distinguishable.
- Errors preserve the specific cause, relevant context, and terminal state.

## Realtime contract

On a realtime callback, perform no allocation, locking, blocking I/O, logging,
or formatting. Do not hide a FIFO, whole-buffer copy, block accumulation, or
thread hop behind an abstraction. All buffers, queues, and work are bounded and
observable. Report algorithmic latency in samples.

Realtime observation uses preallocated data and bounded publication. Logging
and serialization happen outside the callback. Recovery from non-finite samples
or invalid coefficients is deterministic and counted. Inject clocks or timing
sources when deterministic tests require them.

## Build and dependencies

- Use modern target-based CMake; keep library, tests, and optional adapters
  separate.
- Enable strict warnings for project code, preferably with warnings as errors.
  Isolate dependency warnings instead of disabling warnings globally.
- Do not vendor or build a second JUCE version. If extracted DSP requires JUCE,
  consumers provide the explicitly documented pinned version through CMake.
- Never depend on the physical checkout layout of a consumer.
- Consumers pin an exact core revision. Generate reproducible revision identity
  where applicable.
- Kuaru offers `build_coreaudio`/`build_coreaudio_all` only as consumer-side
  convenience wrappers for standalone JUCE-free builds. They are not core API,
  do not establish a checkout-layout dependency in this repository, and do not
  replace production JUCE verification in the Kuaru build.

## Testing

Tests are deterministic, fast, and hardware-independent. Fixtures record core
revision, parameters, sample rate, channels, block size, seed, origin, and
tolerance. Cover fixed, small, and variable blocks; supported mono/stereo
contracts; repeated lifecycle; bypass; smoothing; tails; live changes; and
invalid-input recovery.

Golden vectors are reviewed artifacts. Never update them automatically.
Distinguish bit-exact checks from tolerance checks, and diagnose/document every
difference. Core tests must not depend on UI, devices, network, MIDI, or product
JSON configuration.

## Scope boundary

Kuaru retains parameter adapters, `ProfileCollector`, synthesis, and piano
policy. AHIO retains adapters, metrics, devices, routes, codecs, and transports.
The core provides neutral contracts only. Preserve the current chain and sound
before considering optimization or DSP redesign.

Android Activity/JNI, RPC, Python CLI, device discovery, foreground services,
and UI are not core infrastructure. Extract product infrastructure only when it
is demonstrably neutral and required by this library.


## iOS session preparation — phase A (2026-10-30)

The approved phase adds the shared `SessionPreparation` contract and iOS native
backend only. See architecture.md for lifetime, threading and process lease
rules. REQ-O20 and REQ-O17 remain partial: physical behavior, Kuaru adoption,
route-port inventory and all other platform implementations are not verified.
The next Kuaru integration needs its own roadmap and standalone `go`. After iOS
migration, prepare a separate Android reuse/parity roadmap using the existing
android_audio_runtime helpers and Kuaru audio-focus seam; do not migrate them
as a side effect of iOS work. Session activation must never be treated as JUCE
device readiness, and this layer must not duplicate JUCE's recovery loop.


## Android audio-focus migration — C1 (2026-11-02)

The approved C1 phase adds AndroidSessionPreparation and its JNI driver, explicit
context/class factory and small shared Java listener. REQ-O20 remains partial.
Only focus migrates; locks, foreground service, scheduling, endpoint inventory
and AHIO adoption remain outside C1. Use the neutral injected backend tests and
track native/device results separately. Kuaru preserves its nonfatal denied-focus
startup policy. Gain is a fact, not a device-recovery command. The next C2 phase
needs a new roadmap and standalone go. No commits are automatic.


## Android session resources — C2A (2026-11-02)

C2A adds only the neutral per-resource contract and Android JNI service/wake
driver, plus additive shared Java snapshots. Focus-only factories and iOS
preparation retain their semantics. REQ-O20 remains partial; host/JVM fixture and
Android/iOS build evidence is not physical service qualification. Kuaru resource
ownership adoption is C2B and needs a separately approved roadmap. Do not switch
consumer calls, add multicast/scheduling or change UI/audio policies in C2A.


## Android scheduling comparison — C3A

C3A compares Kuaru legacy scheduling against JUCE-only scheduling on each
Android backend. It adds no shared scheduler API, new iOS mechanism or superclass.
Keep legacy as the default and retain the existing shared runtime for other
consumers. Core source remains unchanged by this experiment. Do not migrate or
retire mechanisms based only on requested priority, app timing heuristics, or
unpopulated latency fields. See docs/devreq/android-thread-scheduling/audit.md
and the consumer acceptance ledger before proposing a later implementation.


## Kuaru scheduling retirement — C3B

The user approved C3B on 2026-11-02, superseding C3A's legacy-default boundary
for Kuaru only. Remove the consumer optimizer, first-callback guard/preparation,
custom scheduler linkage and comparison selector; rely on existing JUCE/Android
scheduling. Keep shared android_audio_runtime unchanged for other consumers.
No new core scheduler, iOS mechanism, backend/DSP/session policy change or AHIO
adoption is authorized. Record builds, artifact rejection of legacy linkage,
matched native counters and lifecycle/listening evidence independently; retain
Bluetooth and LookAndFeel defects as deferred. Broader qualification and other
platform mechanisms keep REQ-O20/O21 partial.
