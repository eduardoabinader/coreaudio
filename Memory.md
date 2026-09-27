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
  kernel release/version; macOS and iOS select distinct `PlatformKind` values.
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
