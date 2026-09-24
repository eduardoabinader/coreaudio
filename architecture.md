# Gigante Labs Core Architecture

## Current state

This document specifies the intended boundary. At phase G0 there is no library
target or DSP implementation, so no component below is claimed as implemented
or verified.

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

## Planned target structure

```text
giganteCore              public C++ library
giganteCoreTestSupport   deterministic fixtures/helpers, test-only
giganteCoreTests         fast hardware-independent tests
consumer adapters        remain in Kuaru and AHIO repositories
```

Targets are logical ownership boundaries, not permission to introduce a FIFO,
copy, block adapter, thread, or dependency. Optional targets must not change
the public library's product neutrality.

## Processing boundary

The planned processor contract has four explicit concepts:

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

Observation distinguishes input, stages, post-chain, and output; reports stage
calls/cost, budget exceedance, algorithmic latency, resets, safety recovery,
and non-finite counts. A bypassed zero-latency stage reports that fact rather
than disappearing. Products separately retain requested/effective device and
route evidence.

## Build and dependency policy

The project uses target-based CMake. Project warnings are strict and scoped to
project targets. Third-party includes/targets are isolated as external/system
dependencies without weakening project warnings globally.

JUCE is not vendored. Phase P1 must decide, from the actual extracted surface,
whether the core can be JUCE-free. If JUCE remains necessary, the consumer
provides the exact pinned target through an explicit CMake integration; the
core never downloads or assumes a consumer checkout path.

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

Status: Accepted, specified; implementation not started.

The library is independently versioned and consumed in one direction by Kuaru
and AHIO at pinned revisions. Product adapters remain with their products.

### ADR-002 — Preserve before improving

Status: Accepted, specified; verification awaits P2/P3.

The current Kuaru chain is characterized before extraction. Algorithm changes,
new defaults, or performance redesign are excluded from migration phases.

### ADR-003 — No hidden realtime adaptation

Status: Accepted, specified; implementation not started.

The API operates synchronously on granted callback buffers. Format conversion,
channel conversion, buffering, copies, or thread changes—if ever required—must
be separate, explicit, measured components rather than processor internals.

### ADR-004 — Group N authority and checkout name

Status: Accepted.

This repository implements AHIO Group N under its stable requirement IDs. The
repository/check-out identifier is `gigantelabscore`, as explicitly directed.
AHIO's existing `gigante-labs-audio-core` label denotes this same intended core,
not a second project or fork. Consumer wording is reconciled only in a later
approved consumer phase; G0 does not authorize editing AHIO or Kuaru.
