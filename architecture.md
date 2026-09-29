# Core Audio Architecture

## Current state

P8H provides neutral bounded realtime metrics for the fixed-order production
chain. P11B adds the independent `gigantePlatform` target and host-facts
contract without migrating either consumer. Kuaru retains its product-owned
parameter, `ProfileCollector`, and platform adapters. AHIO has not adopted the
core or platform target and remains outside this phase.

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
gigantePlatform          platform facts; never linked by giganteCore
gigantePlatformTests     host-facts and lifecycle contract tests
consumer adapters        remain in Kuaru and AHIO repositories
```

Targets are logical ownership boundaries, not permission to introduce a FIFO,
copy, block adapter, thread, or dependency. Optional targets must not change
the public library's product neutrality.

## Platform facts boundary

`gigantePlatform` is a sibling of `giganteCore`; neither target links the
other. CMake selects exactly one hidden `HostFactsSource` implementation for
Android, iOS, macOS, or Linux. Any other `CMAKE_SYSTEM_NAME` fails during
configuration with the unsupported target in the diagnostic. Public headers
and callers contain no platform conditional.

`createHostFactsSource()` returns a separately owned instance rather than a
singleton. A read returns platform identity, logical cores, physical memory,
architecture, operating-system build fingerprint, the explicit source
revision, and a caller-supplied realtime-scheduling outcome. Requested and
observed scheduling policy/priority remain distinct. The facts source reports
that outcome; it does not request scheduling or select policy.

Operating-system-family mechanics are composed rather than copied. Linux and
Android share `PosixHostFactsSupport` for `sysconf`, `uname`, revision, and
scheduling fields; Android overrides only the build fingerprint with
`ro.build.fingerprint`. macOS and iOS share `AppleHostFactsSupport` for their
`sysctl` reads. Each final platform file retains only its platform identity and
factory binding through the common function-backed source adapter.

Potentially unavailable host values use `std::optional`. Formatting renders
them as `unavailable`; it never substitutes a plausible core count, memory
size, architecture, or fingerprint. Host reads and string formatting are
non-realtime operations and must not be called from an audio callback.

### Host-facts collaboration and design patterns

The host-facts boundary combines a factory method, strategy, and adapter with
compile-time platform selection. It is sometimes described more broadly as an
abstract factory, but the current API creates only one product type, so
`createHostFactsSource()` is more precisely a factory method.

```text
KuaruPlatformContext
        |
        v
createHostFactsSource()                 public factory method
        |
        v
FunctionHostFactsSource                 HostFactsSource adapter
        |
        v
platform reader selected by CMake       strategy selected at build/link time
   |             |              |              |
 Linux        Android          macOS           iOS
   |             |              |              |
   +-- PosixHostFactsSupport     +-- AppleHostFactsSupport
        |
        v
HostFacts                               neutral observed data
```

`HostFactsSource` is the consumer-facing abstraction. The product depends on
that interface rather than on `sysconf`, `uname`, `sysctl`, or Android system
properties. This applies dependency inversion at the product/platform
boundary.

`FunctionHostFactsSource` is the only concrete class in the source hierarchy.
It adapts a platform-reader function to `HostFactsSource`; Linux, Android,
macOS, and iOS are functions composed with shared operating-system-family
helpers, not four public subclasses. The platform translation unit linked by
CMake defines the single `createHostFactsSource()` symbol and binds the
corresponding reader. Consequently, callers perform no runtime platform switch
and public headers require no product or platform conditional.

`gigantePlatform` names the CMake target and architectural layer. It is not a
runtime platform enum, singleton, or service locator. Every source returned by
the factory is explicitly owned. A source reports observations, including
physical memory when the operating system makes it available; it does not
derive product policy from those observations. For example, iOS obtains
physical memory through the shared Apple `sysctl` reader, while Android uses
the shared POSIX `sysconf` reader. Failure remains an empty `std::optional`
rather than triggering a guessed value or silent fallback.

## Endpoint facts and selection policy

`EndpointFactSource` publishes generation-numbered snapshots of neutral facts.
An endpoint has its own directional identity, optional platform id, JUCE device
name, optional product name, direction, bus, channel count, and supported
sample rates. A duplex platform device therefore appears as separate input and
output endpoint identities even when both retain the same platform id.

`EndpointRolePolicy` is declarative: required direction, allowed buses, minimum
channels, and minimum sample rate. `evaluateEndpointSelection()` is a pure
non-realtime operation over one immutable snapshot. It returns the requested
identity, effective identity, inventory generation, a terminal status, and one
decision per inventory entry with every applicable rejection reason.

No request means no effective endpoint. Zero, one, and several eligible
candidates have separate statuses; even one candidate remains merely eligible
until explicitly requested. An unknown bus is rejected unless that exact
endpoint was requested. Duplicate endpoint identities reject every duplicate.
When a requested endpoint disappears, another eligible endpoint is marked
`notRequested` rather than silently substituted. Non-finite or negative policy
rates terminate as `invalidPolicy` before endpoint evaluation.

The fake fact source used by policy tests replaces complete snapshots and
advances their generation. `EndpointMonitor` composes an `EndpointFactSource`
with an injected `EndpointChangeSource`: a change signal carries no partial
device payload, and causes a new complete snapshot to be read and published.
Registration failures preserve their specific status and context; snapshot
failures are observable events and never masquerade as empty inventories.

The monitor owns only neutral lifecycle and delivery. `start` publishes an
initial snapshot, repeated `start` retains the original callback, and repeated
`stop` is harmless. Stop first closes delivery, unregisters the source, then
waits for callbacks already in flight. A weak internal state token makes a late
platform signal harmless even after destruction. When the callback itself
initiates teardown, stop waits for every *other* callback but never waits for
its own stack frame. All source reads and callbacks are explicitly
non-realtime. Platform inventory readers and change signals still belong to
P11E/P11F/P11G under REQ-O13 to REQ-O19.

Concrete endpoint discovery is not part of the current core boundary. On
Linux and macOS there is no core-owned device interface: product-owned logic
confirms that the requested device exists before starting `ahioenc` or
`ahiodec`, and those processes use JUCE to open it. The core does not classify
the bus, watch hot-plug events, poll devices, choose a substitute or start a
process. Failure remains explicit and no fallback is inferred. A future need
for shared discovery requires a new roadmap and requirement review rather than
speculative platform infrastructure.

## Platform, logging and build-policy layers

Group O and Group P extend the structure without changing `giganteCore`.
Host facts (P11B), the neutral endpoint value/policy layer (P11C), and the
injected endpoint-monitor lifecycle are present. Concrete platform endpoint
sources are deferred; session preparation and logging remain planned.

```text
giganteCore      JUCE-free DSP; depends on nothing below
gigantePlatform  HostFactsSource, AudioEndpoint, AudioBus, EndpointInventory,
                 EndpointPolicy, EndpointMonitor and EndpointChangeSource
                 (present as neutral/injected contracts); no concrete endpoint
                 source; SessionPreparation remains planned
giganteLog       Logger with levels, per-platform backends, build-policy gating;
                 Linux/Android foundation present; macOS native sink and
                 BuildPolicy later
```

`giganteCore` links neither. The platform layer supplies facts and never
selects; `EndpointPolicy` selects from facts and is testable with fake
inventories. `Logger` is never called from a realtime callback; realtime code
keeps `RealtimeMetrics`. Products depend on these targets, never the reverse.
The full class hierarchy and collaborations are in
`modules/ahio/ahio-architecture.md`, section "Target classes and collaborations".

### Logger structure and level precedence

`giganteLog` exposes no spdlog or JUCE type. The caller owns one explicit
`SessionIdSequence` for every logger in the process, so IDs are monotonic and
process-unique without a singleton. Each `Logger` owns its synchronous spdlog
logger and sink; it is never registered in spdlog's global registry.

```text
SessionIdSequence ──injected──> Logger
                                 ├── canonical line formatter
build ceiling ──────────────────>├── effective = min(build, runtime)
runtime request ────────────────>├── requested/effective report
                                 └── private spdlog::logger
                                       ├── callback sink (tests/adapters)
                                       ├── stderr color sink (Linux/macOS)
                                       └── Android sink (logcat)
```

Facade macros remove calls above `GIGANTE_LOG_COMPILED_LEVEL` before argument
evaluation. Runtime configuration may only lower that ceiling. With build
level `OFF`, `DisabledLogger.cpp` replaces the spdlog implementation and the
dependency is absent from the target graph. With logging enabled, spdlog
1.17.0 is pinned to commit
`79524ddd08a4ec981b7fea76afd08ee05f83755d` and supplies synchronization,
flush, native Android priority mapping and terminal color. The facade supplies
only the stable AHIO-compatible line, sanitization, field order, sessions and
requested/effective reporting.

Linux uses the thread-safe colored stderr sink; color mode is an explicit
construction option and never changes the canonical captured line. Android
uses spdlog's raw-message logcat sink. macOS can use the same terminal sink,
but its required `os_log` adapter and Apple build evidence remain a later
step. iOS is outside the current supported product focus. No asynchronous
spdlog worker, default logger, registry or realtime entry point is used.

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

## EffectsChain structure and collaborations

### Public facade and private implementations

`EffectsChain` is the single public processing facade. It owns lifecycle,
validation, requested/effective state, deterministic invalid-value recovery,
aggregate counters, observation boundaries, and the non-owning PCM contract.
The actual DSP is behind the private `EffectsChainImplementation` interface:

```text
product adapter
      |
      v
gigante::EffectsChain                    public facade
      | owns unique_ptr
      v
EffectsChainImplementation               private strategy
      |
      +-- JuceEffectsChainImplementation production fixed-order DSP
      |
      +-- NeutralEffectsChainImplementation
              standalone contract and neutral bypass
```

`createEffectsChainImplementation()` is the private factory. CMake selects
exactly one implementation: a consumer-provided `GIGANTE_JUCE_TARGET` selects
the JUCE production strategy; its absence selects the JUCE-free neutral
strategy. The public header forward-declares the implementation and exposes no
JUCE or product conditional. The neutral implementation reports that effects
are unavailable, so requesting `ProcessingMode::effects` returns
`effectsNotImplemented`; it never silently substitutes bypass.

Observation is an independent hierarchy rather than product logic embedded in
the processor:

```text
gigante::EffectsStageObserver
      |
      +-- KuaruEffectsObserver            Kuaru-owned ProfileCollector adapter
      |
      +-- RealtimeMetricsObserver         core-owned bounded neutral metrics
      |
      +-- future AhioEffectsObserver      AHIO-owned report adapter
```

The core accepts a nullable observer. `StageScope` pairs begin/end notification
with RAII around each production stage. A null observer changes neither DSP
order nor output.

### Lifecycle and one-block call

```text
construction
    -> createEffectsChainImplementation()

non-realtime setup
    -> prepare(requested configuration, initial parameters)
       -> validate sample rate, mono/stereo layout, maximum block size
       -> retain requested and effective configuration separately
       -> resolve non-finite parameters deterministically
       -> allocate/prepare implementation state and scratch

realtime block
    -> process(non-owning PCM, immutable parameter snapshot, context, observer)
       -> validate lifecycle, channel pointers/count and block size
       -> retain requested parameters and resolve effective parameters
       -> observe input and replace non-finite input samples with zero
       -> process selected implementation in place
       -> observe post-chain and replace non-finite output samples with zero
       -> publish counters/status/latency in ProcessReport

repeatable control
    -> reset(parameters, observer)
    -> release()
```

`AudioBufferView` owns no PCM and cannot escape the synchronous call. Passing a
JUCE buffer therefore needs only a bounded array of channel pointers; it adds
no FIFO, callback-size adaptation, thread hop, or boundary copy. Parameters
that are NaN or infinite retain their preceding effective values, while the
requested values and recovery counters preserve evidence of the bad request.

### Kuaru collaboration

Kuaru owns one `EffectsChain` by composition inside its synthesizer. It owns
the parameter objects, presets, voices, MIDI, instrument calibration,
polyphony policy, headset policy, and product profiling adapter. The core owns
only the musical DSP state and neutral observation contract.

```text
SinukaruPianoSynthesizer
      | owns
      v
gigante::EffectsChain
      | owns
      v
EffectsChainImplementation
      |
      +-- JuceEffectsChainImplementation in the Kuaru production build
```

The Kuaru callback collaboration is:

```text
MIDI + instrument
    -> render voices                              Kuaru
    -> instrument output gain                    Kuaru
    -> sqrt(2/N) polyphony headroom/ramp         Kuaru
    -> expose existing JUCE channel pointers
    -> map SinuAudioParam values                 Kuaru adapter
         to gigante::EffectsParameters
    -> build ProcessingContext
         sourceLevel                             instrument calibration
         generatedNoiseEnabled                   active-voice policy
         outputMuted                             Kuaru output policy
         firstSamplePosition                     product timeline
    -> EffectsChain::process()                   coreaudio
    -> debug capture/output                      Kuaru
```

Preparation and teardown follow the product device lifecycle:

```text
Kuaru prepareToPlay()       -> EffectsChain::prepare()
Kuaru resetRuntimeState()   -> EffectsChain::reset()
Kuaru releaseResources()    -> EffectsChain::release()
```

Kuaru's `effectsParameters()` is the product adapter. Headset gain capping is
resolved there before `outputGainDb` crosses the neutral boundary; the core
does not know `SinuAudioParam`, presets, headset state, or JSON. The
`KuaruEffectsObserver` maps neutral stage events to `ProfileCollector` without
making the core depend on that collector.

The current consumer integration calls `prepare()` and `process()` but does not
yet consume their returned `Status` and `ProcessReport`. The core still retains
the requested/effective state and cumulative recovery counters; surfacing
those results in Kuaru diagnostics is separate consumer hardening, not hidden
inside the DSP.

The current profiling adapter also preserves a legacy bucket boundary rather
than mapping every neutral stage one-to-one: `preGrowl()` begins at
`growlControl`, while `postFilter()` ends at `growlPostFilter`; no current
callback finalizes the legacy `growl` bucket. This does not change samples, but
it means the Kuaru profiling fields must not be described as exact mirrors of
all core stages until a separately measured adapter correction is made.

### Fixed production order and stage collaboration

The production implementation processes the granted PCM in place in this
verified order:

```text
input
  -> tremolo
  -> growl control
  -> low-pass filter
  -> growl post-filter
  -> drive
  -> bell peak
  -> phaser
  -> chorus
  -> reverb
  -> generated noise
  -> soft limiter
  -> output gain / mute
  -> output
```

The growl and filter stages deliberately collaborate:

```text
growl control
    -> measures the pre-filter envelope
    -> modulates the filter cutoff
                         |
                         v
                 low-pass filter
                         |
                         v
growl post-filter
    -> derives body, bite, grit and headroom from the filtered signal
```

State and transition ownership remains inside the implementation:

- tremolo, filter, growl, drive, and chorus parameter smoothing;
- growl envelopes and its filter-cutoff control state;
- two bell filters plus preallocated scratch for coefficient crossfade;
- phaser state and its bounded turn-off tail;
- per-channel chorus delay rings and phase;
- reverb state and tail;
- deterministic per-channel generated-noise state;
- limiter envelope; and
- the block-ramped final gain/mute state.

The current implementation reports zero algorithmic-latency samples. Its
preallocated bell crossfade performs an intentional whole-block scratch copy
only while coefficients transition; this is part of the characterized Kuaru
implementation, adds no FIFO or algorithmic delay, and is not a generic block
adapter.

Changing this order changes the sound and is not an ordinary refactor. The
initial extraction preserves it under REQ-N09 and the reviewed Kuaru goldens.
A later common fixed-order change needs its own measurement and new reviewed
evidence. A configurable order would need requested/effective topology,
preparation-time validation, bounded preallocation, and an explicit policy for
the state and tails of reordered processors. Runtime graph construction or an
implicit product/platform order is outside the current contract.

### AHIO collaboration — specified, not implemented

AHIO will own capture conditioning, encoder/decoder roles, devices, routing,
codec, packets, jitter/clocks, endpoint policy, product parameters, and report
adaptation. `EffectsChain` is placed only in render processing after
`AhioStreamDecoder` and before endpoint processing:

```text
capture endpoint
    -> source conditioning                         AHIO encoder
    -> AhioStreamEncoder
    -> AhioStreamLink
    -> AhioStreamDecoder
    -> gigante::EffectsChain                       decoder render processing
    -> endpoint correction/volume/protection       AHIO endpoint policy
    -> playback endpoint
```

For co-located USB, the roles remain explicit but execute synchronously in the
granted combined callback:

```text
JUCE input
    -> source conditioning
    -> LocalPcmStreamEncoder
    -> LocalPcmStreamLink                          identity PCM
    -> LocalPcmStreamDecoder
    -> EffectsChain                               neutral bypass in P8I
    -> endpoint processing
    -> JUCE output
```

The local encoder/link/decoder boundaries add no compression, RTP header,
serialization, FIFO, worker hop, mandatory copy, or block accumulation. P8I
first verifies neutral bypass against the existing USB baseline. P8J may then
enable one independently calibrated voice effect at a time.

For a distributed deployment, the same render boundary follows decode:

```text
encoder node                                decoder node
microphone                                  network receive/playout
  -> source conditioning                      -> Opus decode
  -> Opus encode                              -> EffectsChain
  -> RTP/UDP ------------------------------->  -> endpoint policy
                                                -> speaker
```

The core sees decoded PCM only. It never sees stream epochs, Opus frames, RTP,
sockets, jitter buffers, endpoint identities, or network clocks. AHIO may map
its first canonical sample position into `ProcessingContext`, but owns that
mapping and all transport synchronization.

The future product adapter collaboration is:

```text
ahio.json / encoder-owned control snapshot
                   |
                   v
          AhioEffectsAdapter
              |          |
              |          +-> ProcessingContext
              +-> EffectsParameters
              |
              +-> EffectsStageObserver -> AHIO bounded metrics/reporting
                   |
                   v
             EffectsChain::process()
                   |
                   v
              ProcessReport -> AHIO non-realtime report adapter
```

AHIO must not copy Kuaru's product values. Its `sourceLevel` is calibrated for
voice/input rather than piano packs; generated noise starts explicitly
disabled; its first route may be declared mono; and endpoint volume/protection
remain after render processing. When the fixed chain is enabled, its internal
soft limiter is a musical render stage and does not replace AHIO's final
endpoint protection. Core `outputGainDb` cannot silently become endpoint
volume; the AHIO adapter must define its requested and effective use.

### Optional future pitch correction at the AHIO boundary

AHIO Group Q records live pitch correction as a deferred nice-to-have. It is
not part of `EffectsChain`, current core requirements, P8I, or P8J. Because the
karaoke accompaniment may be played by another system, AHIO has no implicit
song PCM, key, timeline, or transposition.

```text
external karaoke system ----------------------------> accompaniment speaker
          |
          +-- optional versioned metadata --------┐
          +-- optional reference-audio sidechain -+--> musical context
          +-- optional recognized song/timeline --┘          |
                                                              v
vocal mic -> conditioning -> encode/link/decode -> pitch correction
                                                        |
                                                        v
                                                   EffectsChain
                                                        |
                                                        v
                                             AHIO endpoint -> vocal output
```

If later justified, pitch correction is a separate cohesive processor placed
before the existing vocal effects:

```text
decoded vocal PCM
    -> vocal pitch tracking/correction
    -> EffectsChain
    -> endpoint policy
```

It is not inserted into the fixed chain merely for convenience. A future reuse
audit may justify a neutral `gigante::PitchCorrectionProcessor`; until then,
ownership remains undecided and no core API is promised. Its collaboration
would keep musical-reference acquisition in AHIO:

```text
AHIO musical-reference source
    -> bounded immutable musical-context snapshot
    -> AHIO pitch-correction adapter
    -> separately owned pitch processor
    -> EffectsChain
```

Metadata from the external karaoke system is preferred. A dedicated
reference-audio input may instead feed a bounded non-realtime harmonic
analyzer, and an exact recognized song/version may select a precomputed
timeline if playback position and transposition are also established. Music
leaking into the vocal microphone is not a qualifying separated reference.
Without a valid reference, only an explicitly selected chromatic correction,
pitch stabilization, or disabled mode is allowed; no component invents a key
or silently retains stale harmony.

Any future pitch processor must report its algorithmic latency in samples.
Pitch-detection windows, look-ahead, delay and alignment buffers are inherent
possibilities rather than permission to hide unbounded buffering: they require
fixed limits, occupancy/latency observation, deterministic reset and loss
behavior, and a declared constant- or variable-latency bypass policy.

### Fixed chain migration boundary

Extraction is a relocation and boundary change, not an optimization phase.
Kuaru characterization precedes movement. Kuaru migrates first and must pass
reviewed equivalence gates before AHIO consumes the core. AHIO adoption remains
separately gated and has not occurred.

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

Consumer tooling may wrap this standalone configuration without becoming a
core dependency. Kuaru's `build_coreaudio` helper passes an exact commit plus a
tracked-source digest, keeps Debug and Release trees separate, enables strict
warnings, and runs CTest by default. It intentionally supplies no JUCE target;
the Kuaru product build remains the production-JUCE verification path.

The platform target uses the same optional consumer-provided JUCE interface
target but does not discover or vendor JUCE. P11B host facts need no JUCE API;
linking the interface target only preserves the single dependency authority for
later platform phases. Platform code is built with the same strict warning
policy as the DSP target.

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

Status: Accepted; P8G/P8H Kuaru integration, controlled render-budget evidence,
and the Kuaru gitlink recorded at `ec8128820fbe0ae36dff12d067578bb3ad5513e5`
are verified locally. The checked-out core is ahead during the current phase,
so its later commit still requires an exact Kuaru gitlink update. Remote
reachability was not reverified; AHIO adoption remains P8I.

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

### ADR-009 — Explicit platform-facts ownership

Status: Accepted and implemented in P11B; verified on Linux and compile-checked
for Android arm64. macOS and iOS remain unverified.

Platform selection happens once in CMake and produces an explicitly owned
facts source. Missing facts remain unavailable, scheduling outcome is injected,
and the platform layer makes no device or scheduling-policy selection.

### ADR-010 — Selection requires an explicit endpoint identity

Status: Accepted and verified in P11C.

Eligibility and selection are different states. Policy may report candidates,
but only a caller-supplied endpoint identity can become effective. The result
retains requested and effective identities separately and never substitutes an
eligible endpoint after loss.

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

The test executable replaces ordinary, array, and aligned C++ allocation only
for instrumentation and enables counting on the thread surrounding prepared
`process` calls. The shipping target contains no allocation hook. The same
test source runs against both the standalone neutral implementation and a
consumer-supplied JUCE production implementation so realtime evidence covers
the actual fixed effects chain rather than only its bypass facade.

### ADR-008 — Endpoint policy and logger as separate shared layers

Status: Revised. The endpoint value/policy portion remains implemented and
verified in P11C as an injected neutral contract; concrete platform endpoint
sources are deferred by the 2026-09-28 scope decision. The product-neutral
logger foundation is implemented and Linux/Android-verified in P12B; native
macOS `os_log`, Apple verification and product adoption remain open.

JUCE remains the only audio backend. If a caller injects endpoint facts, the
shared declarative policy can evaluate them; `gigantePlatform` does not
discover, enrich or monitor devices in the current scope. Product-owned logic
gates process startup on known device presence. The logger is a small separate
target so the core stays free of it. Putting endpoints or logging inside
`giganteCore` remains rejected because it violates REQ-N06 and the realtime
contract.
