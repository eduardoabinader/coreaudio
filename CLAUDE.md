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
documentation followed, and Kuaru currently pins core commit
`b39d375d2766920485175fd3cd2e420005e62ce3`. Remote reachability was not
reverified in this workspace. Do not begin P8K/P8I/P8J without a new `go`.
On 2026-09-25 the user further clarified that AHIO requirements are not closed:
until the user explicitly says they are closed, `go` applies only to
Kuaru/coreaudio work and must not start an AHIO stream or DSP integration phase.
P11B is implemented in the core repository: `gigantePlatform` and host facts
exist, with Linux runtime and Android cross-build evidence. Kuaru migration is
still P11H and requires a separate `go`; Apple implementations are not verified
without their toolchains.

## Phase discipline

- Work on one approved phase at a time and stop for `go` before starting the
  next phase.
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
