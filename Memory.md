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
- A desktop `demo` verification attempt exposed an existing product build-policy
  failure: Kuaru applies C++-only `-fvisibility-inlines-hidden` to mbedTLS C
  sources while that dependency uses `-Werror`. No out-of-scope build-policy
  change was made; the disabled `nullptr` branch is source-audited and the core
  path is measured independently.
- P8H remains uncommitted by policy. The Kuaru gitlink cannot identify these
  working-tree changes until the user commits the core and updates the gitlink;
  the configured remote still requires publication of the authoritative
  history.
