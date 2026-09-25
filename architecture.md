# Core Audio Architecture

## Current state

P8H adds neutral bounded realtime metrics to the P8G fixed-order production
chain. Kuaru retains its product-owned parameter and `ProfileCollector`
adapters; builds without profiling pass a null observer to the core. AHIO has
not adopted the core and remains outside this phase.

## Context and dependency direction

```text
Kuaru synthesis + adapter  --->  gigante core  <---  AHIO decoder + adapter
Kuaru output policy        <---  processed PCM --->  AHIO endpoint policy

core -/-> Kuaru product types
core -/-> AHIO stream/device/transport types
Kuaru -/-> AHIO stream/device/transport types
```

The core processes explicit PCM supplied by a consumer. It owns musical DSP
state, smoothing, deterministic recovery, algorithmic-latency reporting, and
neutral realtime observation. It does not own acquisition, playback, voices,
MIDI, configuration files, routes, codecs, transports, UI, or persistence.

## Target structure

```text
giganteCore              public C++ library
giganteCoreTests         fast hardware-independent tests
giganteGoldenVectorTests committed-vector format/finite-data tests
consumer adapters        remain in Kuaru and AHIO repositories
```

Targets are logical ownership boundaries, not permission to introduce a FIFO,
copy, block adapter, thread, or dependency. Optional targets must not change
the public library's product neutrality.

## Processing boundary

The implemented processor contract has four explicit concepts:

- requested preparation data: sample rate, channel layout, maximum block size,
  and any explicit limits;
- effective preparation data: the validated configuration actually granted by
  the processor;
- immutable per-call parameter/context snapshots supplied by the product
  adapter;
- non-owning PCM views whose lifetime is limited to the synchronous call.

`prepare`, `reset`, processing, reconfiguration, and teardown are deterministic
and repeatable. Invalid transitions return a specific terminal result; they do
not choose an alternative configuration. No buffer view may escape the call.

## Fixed chain migration boundary

The first production migration preserves this order and behavior:

```text
tremolo -> growl control -> filter -> growl post-filter -> drive
        -> bell peak -> phaser -> chorus -> reverb -> noise
        -> soft limiter -> output gain
```

Extraction is a relocation and boundary change, not an optimization phase.
Kuaru characterization precedes movement. Kuaru migrates first and must pass
reviewed equivalence gates before AHIO consumes the core.

## Realtime observation

The audio path writes only bounded, preallocated counters and samples of timing
data. A product-owned non-realtime adapter drains/publishes them and performs
clock conversion, formatting, logging, serialization, and persistence.

`RealtimeMetricsObserver` accepts an injected raw-tick clock and a budget in
ticks per sample precomputed outside the callback. It keeps cumulative state on
the single audio producer, then publishes complete snapshots through a fixed
eight-slot SPSC queue using lock-free 32-bit indices. A full queue drops the
publication, increments an observable counter, and never blocks or allocates.
One non-realtime reader drains to the newest stable snapshot. Signal views are
read-only and valid only for the synchronous observer call.

Observation distinguishes input, stages, post-chain, and output; reports stage
calls/cost, budget exceedance, algorithmic latency, resets, safety recovery,
and non-finite counts. A bypassed zero-latency stage reports that fact rather
than disappearing. Products separately retain requested/effective device and
route evidence.

The core exports a static CMake target (`giganteCore`, alias `gigante::core`).
Repository independence does not imply runtime dynamic loading: Kuaru links
the target into its product binary and calls `EffectsChain::process()` once per
audio block after synthesis and polyphony gain.

## Build and dependency policy

The project uses target-based CMake. Project warnings are strict and scoped to
project targets. Third-party includes/targets are isolated as external/system
dependencies without weakening project warnings globally.

JUCE is not vendored. Public headers, the neutral fallback, and standalone
tests remain JUCE-free. The production implementation is selected only when a
consumer supplies an explicit CMake interface target for its pinned JUCE;
the core never downloads JUCE or assumes a consumer checkout path.

Revision identity is generated from explicit build metadata when available and
has a documented reproducible fallback for exported source archives.

## Test architecture

Fixtures consist of immutable PCM inputs, parameter/event timelines, lifecycle
actions, metadata, and expected outputs/metrics. Metadata includes revision,
sample rate, channels, block-size sequence, seed, origin, and comparison mode.

Golden generation is a deliberate migration tool, separate from ordinary test
execution. Tests only read and compare reviewed goldens. A failed comparison
cannot rewrite its expected artifact.

Comparison modes are explicit:

- bit-exact for paths where identical representation and evaluation order are
  required;
- numeric tolerance with recorded absolute/relative or signal-domain criteria
  where bit identity is not practical.

Fakes or injected dependencies supply clocks and observation sinks. Core tests
never acquire hardware or parse product configuration.

## Decisions

### ADR-001 — Independent neutral core

Status: Accepted; P8G Kuaru integration and controlled render-budget evidence
are verified, while the repository pin awaits an authoritative remote and exact
consumer gitlink; AHIO adoption remains P8I.

The library is independently versioned and consumed in one direction by Kuaru
and AHIO at pinned revisions. Product adapters remain with their products.

### ADR-002 — Preserve before improving

Status: Accepted; P8G Kuaru output is bit-exact for all five reviewed vectors.

The current Kuaru chain is characterized before extraction. Algorithm changes,
new defaults, or performance redesign are excluded from migration phases.

### ADR-003 — No hidden realtime adaptation

Status: Accepted; implemented for bypass and production processing. The only
whole-block copy is the preallocated bell-filter crossfade scratch already
present in Kuaru; it adds no latency, allocation, FIFO, or thread hop.

The API operates synchronously on granted callback buffers. Format conversion,
channel conversion, buffering, copies, or thread changes—if ever required—must
be separate, explicit, measured components rather than processor internals.

### ADR-004 — Group N authority and checkout name

Status: Accepted.

This repository implements AHIO Group N under its stable requirement IDs. The
canonical repository/check-out identifier is `coreaudio`. Earlier planning and
bootstrap labels denote this same project, not a second implementation or fork.

### ADR-005 — Raw float goldens and explicit render seed

Status: Accepted and verified in P8F.

Kuaru's 24-bit WAV writer applies dither, so a container hash is not an engine
bit-equivalence gate. The migration authority is interleaved float32-LE PCM
captured immediately after `processBlock`. Reference rendering opts into a
per-voice deterministic detune sequence with seed `1592594996`; normal product
playback retains its prior system-random path. Two full fresh-process captures
must agree before a vector is promoted manually. Each block geometry remains
an independent golden because callback-granular changes can legitimately make
different block sizes produce different samples.

### ADR-006 — Consumer-supplied JUCE implementation

Status: Accepted and verified in P8G.

`giganteCore` has one public, JUCE-free contract and two build-time
implementations. Standalone builds use the neutral implementation. A product
that needs the production chain supplies an existing JUCE interface target via
`GIGANTE_JUCE_TARGET`; Kuaru supplies its pinned JUCE 8.0.8 headers and links
the same JUCE module objects it already owned. This is dependency injection at
the target boundary, not product selection in public headers.

### ADR-007 — Raw-tick SPSC metrics publication

Status: Accepted and locally verified in P8H.

Timing uses an injected monotonic raw-tick source. Callback code does not
convert ticks to wall time. Metrics storage is fixed-size and single-producer;
publication uses a bounded SPSC queue with an observable drop count. Formatting,
serialization, logging, and persistence remain consumer work outside the core.
