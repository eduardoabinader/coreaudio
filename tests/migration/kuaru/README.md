# Kuaru pre-migration reference vectors

This directory defines P8F characterization inputs. It does not contain or
compile Kuaru production code. The current Kuaru offline renderer remains the
rendering authority and must be supplied explicitly; no checkout layout is
assumed by the core.

Capture candidates into a new directory:

```sh
cmake \
  -DKUARU_BINARY=/absolute/path/to/sinukaru \
  -DKUARU_CONFIG=/absolute/path/to/kuaru.kuaruconf \
  -DKUARU_SAMPLES_DIR=/absolute/path/to/samplepack \
  -DOUTPUT_DIR=/tmp/kuaru-p8f-candidates \
  -P cmake/CaptureKuaruReferences.cmake
```

Use the effective configuration artifact accepted by the supplied binary. A
sinupack build requires its generated encrypted `kuaru.kuaruconf`; passing the
plain source JSON correctly fails before rendering.

The script refuses an output directory that already has a candidate manifest.
It never writes expected vectors into the source tree and never updates a
golden automatically. Promotion requires reviewing render logs, metadata,
hashes, audio, and every difference, then adding the accepted artifacts
deliberately. P8F's reviewed artifacts are under `tests/golden/kuaru`; capture
never writes there.

Each run writes an auditionable 24-bit WAV and an interleaved float32-LE PCM
vector. Determinism is checked on the raw float vector: JUCE's float-to-24-bit
WAV conversion uses low-bit dither, so container hashes can legitimately differ
by one output LSB even when the engine's float output is identical. Both hashes
remain in the candidate manifest; only the undithered float hash is the
bit-exact same-build gate.

The scenarios exercise the shipped Kuaru `prepareToPlay`/`processBlock` path:

- transient and tail behavior at fixed small and non-power-of-two blocks;
- sustained material and a live multi-effect parameter timeline;
- deterministic reset/process startup by repeating a render in a fresh process;
- an oversized numeric parameter intended to exercise the existing bounded
  parameter/recovery path without non-standard JSON NaN syntax.

Core-only mono/stereo, varying-block, lifecycle, bypass, and non-finite PCM
recovery are covered by `giganteCoreTests`. Kuaru's offline renderer is stereo
and fixed-block by contract, so this migration harness does not pretend it
provides evidence for layouts or callback geometries that it cannot render.
