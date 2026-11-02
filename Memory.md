# Core Audio Memory

This file records durable decisions and reproducible verification results.
Requirements belong in `requirements.md`; intended design belongs in
`architecture.md`. Every verification entry must state date, environment,
procedure, and result. Assumptions are not facts.

## 2026-09-24 — G0 governance baseline created

### Specified

- The repository is an independent C++ audio core at the local checkout
  `modules/coreaudio`.
- This repository is the authoritative implementation location for AHIO Group
  N. Its stable IDs `REQ-N01` through `REQ-N45` are preserved locally.
- Public namespace and naming, one-way product dependencies, deterministic
  lifecycle, realtime safety, dependency isolation, deterministic fixtures,
  golden governance, and product boundaries are recorded under those IDs.
- Repository-only practices use `GLC-GOV-01` through `GLC-GOV-07` rather than a
  competing set of product requirement IDs.
- Work follows G0 then AHIO's P8F/P8G/P8H core phases. P8K/P8I/P8J remain AHIO
  consumer phases. Only G0 is currently authorized.
- The future vocoder, algorithm improvements, and DSP redesign are outside the
  authorized scope.

### Implemented

- The four required governance documents now exist.
- The requirements matrix independently tracks specified, implemented, and
  verified state and maps Group N to phases and acceptance evidence.
- No library, build target, test executable, adapter, DSP, dependency, or
  product integration has been implemented in G0.

### Verified

- All four required documents exist and are non-empty.
- `requirements.md` contains exactly one primary matrix row for every stable
  ID from `REQ-N01` through `REQ-N45`; no stale `GLC-REQ` or replacement P1–P5
  phase IDs remain.
- `git diff --no-index --check /dev/null <document>` passed for every new
  untracked document. This form was necessary because an ordinary Git diff does
  not inspect untracked files in a repository with no first commit.
- Parent Kuaru `git diff --check` passed. Git emitted its pre-existing
  fsmonitor IPC warning, but the command returned success and no whitespace
  error.
- These checks verify document structure, traceability, and whitespace only.
  No build or DSP behavior exists to verify in G0.

### Durable decisions

- The checkout/repository identifier is `coreaudio`. Earlier planning and
  bootstrap labels refer to the same intended core and are not another library
  or fork.
- Consumer documentation is reference material, not automatically copied
  policy. Core requirements contain only neutral rules needed by this library.
- Migration order is characterization, Kuaru-only extraction/parity,
  product-neutral observation, then AHIO neutral/bypass adoption.

## 2026-09-24 — Kuaru/AHIO documentation and AHIO history audit

### Sources read

- Kuaru: `CLAUDE.md`, `memory/MEMORY.md`, `memory/architecture.md`, and the
  DSP/golden/realtime portions of `memory/sinukaru_requirements.md`, including
  the shipped processing order, `ProfileCollector`, offline render, golden
  render gap, lifecycle, invalid recovery, and future vocoder boundary.
- AHIO: `Memory.md`, `ahio-requirements.md`, `ahio-architecture.md`, and
  `README.md`, with Group N, P8F–P8J/P8K ordering, the USB baseline, local and
  distributed stream boundaries, and the deferred vocoder reviewed.
- Metadata, changed-file inventories, and diffs for AHIO commits `6f3e0a7`, `a6a2cac`,
  `167f9a2`, `b2f2100`, `9fa103e`, `8b255fc`, `c1af809`, `9db5e13`,
  `fe5f2d8`, and `71ba027` were inspected in chronological context.

### Findings adopted

- `c1af809` introduced REQ-N01–N35 and P8F–P8J. These IDs are the canonical
  core contract and must not be replaced by local duplicates.
- `fe5f2d8` restored/added deferred REQ-N36–N45 for a future two-input vocoder;
  those requirements do not authorize work in P8F–P8J.
- `71ba027` inserted P8K between core parity and AHIO USB adoption and made the
  local/distributed stream topology explicitly AHIO-owned. No stream, packet,
  codec, jitter, endpoint, or branch type belongs in this core.
- The P8E commits from `a6a2cac` through `8b255fc` reinforce explicit requested
  versus effective identity, transactional selection, no silent fallback,
  directional terminal errors, and fake-versus-hardware evidence separation.
  Those principles inform neutral contracts; their Android/USB implementations
  remain outside this repository.
- `6f3e0a7` reinforces that partial/aborted evidence is retained and labeled,
  not promoted to verification. `9db5e13` and the Wi-Fi portions of `71ba027`
  remain AHIO transport context, not core implementation scope.

### Verification state

- This was a source/document/history audit. It verifies the provenance and
  scope mapping of the G0 documentation only; it verifies no DSP behavior,
  realtime property, build, or consumer integration.

## 2026-09-24 — P8F core skeleton and Kuaru characterization completed

### Specified

- P8F is limited to the independent build/API/test skeleton and deterministic
  pre-extraction Kuaru references. It moves no production effect.
- The initial processor supports explicit mono/stereo non-owning PCM views,
  requested/effective preparation state, repeatable lifecycle, neutral bypass,
  zero algorithmic latency, bounded observer calls, and deterministic counted
  replacement of non-finite PCM with zero.
- Requesting the future effects mode before P8G returns the specific terminal
  result `effectsNotImplemented`; it never silently falls back to bypass.

### Implemented

- Target-based CMake defines the C++17 static target `giganteCore` and alias
  `gigante::core`, separate test executables, scoped strict warnings with
  warnings-as-errors, and explicit `GIGANTE_REVISION` build identity.
- Public headers are product-neutral, JUCE-free, and use only namespace
  `gigante`. The implementation has no FIFO, thread, lock, allocation, I/O,
  logging, formatting, product conditional, or whole-buffer copy in `process`.
- Kuaru's offline renderer gained an opt-in raw float32-LE output and an
  explicit per-voice deterministic detune seed. Ordinary playback still uses
  the previous system-random detune path. The seed is stored per voice; no
  global random state was added.
- The capture script requires explicit absolute Kuaru binary/config/sample
  paths, refuses to overwrite an existing candidate manifest, launches a new
  process per vector, and writes only to the caller's candidate directory.
- Five reviewed float32 goldens cover transient/tail at blocks 64 and 257,
  sustained/live changes at blocks 7 and 512, and oversized-parameter recovery
  at block 64. Seed `1592594996`, rate 48000 Hz, stereo layout, fixture hashes,
  sizes, origin, comparison policy, and expected hashes are recorded.

### Diagnosis and decisions

- The first WAV-hash experiment differed by one 24-bit output LSB because the
  JUCE writer dithers float-to-integer conversion. WAV remains auditionable,
  but raw PCM immediately after `processBlock` is the bit-exact authority.
- The first raw-float repeat also differed materially. The cause was Kuaru's
  note-on detune using `juce::Random::getSystemRandom()`. This was not hidden by
  a tolerance: a test-only explicit seed seam was added and normal playback was
  left unchanged.
- Different block sizes are independent goldens, not expected to hash equally,
  because product parameter changes are callback-granular. Same fixture,
  geometry, seed, binary, and toolchain are required to be bit-exact.
- Promotion was manual after two full fresh-process captures matched for all
  five vectors. Capture has no path that updates committed goldens.

### Verified

- The Kuaru desktop target built successfully with GCC 16.2.1. Warnings shown
  during that product build came from JUCE/generated or existing product code;
  the independent core applies `-Werror` only to its own targets.
- Two complete seeded captures produced identical SHA-256 values for every
  vector. An additional same-run fresh-process reset repeat also matched
  `transientTailBlock64` bit-for-bit.
- FFmpeg `astats` inspected the invalid-recovery raw vector: 108000 frames per
  channel, 0 NaNs, 0 infinities, and 0 denormals.
- The committed hashes are verified by `giganteGoldenHashVerification`; format,
  exact size, and finite samples are verified by `giganteGoldenVectorTests`.
- Final core configure/build with `GIGANTE_WARNINGS_AS_ERRORS=ON` and revision
  `p8f-working-tree` passed under GCC 16.2.1. CTest passed 3/3:
  `giganteCoreTests`, `giganteGoldenVectorTests`, and
  `giganteGoldenHashVerification` (0.07 s total on the final run).
- Kuaru was rebuilt after the documentation/header closeout. A fresh final
  five-vector capture plus its internal reset repeat passed, and `cmp` matched
  every final raw vector byte-for-byte with the committed goldens.

## 2026-09-24 — P8G chain extraction implemented; exact-pin gate remains

### Specified

- P8G moves only Kuaru's fixed production effects chain. AHIO adoption and P8H
  metrics/snapshot work remain unauthorized until a later `go`.
- Kuaru retains synthesis, MIDI, packs, instrument gain/headroom, active-voice
  policy, headset cap, output-mute request, and its `ProfileCollector` adapter.

### Implemented

- `EffectsChain` now owns the ordered tremolo, growl control, filter, growl
  post-filter, drive, bell peak, phaser, chorus, reverb, noise, soft limiter,
  and output-gain stages plus their smoothing, delay, envelope, filter,
  limiter, crossfade, and deterministic noise state.
- Production DSP is selected only by an explicit consumer-provided JUCE target.
  Standalone builds retain the JUCE-free neutral implementation; public headers
  contain no product or JUCE conditional.
- Kuaru supplies neutral parameters, explicit source level, noise-enable and
  output-mute policy, and maps neutral stage callbacks back to
  `ProfileCollector`. The prior 680-line product DSP implementation was removed.
- `AudioParamController` no longer has the one-value
  `SinuAudioParamBehaviorMode` enum. Protocol output remains `Direct`, and the
  direct resolver now names requested/effective assignments explicitly.

### Diagnosis and decisions

- The first candidate differed in all vectors. No golden was changed. The cause
  was JUCE reverb initialization order; setting parameters after `prepare`
  entered JUCE's internal ramp from a different initial dry level.
- The live-change vectors then diverged at filter automation because the first
  adapter version dirtied piano voices for core-only parameters. Restoring the
  previous ownership classification removed that difference.
- The remaining divergence began exactly at phaser enable. Kuaru had set mix
  before the final phaser reset; preserving that order restored exact output.

### Verified

- Standalone core configure/build with strict warnings passed; CTest passed 3/3.
- Kuaru desktop configured and built with its pinned JUCE 8.0.8 supplied through
  the explicit interface target. Warnings observed were in existing JUCE/product
  sources, not the strict core target.
- A fresh capture from relinked Kuaru `5de7820` and its fresh-process reset
  repeat completed. `cmp` matched all five post-migration float32 vectors
  byte-for-byte with the reviewed P8F goldens at block sizes 7, 64, 257, and
  512.
- A controlled Linux x86_64/GCC 16.2.1 render-budget comparison used an exact
  `git archive` of pre-migration Kuaru `0497c33` against Kuaru `5de7820` with
  core `2aada7e`. Both were release-optimized developer builds using JUCE
  8.0.8, 48 kHz stereo, eight voices, seed `1592594996`, the same
  `live-changes.jsonl`, and CPU 4 affinity. Fifteen alternating pairs per block
  rendered three seconds each. At block 7, baseline/current medians were
  48.2656/49.6743 ms (+2.92%); at block 512 they were 24.5156/24.9065 ms
  (+1.59%). The candidate remained 60.39x and 120.45x realtime, respectively,
  and the output stayed bit-exact. Both deltas pass the reviewed 5% aggregate
  render-budget tolerance. The additional work consists of per-block parameter
  validation and neutral observer dispatch plus bounded full-buffer non-finite
  scans; the larger delta at block 7 is consistent with the per-block part.
- This offline comparison covers deterministic migration cost, not scheduler,
  device, xrun, or disabled-profiler overhead; those remain P8H/product
  evidence. Raw timing rows are retained at
  `/tmp/p8g-benchmark-final-20260924.tsv` for this handoff.
- On 2026-09-25 the authoritative repository was named `coreaudio` and
  integrated into Kuaru at `modules/coreaudio`. The local Kuaru gitlink pins
  commit `3699b89bbe4c9efdf06a4fa56b55935b65d7faf9`; repository naming in core,
  Kuaru, and AHIO documentation was reconciled in the same closeout work.
- Remote verification found `origin/main` at the unrelated root commit
  `be786ec` (`Initial commit`, containing only `.gitignore`). The pinned
  `3699b89` history is not reachable from that remote ref, so a fresh clone
  cannot yet materialize the gitlink. No force-push, history rewrite, merge, or
  remote branch creation was performed implicitly.
- Kuaru now derives `GIGANTE_REVISION` from this checkout and adds a stable
  dirty-source digest when applicable; it no longer misidentifies the core with
  the Kuaru product revision.
- P8G still awaits remote reachability of its pinned commit. No P8H work has
  started.

## 2026-09-25 — P8H neutral realtime observation

### Authorization and boundary

- The user explicitly said to continue after the remote-publication blocker was
  restated. P8H therefore proceeded as a documented exception; this did not
  authorize a push, history rewrite, AHIO integration, P8I, or P8J.
- Kuaru still owns `ProfileCollector`, all log/JSON/WAV output, requested product
  configuration, and product policy. The core remains a static neutral library.
- After P8H, the user clarified that AHIO requirements are not yet closed and
  Kuaru plus coreaudio are the current priority. No `EffectsChain` integration,
  P8K stream-role implementation, P8I, or P8J work may begin in AHIO until the
  user explicitly closes those requirements. A briefly started P8K worktree
  experiment was fully removed; no AHIO code from it remains.

### Implemented

- `RealtimeMetricsObserver` uses an injected raw-tick `TickClock`, fixed stage
  arrays, and an eight-slot SPSC snapshot queue. Only lock-free 32-bit atomic
  queue indices cross threads. Queue saturation drops a publication and counts
  it; it never allocates, locks, blocks, logs, formats, or performs I/O.
- Stable snapshots contain input/post-chain/output peak and RMS, finite sample
  counts, per-stage call/total/maximum ticks, maximum callback ticks, callback
  budget exceedance, algorithmic latency, explicit resets, invalid parameters,
  recovery, non-finite samples, bypass state, first-sample position, tick rate,
  and dropped publications.
- Budget conversion is explicit outside the callback: consumers provide raw
  ticks per sample. Signal observation receives a synchronous read-only PCM
  view. `EffectsChain` still accepts `nullptr`; Kuaru returns `nullptr` from its
  adapter when profiling is compiled out and retains the exact legacy stage
  mapping when profiling is enabled.

### Verified

- GCC 16.2.1 strict standalone configure/build passed and CTest passed 3/3.
  Deterministic fake-clock tests cover every stage, three signal points, reset,
  budget exceedance, invalid/non-finite recovery, queue overflow, and stable
  draining; public-header isolation includes the metrics API.
- Kuaru's profiling-enabled desktop target rebuilt successfully with the pinned
  JUCE 8.0.8. A fresh five-vector capture and reset repeat retained every
  reviewed float32 SHA-256 exactly at blocks 7, 64, 257, and 512.
- Disabled-observer overhead was measured with release-optimized standalone
  cores before and after P8H, CPU 4 affinity, identical neutral stereo buffers,
  and ten alternating 15-sample median runs. Median totals were
  36.33465/37.24585 ms for 2,000,000 block-7 calls (+2.51%, about 0.46 ns per
  call) and 31.55775/31.67775 ms for 100,000 block-512 calls (+0.38%, about
  1.20 ns per call). Both are inside the already reviewed 5% aggregate budget.
- A test-local global allocation probe now counts ordinary, array and aligned
  C++ allocations only on the thread executing `EffectsChain::process`.
  Thirty-two prepared stereo blocks with the bounded metrics observer produced
  zero allocations in both the JUCE-free neutral target and the production
  JUCE chain linked from the rebuilt Kuaru consumer artifacts. The production
  run enabled tremolo, filter, drive, chorus, reverb and output gain and passed
  all ten lifecycle/processing tests. This closes the remaining REQ-N25
  allocation-instrumentation evidence without adding instrumentation to the
  shipping library or callback.
- A desktop `demo` verification attempt exposed an existing product build-policy
  failure: Kuaru applies C++-only `-fvisibility-inlines-hidden` to mbedTLS C
  sources while that dependency uses `-Werror`. No out-of-scope build-policy
  change was made; the disabled `nullptr` branch is source-audited and the core
  path is measured independently.
- The user subsequently committed P8H as
  `396ce772a7ddb3117638504eee7733ef256d9ff6`; Kuaru commit `95d205d` pins that
  exact core gitlink. Remote reachability was not reverified during this
  closeout, and AHIO consumption remains explicitly unauthorized.

## 2026-09-25 - Group O platform layer specified (no code)

REQ-O01 to REQ-O24 added to `requirements.md` (mirror of AHIO Group O): a separate `gigantePlatform` target
beside `giganteCore`, clarifying REQ-N06 without changing it; JUCE as the only audio backend; platform facts
separate from a shared declarative endpoint policy; per-platform fact sources; phases P11A to P11H. Nothing
implemented or verified. The Kuaru migration (P11H) needs its own go and is blocked, like P8G, on the pinned
history being reachable from the remote.

## 2026-09-25 — G1 consumer-side standalone build helpers

- Kuaru now owns `build_coreaudio` and `build_coreaudio_all` in its `utils.sh`,
  with `b_core` and `b_core_all` aliases. This is consumer tooling and creates
  no dependency from this repository back to Kuaru.
- The helpers configure the JUCE-free standalone target with warnings as
  errors, tests enabled by default, separate Debug/Release build trees, and an
  explicit revision composed of the exact commit plus the stable tracked-diff
  digest when dirty. Production JUCE coverage remains in Kuaru's build.
- `bash -n`, help/error checks, and simulated CMake/CTest calls verified option
  parsing, Debug/Release paths, clean/no-tests behavior, both-config sequencing,
  and revision propagation. No compilation was run in this tooling phase and
  no AHIO file was changed.

## 2026-09-25 — P11B platform target and host facts

### Implemented

- P11B started from core commit `b39d375d2766920485175fd3cd2e420005e62ce3`,
  which is the exact gitlink currently pinned by Kuaru.
- Added the static `gigantePlatform` target (`gigante::platform`) beside and
  independent of `giganteCore`. The DSP target does not link it.
- CMake selects one implementation for Android, iOS, macOS, or Linux. Any
  unsupported system fails configuration and names `CMAKE_SYSTEM_NAME`.
- `HostFactsSource` is explicitly created and RAII-owned; there is no hidden
  singleton. It reports platform name/type, logical cores, physical RAM,
  architecture, OS build fingerprint, source revision, and the injected
  realtime-scheduling result.
- Requested and observed scheduling policy/priority remain separate. The facts
  source records the supplied result and performs no scheduling request or
  policy selection.
- Unavailable facts are `std::optional` and format as `unavailable`; the layer
  does not silently substitute one core, zero memory, or an invented identity.
- Android reads `ro.build.fingerprint`; Linux uses `uname`; Apple sources use
  `sysctl`. No public header contains a platform conditional. Kuaru and AHIO
  were not migrated or modified.

### Verified

- Strict Debug and Release Linux standalone builds passed with GCC 16.2.1 and
  CTest 4/4 in each configuration.
  Tests cover real facts, explicit scheduling round-trip, stable names,
  unavailable-value formatting, repeated independent source lifecycle, and
  public-header isolation.
- Android arm64 `gigantePlatform` cross-built with NDK 29.0.13599879,
  `ANDROID_PLATFORM=android-29`, Clang 20.0.0, and warnings as errors.
- The existing Kuaru desktop build was reconfigured and its isolated
  `gigantePlatform` target built successfully through Kuaru's pinned JUCE 8.0.8
  interface target. No product source linked or consumed the platform API.
- A `CMAKE_SYSTEM_NAME=Generic` configure was required to fail and produced
  `gigantePlatform does not support target platform 'Generic'`.
- macOS and iOS implementations are present but are not marked verified because
  neither toolchain is available in this Linux workspace. Consumer measurement
  publication, device facts/policy, and Kuaru migration remain later phases.

### Commit-integrity follow-up

- The user-created P11B commit `dd7f4e0` captured the CMake and documentation
  changes but omitted every newly created P11B source/header/test file. Those
  files remain untracked and are required for that commit's CMake graph to
  configure and build. No history rewrite or commit was performed by the
  assistant. They must be added in a corrective commit or an explicitly chosen
  amend before a clean checkout can build P11B.
- Kuaru's committed gitlink still points to `b39d375`; neither `dd7f4e0` nor
  the later work is pinned by the product yet.

## 2026-09-25 — P11C neutral endpoint policy

### Implemented

- Added neutral per-direction endpoint facts: endpoint identity, optional
  platform id, JUCE device name, optional product name, bus, channels, and
  supported sample rates. A generation-numbered inventory snapshot is the only
  policy input.
- Added a declarative role policy for required direction, allowed buses,
  minimum channels, and minimum sample rate. Evaluation is platform-independent
  and performs no discovery, I/O, device opening, or selection side effect.
- Requested and effective endpoint identities are distinct. Without an
  explicit request, zero, one, and several eligible candidates produce distinct
  statuses but no effective endpoint.
- Every inventory entry retains its disposition and all applicable rejection
  reasons. Duplicate identities reject all duplicates. Unknown bus is rejected
  unless that exact identity was explicitly requested. Invalid policy rate is
  a distinct terminal error.
- The test fake replaces complete inventories and increments their generation.
  A loss/substitution sequence proves the replacement remains `notRequested`
  until the caller explicitly requests it.

### Verification

- Strict Linux Debug and Release standalone builds passed with CTest 5/5 in
  each configuration. The new hardware-free suite covers zero/one/several
  candidates, explicit selection, all eligibility reasons, unknown bus,
  duplicates, loss and substitution, duplex direction identity, missing and
  invalid identities, invalid policy, stable names, and inventory generations.
- Android arm64 `gigantePlatform`, including the common policy source,
  cross-built in Release with NDK 29.0.13599879 and warnings as errors. The
  isolated Kuaru-consumer target also rebuilt through its JUCE 8.0.8 interface
  target; no Kuaru source consumes the endpoint contract.
- P11C does not implement a platform inventory backend, monitoring thread,
  consumer session freeze, transport derivation, Kuaru adoption, or any AHIO
  change. Those remain separately gated phases.

## 2026-09-27 — P11B/P11C platform duplication closeout

- Review found two internal copies: Linux/Android repeated core-count, memory,
  and `uname` reads; macOS/iOS repeated all `sysctl` helpers and fact assembly.
- Added internal composition only: `PosixHostFactsSupport` owns the common
  Linux/Android mechanics, `AppleHostFactsSupport` owns the common macOS/iOS
  mechanics, and `FunctionHostFactsSource` owns the repeated adapter lifecycle.
- Final platform files now contain only actual differences. Android replaces
  the POSIX kernel fingerprint with `ro.build.fingerprint`; Linux keeps the
  kernel release/version; macOS and iOS select distinct platform names.
- The user clarified that `gigantePlatform` is the shared target that receives
  Kuaru's current `SinuPlatform` responsibilities, not the name of a public
  platform-identity enum. The unnecessary enum was removed; `HostFacts` keeps
  the required `platformName` fact.
- P11C public names now match the shared contract: `AudioEndpoint` and
  `AudioBus` replace the locally invented endpoint-facts and bus names.
- The temporary duplication between these neutral facts and Kuaru's existing
  `SinuPlatform` remains intentional until P11H. No Kuaru or AHIO source was
  migrated or modified by this closeout.
- Strict Linux Debug and Release CTest passed 5/5 in each configuration;
  Android arm64 Release cross-built with NDK 29.0.13599879 and warnings as
  errors; and the isolated Kuaru-consumer `gigantePlatform` target rebuilt
  through its pinned JUCE interface. Apple remains unverified without an
  available Apple toolchain.

## 2026-09-25 - Group P logger and shared build policy specified (no code)

REQ-P01 to REQ-P15 added to `requirements.md`, phases P12A to P12E, and a planned-layers section plus ADR-008
in `architecture.md`. `giganteLog` is a provisional separate target; the logger keeps the AHIO line format as a
golden-vector contract, has five levels with compile-time and runtime ceilings, per-platform backends and no
realtime API in v1. The build policy move is limited to the product-neutral part of `SinuBuildPolicy`, after an
audit. Kuaru adoption is REQ-AL01 to AL04 in `memory/sinukaru_requirements.md`. Nothing implemented or verified.

REQ-P16 added: spdlog is the planned engine behind `gigante::Logger`, gated by the P12B evaluation (store size, `os_log` sink, pin, realtime reachability, license). Not measured.

## 2026-09-27 — P11H Kuaru host-facts adoption

- Kuaru removed its platform singleton and consumes `gigante::platform`
  through an explicit product-owned adapter. The shared target remains free of
  Kuaru types and `giganteCore` remains independent from it.
- The Kuaru source revision is deliberately substituted only while formatting
  the product diagnostic; `HostFacts::sourceRevision` continues to identify
  the Core Audio build. Kuaru's `ProfileCollector` receives the already
  formatted string before callbacks begin.
- Kuaru's Android screen-awake JNI diagnostic stayed product-owned because it
  calls Kuaru `MainActivity`. Headset monitoring, audio focus, wake lock,
  foreground service and `android_audio_runtime` were not migrated.
- Core Audio now supports the consumers' CMake 3.22 baseline without the
  3.23-only `FILE_SET HEADERS`; public includes remain target-scoped.
- Strict standalone Debug and Release CTest passed 5/5. The full Kuaru Linux
  desktop build and Android store APK build/verification passed. Android host
  facts were not exercised at runtime because no ADB device was connected;
  Apple remains unverified. The exact post-commit Kuaru gitlink is pending.
- No AHIO change belongs to this P11H step.

## 2026-09-27 — P11H neutral endpoint-monitor lifecycle

- Added `EndpointMonitor`, composed from an injected `EndpointFactSource` and
  `EndpointChangeSource`. Platform signals contain no selection or partial
  device policy; every signal triggers a complete generation-bearing inventory
  snapshot on the source's documented non-realtime thread.
- Requested and effective running state are distinct in the start result.
  Empty callbacks, unavailable sources, denied permission and generic source
  failure have stable statuses and retain diagnostic context. A fact-source
  exception becomes a `snapshotFailed` event rather than an empty inventory.
- Lifecycle is deterministic: start publishes the initial snapshot; repeated
  start neither registers twice nor replaces the callback; repeated stop is
  harmless; stop rejects new delivery, unregisters, and waits for an in-flight
  callback; callback-triggered teardown does not wait for its own stack frame;
  stale signals after stop or destruction are ignored safely.
- The hardware-free suite covers initial/change snapshots, generations,
  repeated lifecycle, specific failures, in-flight teardown, late events,
  destructor cleanup and stable names. Standalone Debug and Release CTest pass
  6/6 with strict warnings; the monitor test also passes under ThreadSanitizer.
  Android arm64 `gigantePlatform` cross-builds with NDK 29 and `-Werror`.
- This is REQ-O15 and a contract slice of REQ-O14 only. No Android JNI
  inventory/change source, Kuaru headset migration, AHIO change, device test or
  product policy was added. Those remain separately gated.

## 2026-09-28 — EffectsChain collaboration documented

- Expanded `architecture.md` with the implemented `EffectsChain` facade,
  private implementation factory/strategy, JUCE and neutral selections,
  lifecycle, non-owning PCM call, deterministic recovery and independent
  observer hierarchy.
- Recorded the exact Kuaru ownership/callback collaboration and fixed DSP
  order, including the growl/filter relationship, state ownership and the
  verified migration boundary. The document also records two current consumer
  limitations: Kuaru does not consume `Status`/`ProcessReport`, and its legacy
  profiling buckets do not map every neutral stage one-to-one.
- Recorded the specified but unimplemented AHIO placement for local USB and
  distributed decode: `EffectsChain` follows `AhioStreamDecoder` and precedes
  AHIO endpoint policy. AHIO adapters, voice calibration, transport and reports
  remain product-owned and separately gated.
- Mirrored AHIO Group Q only as an architectural boundary: optional future
  pitch correction would be a separate processor before `EffectsChain`, while
  accompaniment metadata/reference acquisition remains AHIO-owned. This does
  not add a core requirement, promise a core API, or authorize implementation.
- Documentation only; no C++ source, build target, runtime behavior, golden
  vector or verified phase status changed.

## 2026-09-28 — Concrete endpoint sources removed and roadmap gate added

- The initial platform endpoint factory, JUCE enumeration wrapper, Linux
  inotify worker, inactive macOS draft and their tests were removed. They had
  no current consumer and duplicated device responsibilities before a concrete
  product need was established.
- The user clarified the actual Linux/macOS flow: product-owned logic confirms
  device presence and only then starts `ahioenc` or `ahiodec`; the process uses
  JUCE to open the requested device. There is no core-owned UI, discovery,
  bus classification, hot-plug monitor, polling or fallback.
- The neutral `AudioEndpoint`/`EndpointPolicy` and injected `EndpointMonitor`
  contracts remain unchanged pending a separate roadmap decision. Host facts,
  DSP, Kuaru and AHIO implementation behavior are unchanged.
- P11F is deferred and no concrete platform-source verification is claimed.
  Stable requirement IDs and the historical P11C/P11H evidence are retained.
- Governance now requires a reviewable roadmap before implementation and a
  new explicit `go` after the roadmap. This cleanup followed that gate.
- Standalone strict Debug and Release builds passed CTest 6/6 with no concrete
  endpoint-source test remaining. The Kuaru `sinukaru` consumer target rebuilt
  and linked against coreaudio revision `33898d50372cea05378e779da6ed44a387341b7d`
  plus the working-tree digest. The final source audit found no concrete
  endpoint-source, ALSA/sysfs or inotify implementation reference, and
  `git diff --check` passed in both the coreaudio and Kuaru repositories.

## 2026-09-28 — P12B multiplatform logger foundation

- Added the independent JUCE-free `giganteLog` / `gigante::log` target and the
  public `gigante::Logger` facade. The facade owns canonical AHIO-compatible
  formatting, stable field sorting, single-line sanitization and the explicit
  requested/compiled/effective level report. spdlog types remain private.
- The build ceiling is one of `OFF`, `ERROR`, `WARN`, `INFO`, `DEBUG` or
  `TRACE`. Facade macros remove higher calls before argument evaluation;
  runtime configuration can only lower the effective level. Invalid CMake
  strings fail configuration rather than selecting a default. An `OFF` build
  compiles `DisabledLogger.cpp` and has no spdlog target or link dependency.
- `SessionIdSequence` is explicitly owned and shared by the product and
  injected into its loggers. This makes session IDs monotonic across logger
  instances without a singleton; zero is reserved for events without a
  session.
- Enabled builds use synchronous spdlog 1.17.0 pinned to immutable commit
  `79524ddd08a4ec981b7fea76afd08ee05f83755d`, with bundled fmt 12.1.0.
  The exact revision was exported from the user's existing clone for the
  pinned verification build; the clone itself was not modified. No default
  logger, global registry, asynchronous pool or realtime entry point is used.
- CMake selects separate backend units. Linux uses spdlog's thread-safe
  colored stderr sink, Android uses its raw-message logcat sink, and tests use
  its callback sink. spdlog supplies color, flush and sink synchronization;
  the facade does not reproduce them. macOS can use the terminal unit, but
  native `os_log` and Apple build evidence remain open. iOS is outside the
  current Linux/macOS/Android product focus.
- MIT notices for spdlog and bundled fmt are in `THIRD_PARTY_NOTICES.md`.
  Informative Release archive sizes were: Linux `giganteLog` 96,900 bytes and
  spdlog 1,485,698 bytes; Linux `OFF` `giganteLog` 32,896 bytes with no
  spdlog; Android arm64 `giganteLog` 1,108,722 bytes and spdlog 10,405,840
  bytes. These are static archives with toolchain/debug metadata, not final
  linked-product deltas, and are not gates.
- Strict Linux Debug/TRACE, Release/INFO pinned-source and Release/OFF suites
  pass CTest 8/8. Tests cover the AHIO golden line, ordering, sanitization,
  both level ceilings, compiled-out side effects, callback/platform sink
  lifecycle, shared sessions, flush and concurrent non-interleaving. Android
  arm64 Release cross-builds `giganteLog` with NDK 29 and warnings as errors.
  The Kuaru `sinukaru` target rebuilt and linked without adopting or linking
  `giganteLog`; AHIO was not changed. macOS and product adoption remain
  explicitly unverified, so P12B and REQ-P05/P10/P16 stay partial.

## 2026-09-29 — DEV_REQ workflow

- Before every implementation or documentation phase, present a reviewable
  roadmap containing the phase scope, expected files, affected requirements,
  planned verification and explicit exclusions.
- Do not edit files or begin implementation until the user approves that
  roadmap with a new explicit `go`. Read-only inspection and clarification may
  precede approval when needed to prepare an accurate roadmap.
- Execute only the approved phase. A `go` for one phase does not authorize the
  next phase or expand its stated boundaries.
- At phase completion, update the applicable requirements, architecture and
  memory records with what is specified, implemented and actually verified.
  Record remaining work and failed or interrupted checks without presenting
  them as complete.
- Run the verification declared in the roadmap, including `git diff --check`
  in every changed repository, then stop and report the result before
  proposing or beginning another phase.
- Never create a commit. Provide a detailed commit message for the completed
  diff when requested.
- This approval gate is named the `DEV_REQ workflow` and applies to all future
  coreaudio development phases.

## 2026-09-29 — P12B final verification pass

- Rebuilt the Kuaru `sinukaru` consumer against coreaudio revision `1516f90`
  plus the documentation working-tree digest. The executable linked
  successfully without building or linking `giganteLog`; logger adoption
  remains a separate phase.
- Recreated the Linux Debug/TRACE, Release/INFO and Release/OFF configurations
  with warnings as errors. Each configuration passed CTest 8/8. The enabled
  configurations used the exact spdlog 1.17.0 tag commit; the OFF build tree
  contained no spdlog path or target artifact.
- Rebuilt `giganteLog` for Android arm64, API 26, with NDK 29 and warnings as
  errors. Final archive sizes are the values recorded in the P12B entry above.
- Confirmed that the public logger header contains no JUCE, spdlog or platform
  conditional, and that `git diff --check` passes in both repositories.
- macOS `os_log`, Apple build evidence and product adoption remain unverified;
  P12B and REQ-P05/P10/P16 therefore remain partial.

## 2026-09-29 — G3 implementation layout

- Kept every public header under `include/gigante`, so consumer include paths
  and the single `gigante` namespace remain unchanged. Moved implementation
  files and private headers into `src/dsp`, `src/logger`, `src/metrics`, and
  `src/platform` according to their existing target ownership.
- The move is physical only: `giganteCore` still owns DSP plus metrics,
  `giganteLog` still owns logging, and `gigantePlatform` still owns platform
  facts and endpoint contracts. No new target edge, runtime selection,
  compatibility header, algorithm or product integration was added.
- Linux Debug/TRACE, Release/INFO and Release/OFF strict builds each passed
  CTest 8/8. Android arm64/API 26 rebuilt `giganteCore`, `gigantePlatform`, and
  `giganteLog` with NDK 29 and warnings as errors. The Kuaru `sinukaru`
  consumer rebuilt and linked without any include change.
- Source and CMake audits found no reference to an old flat implementation
  path. Public-header isolation remains covered by the existing build targets;
  `git diff --check` passed in both repositories.

## 2026-09-29 — P12E-K1 Kuaru C++ logger consumer

- Renamed the public sink selector from `LogSinkKind`/`sinkKind` to the more
  precise `LogSinkType`/`sinkType`. No compatibility alias was retained because
  Kuaru is the first product consumer and the API has not been released.
- Kuaru now links `gigante::log` on Desktop and Android and owns the injected
  `SessionIdSequence`, logger and product session through `KuaruLogContext`.
  Coreaudio does not own product state and `giganteCore` still does not depend
  on logging.
- The product supplies the provisional ceiling required by REQ-P15:
  store→WARN, demo→INFO and dev/profiling→TRACE. The Android store compile
  database records `GIGANTE_LOG_COMPILED_LEVEL=2`; its final shared library
  retains the sampled warning event and omits sampled INFO events.
- The Kuaru adapter test verifies session allocation, canonical line output,
  native level propagation and flush. The full Desktop dev executable and
  Android arm64 store shared library build successfully. Coreaudio's strict
  TRACE, INFO and OFF configurations each still pass CTest 8/8 after the public
  rename.
- Kuaru's audio callback and debug sidecar were not changed, and no callback
  log call was introduced. Java `Log.*`, JUCE `DBG`, a real-device store
  logcat capture, AHIO adoption, macOS native logging and shared `BuildPolicy`
  remain explicitly open.

## 2026-09-29 — P12E-K2 dynamic fields and Kuaru Java binding

- Added `std::span<const LogField>` overloads to `Logger::log` and
  `Logger::formatLine`; the initializer-list surface delegates to the same
  implementation. This keeps dynamic JNI fields inside the canonical core
  formatter without adding a product or Java type to coreaudio.
- Added deterministic dynamic-field coverage. Strict standalone Debug and
  Release builds pass CTest 8/8, including canonical ordering/sanitization and
  the existing compiled-out side-effect checks.
- Kuaru owns the Java/JNI adapter and reuses its lifecycle-owned
  `KuaruLogContext`. Its Java golden test, C++ adapter test, full Desktop build,
  Android store Java/native compilation and R8 shrink pass. The shrunk DEX
  removes the three routine `INFO`/`DEBUG` event names and retains the four
  warning names; the native library exports the expected JNI symbol.
- Kuaru's complete APK verification is not claimed: its pre-existing generated
  `kuaru.kuaruconf` ordering alternates between a duplicate asset on an
  incremental run and a missing packaged asset after Gradle clean. Device
  logcat, P12E-K3 `DBG`, AHIO adoption and macOS native logging remain open.

## 2026-09-29 — P12E-K2B Kuaru packaging verification closeout

- Kuaru separated its generated encrypted config from the asset root owned by
  its copy task and added an incremental Gradle task that invokes the existing
  cryptor before CMake configure and asset merge.
- Clean and incremental Android store APK builds now pass `verifyStoreApk`.
  The incremental generator is up to date and preserves the encrypted asset's
  hash. The packaged DEX continues to omit routine Java log events and retain
  warnings plus the explicit session-zero fallback.
- This closeout changes no coreaudio implementation. Runtime device logcat,
  Kuaru P12E-K3, AHIO adoption and macOS native logging remain open.
- A Kuaru repository audit also found that its earlier P12E-K1 commit omitted
  the required `KuaruLogContext` source/header/test files even though its build
  graph references them. Local verification used the untracked files; Kuaru
  must add them before clean-checkout reproducibility can be claimed. This is
  a consumer-repository integrity issue, not a coreaudio source change.

## 2026-09-29 — P12E-K3 Kuaru JUCE diagnostic closeout

- Kuaru migrated ten non-realtime JUCE `DBG` sites that already had an explicit
  `KuaruLogContext` and classified 70 bootstrap/implementation traces as
  intentionally retained dev-only diagnostics. Three textual references are
  commented templates, not calls. An executable source inventory rejects new
  or moved unclassified sites.
- Desktop dev plus both Kuaru tests pass. Android dev proves all ten structured
  events compile into the native library; Android Store passes APK verification
  and contains none of their names under its `WARN` build ceiling. Callback and
  debug-save paths remain unchanged.
- No coreaudio implementation or AHIO file changed. Kuaru runtime Store logcat,
  AHIO adoption and macOS native logging remain open.

## 2026-09-30 — P12D deferred and P12E status reconciled

- The user chose to leave shared `gigante::BuildPolicy` extraction for later.
  P12D and REQ-P12 to REQ-P14 remain specified but unimplemented;
  `SinuBuildPolicy` stays Kuaru-owned and the verified product-supplied log
  ceiling from REQ-P15 remains authoritative. Resuming P12D requires a new
  roadmap and `go`.
- P12E status now reflects the completed K1/K2/K3 source work, tests and
  Desktop/Android artifact audits. Runtime Store device logcat is the only
  remaining Kuaru logging evidence; AHIO adoption and Apple work remain outside
  the authorized boundary.
- Kuaru's committed gitlink records coreaudio `8ee15a7`, while the working tree
  checks out `236630a`. No gitlink was changed implicitly; exact clean-checkout
  verification remains open until a deliberate parent-repository update.

## 2026-10-16 — P13A/P13B render capability (Group Q), an anticipated-reuse exception

- This phase did not originate from AHIO scope or a current Kuaru/AHIO need.
  It originated from a separate product, `gst-harness` in the `gstools`
  repository, which needs to know whether the host can actually render video
  before deciding how to initialize its own Qt application object (a real
  windowing platform plugin needs a working display; a headless CLI/daemon
  mode does not and must not require one). The user explicitly decided this
  belongs in coreaudio as a generic, reusable platform fact, overriding the
  default "demonstrably required by this library today" bar as a deliberate
  exception, not because any current consumer needs it.
- The user also directed that the contract be generic and extensible from the
  start, not a one-off video flag: a capability domain is an enum, and a
  later domain is added the same way (extend/add an enum, add one probe per
  `*HostFactsSupport` platform family), matching the pattern this layer
  already uses for `HostFacts` itself rather than inventing a new mechanism.
- Implemented: `RenderCapability{audio, video}` in `include/gigante/HostFacts.h`;
  `HostFacts::renderCapabilities` (`std::vector`, empty when unprobed/
  undetectable, never guessed present); `detectPosixRenderCapabilities()`
  (audio always present; video from a reachable Wayland socket at
  `$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY` or an X11 socket at
  `/tmp/.X11-unix/X<N>` parsed from `$DISPLAY` — file presence, not a full
  protocol handshake, so no Wayland/X11 client library was added);
  `detectAppleRenderCapabilities()` (macOS: `CGSessionCopyCurrentDictionary`'s
  `kCGSessionOnConsoleKey`, linking `ApplicationServices` only for the macOS
  CMake branch, not iOS; iOS: both unconditionally, since a handheld device
  always has a built-in screen and speaker). `AndroidHostFacts` was not
  touched — explicit scope decision, not an oversight; it reports an empty
  vector until a later phase adds a probe.
- Verification on macOS (AppleClang 21, CMake standalone configure,
  2026-10-16): `gigantePlatformTests` built clean with no new warnings and
  passed, including the new cases (audio always present, no duplicate
  entries, `renderCapabilityName` stability for both values, a
  default-constructed `HostFacts` has an empty `renderCapabilities`). Beyond
  the test binary, a separate standalone probe program (linked directly
  against `libgigantePlatform.a` plus `ApplicationServices`) was built and run
  on this real interactive console session and printed `platform=macOS
  capabilities=audio video` — confirming the macOS probe against real ground
  truth, not only that the test suite's loose assertion passed.
- Not verified: the POSIX probe was only exercised in the "no display set"
  path on this macOS machine (no Linux host with a real Wayland/X11 session
  available locally); REQ-Q02 is recorded as Verified: Partial for exactly
  this reason. Android and iOS have no device verification. No Kuaru or AHIO
  call site was changed; this phase added the fact only, per REQ-Q04.
- Full-suite confirmation: a standalone configure without `-DCMAKE_CXX_STANDARD=20`
  left `giganteLog`'s `std::span` usage unbuildable (a pre-existing C++17/20
  gap, unrelated to this phase — `gigantePlatformTests` alone built and passed
  under both configures). Reconfiguring with `-DCMAKE_CXX_STANDARD=20
  -DCMAKE_CXX_STANDARD_REQUIRED=ON` built and passed the entire suite, 8/8:
  `giganteCoreTests`, `giganteGoldenVectorTests`, `giganteGoldenHashVerification`,
  `gigantePlatformTests`, `giganteEndpointPolicyTests`, `giganteEndpointMonitorTests`,
  `giganteLogTests`, `giganteLogCompiledOutTests`.


## 2026-10-30 — iOS session preparation phase A

User approved the presented coreaudio-only phase with standalone `go`.
Implemented the neutral SessionPreparation interface, ManagedSession event/lifetime
gate, private fake-injectable backend boundary and Objective-C++ iOS backend.
Playback/default category/mode, activation/deactivation, interruption/resume hint,
route-change reason and media-services events are represented without claiming
JUCE device readiness or duplicating its recovery loop. Reports retain primary,
cleanup and callback errors. A process lease rejects competing prepared shared
instances. Native deactivation failure requires explicit cleanup retry.

Verification on macOS, AppleClang 21 / Xcode SDK 27.1:
- Standalone Debug and Release builds with strict warnings/-Werror and explicit
  C++20, pinned spdlog source reused locally: 9/9 CTest tests pass in each.
  C++20 is the existing standalone logger workaround; this phase does not
  change the public C++17 build declarations.
- Public header isolation object targets compile. Session tests cover validation,
  requested/applied/observed state, repeated prepare/release, partial/throwing
  preparation cleanup, native errors, cleanup failure and explicit retry,
  original callback retention, resume hints and route reason, loss/reset,
  callback exceptions, stale generations, callback-side release/destruction,
  external in-flight teardown and unsupported desktop behavior.
- ThreadSanitizer Debug build: giganteSessionPreparationTests passes (1/1).
- iOS arm64 / iphoneos, minimum iOS 15, Debug static gigantePlatform cross-build
  passes with Objective-C++ and AVFoundation/Foundation. Enabling tests and
  building giganteSessionPreparationTests additionally links an iOS test app;
  the iOS platform public-header isolation target compiles. Neither is run on a
  device; no Kuaru app build, installation, signing setup or device use occurred.
- Earlier logging-OFF Debug run also passed 9/9. Runtime iOS behavior remains
  unverified; desktop fake tests and a cross-build do not establish that evidence.

Reproduction: configure this repository with CMAKE_BUILD_TYPE=Debug or Release,
CMAKE_CXX_STANDARD=20, and FETCHCONTENT_SOURCE_DIR_SPDLOG pointing at the existing
pinned source when offline; build then run ctest --output-on-failure. This run
used /tmp/coreaudio-session-debug and /tmp/coreaudio-session-release. TSan used
/tmp/coreaudio-session-tsan, GIGANTE_LOG_COMPILED_LEVEL=OFF and
-fsanitize=thread in CMAKE_CXX_FLAGS and CMAKE_EXE_LINKER_FLAGS. Cross-build used
/tmp/coreaudio-session-ios, CMAKE_SYSTEM_NAME=iOS, CMAKE_OSX_SYSROOT=iphoneos,
CMAKE_OSX_ARCHITECTURES=arm64, CMAKE_OSX_DEPLOYMENT_TARGET=15.0,
GIGANTE_BUILD_TESTS=OFF and GIGANTE_LOG_COMPILED_LEVEL=OFF for the initial library
build, then GIGANTE_BUILD_TESTS=ON for the test-link/header-isolation smoke check.
Tracked diffs in coreaudio and the parent pass git diff --check; new source and
diagram files also pass explicit whitespace checks, and all three SVGs parse.

REQ-O17/O20 stay partial. Kuaru adoption and device acceptance are next, separately
approved. Android source behavior is untouched; its shared-interface adoption
requires a subsequent reuse/parity roadmap after iOS migration. No AHIO adoption,
endpoint inventory, DSP changes, provisioning or app rebuild was performed.


## 2026-11-02 — Android session preparation C1

Approved by standalone go after the presented C1 roadmap. Added neutral Android
focus backend, injected private driver and Android-only JNI context factory.
Shared Java listener uses token-based RegisterNatives callbacks; no Kuaru package
or JNI_OnLoad dependency exists in core. Reports preserve requested attributes,
native request/abandon results, observed raw change, Java cause and cleanup error.
ManagedSession retains observations across release and rejects stale generations.
Android categoryApplied remains false; successful focus grant is distinct from
both device readiness and product startup policy.

Core Debug/Release CTest 10/10 pass with explicit C++20 (existing logger workaround)
and pinned local spdlog source. ThreadSanitizer session suites pass 2/2. Consumer
focused tests pass 9/9; macOS and Android Oboe/OpenSLES builds pass. Hardened store
R8 task and DEX checks preserve the shared listener/native signature. Signed iOS
build/verifier pass; physical startup unavailable because no iPhone is connected.

Pixel 9 Pro baseline PID 5447 and migrated PID 6701 report the same media/music
attributes, GAIN, request=1 and flags=0. Only listener ownership changes to shared
AudioFocusListener. User confirms matching normal notes, background held-note
release/fresh-note return and assistant/other-audio focus takeover behavior.
The Pixel initially disconnected before final native callback capture and cleanup-build
startup. After reconnection, the exact final APK installed in place and started
as PID 16759 with shared grant result=1 and matching AudioManager facts. Fresh
focus callback capture remains pending. Host tests do not substitute for native failure or
all focus-loss variants. Full consumer evidence, baseline hashes, install-data
caveat and reproduction are in Kuaru docs/devreq/android-session-preparation/acceptance.md.

C2 service/wake locks, C3 scheduling, endpoint work and AHIO adoption are deferred
and separately gated. REQ-O20 remains partial. No commit is made automatically;
Kuaru must pin the complete C1 core revision after the user commits it.

## 2026-11-02 — Android session resources C2A

User explicitly selected the pending C2A roadmap after the completed IOSDog
repair. Added independent SessionPreparation resource request/release/observe
operations and reports for backgroundAudio/cpuAwake. ManagedSession serializes
operations, preserves reports through focus prepare/release, and tears down only
owned resources in wake/service/focus order. Request and cleanup causes stay
separate. Unsupported providers fail explicitly, not by successful no-op.

AndroidSessionPreparation optionally composes AndroidSessionResourceDriver.
The new five-argument public factory injects context and all helper classes;
the existing three-argument factory remains focus-only. JniSessionResourceDriver
uses process leases plus the Java class monitor across observe-and-act, refuses
external ownership and retains pending-stop or failed-cleanup ownership until
explicit observation/retry. Native observation failure does not prevent owned
cleanup. JNI attachment/local frames/Throwable handling were extracted unchanged
from C1 into a private support header. Global refs belong to the backend.

Shared Java helpers add atomic state/error snapshots without switching Kuaru
owners or changing their existing method signatures/boolean/exception behavior.
Pending stop stays visible until onDestroy; wake state comes from isHeld.

Verified: standalone macOS strict-warning build, public headers, and CTest 11/11;
real resource JNI driver with JVM fixtures under -Xcheck:jni; actual Java helper
lifecycle/error/retry fixture test; Java API 36 compilation; Android arm64/API 29
platform-library cross-build (NDK 29.0.13599879); iOS arm64/iPhoneOS 16 platform
library build. No product APK or physical service qualification was performed.
Commands, file tree and evidence boundaries are in
[acceptance](docs/devreq/android-session-resources/acceptance.md). The JNI harness
adds lease/failure/worker-thread proof independent of the C++ fake driver. The
Java helper test lives in Kuaru tests/AndroidResourceSnapshotTests.py and
exercises actual helper code against JVM lifecycle fixtures,
not an Android OS. SVG/HTML checks and both repository diff checks pass.

REQ-O20 remains partial. C2B Kuaru ownership handoff, physical acceptance,
manifest/R8 adoption, multicast/scheduling, screen and Bluetooth policy and AHIO
adoption remain deferred and separately gated. No stage/commit or capture/build
cleanup was performed. Additional private JNI support and test harness files are
reconciled against the approved scope in the acceptance file tree.


## 2026-11-02 — C2B consumer adoption and repeated pending-stop repair

Kuaru's approved C2B adopts the C2A backend through one consumer owner. Initial
physical teardown exposed a second stopService call from full session cleanup
while onDestroy was queued; the already removed native service record returned
false while Java still reported running. No resource remained after exit, but
its error report was misleading. The real-JNI regression failed before repair.

JniSessionResourceDriver now retains accepted-stop state separately from report
fields, skips repeated native stop while owned/stopPending, and clears it on
inactive observation/new acquisition. Native denied/throwing stops still allow
explicit retry. Existing JVM fixtures verify repetitions across observation and
a denied-then-accepted stop retry; the harness passes under -Xcheck:jni. No public
API or product policy was added. This necessary fix touches the private driver,
JNI test and existing Java fixture beyond C2B's original docs-only core plan.

Kuaru registered host tests 2/2, existing core CTest 11/11, actual-helper JVM test,
final Android Oboe/OpenSLES builds, R8 retention and signed iOS build pass. User
reports baseline/migrated normal notes, Home/return and focus-takeover parity;
final patched notes/power-button exit pass. Final native shutdown shows one
accepted service stop, pending ownership, later inactive/unowned, no resource
error and no remaining native service/audio partial wake lock. A fresh startup
recreates exactly one service/lock. Native denial/pending-start races remain
host-only; UI LookAndFeel teardown assertion remains a separate deferred issue.

Core baseline is c299ef3 with this local patch; no commit/gitlink update is
claimed. See the consumer docs/devreq/android-session-resources/acceptance.md
for artifacts, detailed qualification and full file reconciliation. Earlier
Java-test relocation documentation edits are retained.


## C3A Android scheduling comparison — 2026-11-02

The consumer now has an explicit legacy|juce scheduling comparison selector; legacy
remains the default. Four optimized profiling builds passed on JUCE 9.0.4.
Matched Pixel 9 Pro speaker trials (48 kHz/480 frames, Cigar reeds/default)
showed zero native underrun-counter deltas in all twelve 30-second windows.
Callback-processing ranges overlap; these trials demonstrate no custom-hook
benefit. Both JUCE-only backends passed user listening/idle/Home-return checks.
Both modes/backends passed reopen and ordinary power-exit/fresh-launch checks;
the existing LookAndFeel teardown assertion remains deferred in all four.
Framework render threads already used FIFO/RT2. Legacy OpenSLES also changed
main-thread nice during synchronous priming, so first processing call must not
be assumed to identify the render thread. No default flip, scheduler removal,
new shared API or AHIO change is authorized by this comparison.

Normal legacy/Oboe dev APK was restored, with the prior 1920-frame setting.
iOS dev build passed; runtime not repeated. The stale Bluetooth output entry
reported with Bluetooth off cleared after a cold start but has no source fix;
its exact inventory layer remains unresolved. Bluetooth output blocking and
broader device qualification are separate deferred work. See the consumer
docs/devreq/android-thread-scheduling/{comparison,acceptance}.md for protocol,
artifact identities, metric semantics, signing workaround and check limits.
