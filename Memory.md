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
