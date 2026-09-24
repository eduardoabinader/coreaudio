# Gigante Labs Core Memory

This file records durable decisions and reproducible verification results.
Requirements belong in `requirements.md`; intended design belongs in
`architecture.md`. Every verification entry must state date, environment,
procedure, and result. Assumptions are not facts.

## 2026-09-24 — G0 governance baseline created

### Specified

- The repository is an independent C++ audio core at the local checkout
  `modules/gigantelabscore`.
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

- The checkout/repository identifier is `gigantelabscore`. The older AHIO
  planning label `gigante-labs-audio-core` refers to the same intended core and
  is not a second library or fork.
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

## 2026-09-24 — P8G chain extraction implemented; closure gates remain

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
- A fresh P8G capture and its fresh-process reset repeat completed. `cmp` matched
  all five post-migration float32 vectors byte-for-byte with the reviewed P8F
  goldens at block sizes 7, 64, 257, and 512.
- P8G is not marked complete yet: the no-commit workflow prevents an exact
  Kuaru gitlink until the core commit exists, and controlled stage-budget
  evidence is still required. No P8H work has started.
