# Gigante Labs Core Requirements

## Authority and status model

This repository is the authoritative implementation of AHIO Group N. The
stable IDs `REQ-N01` through `REQ-N45` are retained so Kuaru, AHIO, phases, and
tests refer to one contract. The wording below is self-contained and adapted
only where the user explicitly selected the repository name/path
`gigantelabscore`; it does not import AHIO transport or Kuaru product policy.

Each requirement tracks three independent states:

- **Specified**: the normative contract is approved.
- **Implemented**: repository or consumer code implements the contract.
- **Verified**: named acceptance evidence demonstrates the behavior.

A build can support **Implemented**, but cannot by itself establish
**Verified**. Requirement IDs are never renumbered or reused.

## N.1 — Ownership, repository, and naming

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N01 | Shared realtime audio lives in the independent `gigantelabscore` repository and contains cohesive audio processing, realtime observation, and deterministic test support—not general product utilities. | Yes | Yes | Yes | P8G core chain plus standalone/core and Kuaru golden verification |
| REQ-N02 | Kuaru and AHIO consume an exact pinned revision of the same authoritative core; copies, translations, and product forks are prohibited. | Yes | No | No | P8G Kuaru pin; P8I AHIO pin |
| REQ-N03 | Public C++ uses the single namespace `gigante`; common types name their responsibility without repeating the owner name. | Yes | Yes | Yes | P8F public-header isolation build and review |
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
| REQ-N21 | Core processing/observers perform no allocation, lock, blocking call, device/network/file I/O, log formatting, or mutable configuration parsing on an audio callback. Publication uses bounded realtime-safe hand-off. | Yes | Partial | Partial | P8F bypass/observer path source audit; P8G/P8H instrumentation remains |
| REQ-N22 | The core adds no FIFO, whole-block accumulation, callback-size conversion, thread hop, or full-buffer copy merely for abstraction. Any unavoidable conversion has measured ownership, cost, and latency. | Yes | Yes | Yes | P8F non-owning synchronous PCM implementation and source audit |
| REQ-N23 | The core reports algorithmic latency in samples. Bypass/no-look-ahead stages report zero; delay/tail and transport/device buffering remain distinct evidence. | Yes | Yes | Yes | P8G production and bypass report zero samples; no hidden adaptation |
| REQ-N24 | Smoothing, delay lines, envelopes, filter/limiter memory, and deterministic noise state belong to the core. `prepare`/`reset` initialize them deterministically; live changes preserve current Kuaru smoothing until separately approved. | Yes | Yes | Yes | P8G state relocation and live-change bit-exact vectors at blocks 7/512 |
| REQ-N25 | Non-finite samples, invalid coefficients, recovery resets, and safety activation follow explicit bounded policies. The callback records counters/state only; explanatory/persistent logs occur outside it. | Yes | Yes | Partial | P8G counters/recovery implemented; invalid golden and core tests pass; allocation instrumentation remains |
| REQ-N26 | The core exposes neutral per-stage observation through an `EffectsStageObserver`-equivalent contract and depends on neither `ProfileCollector` nor AHIO metrics. | Yes | Yes | Yes | P8F observer callbacks/tests and dependency audit |
| REQ-N27 | Kuaru adapts observation to `ProfileCollector`; AHIO adapts it to AHIO metrics/reports. A no-op observer changes neither processing behavior nor product conditionals in the core. | Yes | No | No | P8H Kuaru parity; P8I AHIO adapter |
| REQ-N28 | Observation is preallocated/bounded and uses no owning `std::function`, allocating dispatch, formatting, or wall-clock conversion on the callback. Disabled observation has measured negligible overhead. | Yes | No | No | P8H realtime/overhead evidence |
| REQ-N29 | Stable snapshots expose input/post-chain/output peak and RMS, per-stage calls/time, maximum callback contribution, budget exceedance, algorithmic latency, resets, and non-finite counts. Products retain requested/effective configuration evidence. | Yes | No | No | P8H deterministic snapshot tests |
| REQ-N30 | Debug WAV, JSON sidecars, console, and report-file writing stay outside the realtime core. A future shared tap is a separate bounded target, never hidden chain I/O. | Yes | No | No | P8H dependency/I/O audit |

## N.4 — Migration and acceptance

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| REQ-N31 | Before moving an effect, capture deterministic Kuaru reference vectors for representative presets, transients, sustain, tails, live changes, reset, and invalid recovery; record all random seeds. | Yes | Yes | Yes | P8F reviewed float32 goldens, manifest, two complete identical captures |
| REQ-N32 | Migrate Kuaru alone first. AHIO waits for bit equivalence where practical and reviewed tolerance elsewhere, with no unexplained stage-budget regression. | Yes | Yes | Partial | P8G five-vector bit parity; offline wall-time evidence recorded, controlled stage-budget run remains before closure |
| REQ-N33 | Tests cover supported rates, mono/stereo, fixed/varying/small blocks, bypass, smoothing, tails, reset, malformed parameters, non-finite recovery, deterministic metrics, and repeated prepare/reconfigure. | Yes | Partial | Partial | P8F lifecycle/bypass/recovery suite plus Kuaru goldens; extracted smoothing/metrics remain P8G/P8H |
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

## Repository governance requirements

These IDs cover repository practice not owned by the product-facing Group N
contract.

| ID | Requirement | Specified | Implemented | Verified | Phase / evidence |
|---|---|---:|---:|---:|---|
| GLC-GOV-01 | Maintain `CLAUDE.md`, `Memory.md`, `requirements.md`, and `architecture.md`; update affected documents at each phase boundary. | Yes | Yes | Yes | G0 file/consistency checks passed 2026-09-24 |
| GLC-GOV-02 | Work one approved phase at a time and await `go` before the next phase. | Yes | Yes | Yes | G0 stops before P8F and awaits `go` |
| GLC-GOV-03 | Never commit; provide a detailed proposed commit message. | Yes | Yes | Yes | G0 handoff; repository has no commits |
| GLC-GOV-04 | Run `git diff --check` in every changed repository. | Yes | Yes | Yes | G0 checks passed 2026-09-24 |
| GLC-GOV-05 | Use modern target-based CMake; separate library, tests, and adapters; apply strict project warnings while isolating external warnings. | Yes | Yes | Yes | P8F target build with GCC 16.2.1 and `-Werror` |
| GLC-GOV-06 | Do not vendor a second JUCE. Reuse a consumer-provided exact pin through explicit CMake integration if the extracted DSP still requires JUCE. | Yes | Yes | Yes | P8F core is JUCE-free; repository dependency audit |
| GLC-GOV-07 | Fixtures record revision, parameters, rate, channels, block size, seed, origin, tolerance, and comparison mode; goldens are never automatically updated and every difference requires a documented decision. | Yes | Yes | Yes | P8F fixture plan, reviewed manifest, immutable capture workflow |

## Phase traceability

| Phase | Scope | Requirement IDs | Exit evidence | Status |
|---|---|---|---|---|
| G0 | Establish the core repository as Group N authority and import its governance/architecture context | N01–N45; GOV-01–04 | Required documents, source audit, consistency checks, `git diff --check` | Complete 2026-09-24 |
| P8F | Build skeleton plus deterministic Kuaru characterization; no production DSP moves | N01–N08, N21–N23, N26, N31, N33; GOV-01–07 | Configure/build/CTest, header isolation, dependency decision, fixture schema, reviewed pre-migration vectors | Complete 2026-09-24; awaiting `go` for P8G |
| P8G | Extract fixed-order chain and migrate Kuaru alone | N09–N11, N15, N17–N25, N32–N33; GOV-01–07 | Bit-exact/tolerance parity, lifecycle/realtime suite, stage-budget comparison | Implemented; five-vector bit parity verified; exact gitlink and controlled stage-budget evidence pending before complete |
| P8H | Neutral observation plus Kuaru `ProfileCollector` adapter | N26–N30, N32–N33; GOV-01–07 | Metrics parity and disabled-observer overhead | Not started |
| P8K | AHIO-owned stream roles and local identity plan; no core implementation ownership | L32–L54 in AHIO | AHIO evidence; prerequisite for P8I | Not started in AHIO |
| P8I | AHIO consumes pinned verified core in USB neutral bypass | N02, N06–N08, N13–N16, N19–N23, N27, N29, N34 | AHIO conservation/buffering/xrun/CPU/physical-latency comparison | Not started in AHIO |
| P8J | AHIO enables independently calibrated effects incrementally | N10–N14, N17–N20, N24–N25, N35 | AHIO per-effect quality/stability/latency evidence | Not started in AHIO |

The vocoder and every DSP optimization/redesign require a later explicit phase
and `go`; they are not implied by completing P8F–P8J.
