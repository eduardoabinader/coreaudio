# R2: streams, copy and diagnostics

Date: 2026-11-03. Requirements: Group R (`REQ-RC`) in
[requirements.md](../../../requirements.md), R2 decisions 1–7 from the approved roadmap.

## Result

**Implemented and passing on macOS arm64, with both TLS backends.** A project can
declare live streams (SSE or binary lane) with a delivery policy. It can register a
resource store for the `copy*` family and enable bounded diagnostics. The client can
subscribe with reconnect limits, upload with resume and download with Range.

Android and iOS are compile-only: nothing has run on a device. No project uses the
library yet. It remains limited to trusted networks because of the R0 gaps.

## Verification

| Roadmap check | Result |
|---|---|
| 1. Cross builds | Pass: Android arm64-v8a and x86_64 (NDK 29.0.13599879, API 29), iOS device arm64 and Simulator arm64, Mbed TLS, Release. Library target only |
| 2. Off by default | Pass: with the option off there is no `giganteRestCore` target or fetched dependency, and the 12 existing tests pass |
| 3. Native tests | Pass: 9 of 9 `rest` tests on OpenSSL 3.5.9 and Mbed TLS 3.6.7, including the three new suites |
| 4. Sanitizers | SANITIZER_RESULT |
| 5. `curl` interoperability | Pass: 29 checks, 15 of them new: SSE framing, heartbeat, gap after `Last-Event-ID`, unknown stream, `copyList`, whole and ranged download, create, wrong offset, upload and round trip, and 411 |
| 6. R0 probes | Pass on Mbed TLS against the rewritten stream classes: TLS 17, lifetime 63, capacity 5 scenarios at scale 0.2 |
| 7. `git diff --check` | Pass, plus a whitespace check of new files |

### What the new suites cover

- **RestStreamTests:**
  - Each delivery policy against a subscriber that never reads, checked on the hub: `reliable` refuses the whole publish, `realtime` drops the oldest and counts it, `broadcast` evicts only the slow subscriber.
  - Declaration rules, SSE framing with multi-line data, increasing event IDs and reserved event names.
  - A gap event after `Last-Event-ID`, with no replay.
  - Producer close: queued events drain, then the end event; publishing afterwards returns closed, and a new subscriber gets 410.
  - Rejections: 404, the stream cap (503 and a counter, with `health` still answering), and a client refused by policy.
  - Heartbeats keep an idle stream open past the client's idle timeout.
  - A slow binary subscriber: 2,000 publishes of 16 KiB, the slowest under 100 ms, with drops counted.
  - Stop with open streams, and a stream that outlives its server.
- **RestCopyTests:**
  - List, and download whole and by Range. 403 and 404, `..` never reaching the store, and store failures without detail.
  - Upload commit, digest mismatch (422, discarded), over size (413), no space (507), path-like ID (404), not writable (403) and an empty upload.
  - The resume protocol: wrong offset (409), one open transfer per resource, a partial upload that pauses, `copyStatus`, a body past the declared size (413), commit and final state.
  - `copyCancel` (discarded, then 410), the resume window expiring, and stop discarding uncommitted transfers.
  - The transfer cap with `health` still answering, bandwidth pacing, and start refusing caps that oversubscribe `maxThreads`.
  - 1 GiB each way through a store that keeps no bytes, digest-checked on both sides, with peak RSS growing by less than 128 MiB.
- **RestDiagnosticsTests:**
  - `getLog` reads only `.log`, `.txt` and `.jsonl` files under the root. It skips hidden files, symbolic links (to files and folders) and anything beyond `maxDepth`.
  - Timestamps are parsed, CR is stripped, invalid UTF-8 is replaced, and the root path is never returned.
  - Filters all have to match. `maxLines` keeps the newest, the byte cap applies, and path-like globs get 400.
  - `cmdStatus` filters with a glob. `appendLogMarker` validates its fields, refuses newlines (log injection), gives 404 for an unknown command and 413 over its body limit.
  - Provider failures are hidden, routes that are not registered are absent, and capabilities list what is registered.

## Bugs found during R2

| Bug | Fix |
|---|---|
| Removing the moved `Sha256` class from the probe header left a fragment, so the probes no longer compiled | Fragment removed; the probes use `src/rest/Sha256` |
| The new start-time sizing check refused the shared test fixture (8 reserved + 32 streams > 32 threads); the stream tests did not check `started` | The fixture states its caps (8 streams, 8 transfers), and the stream tests now assert that start succeeded |

## Not verified

- **Device runtime:** nothing ran on Android or iOS (R4).
- **Linux:** not built here; the R1 command applies unchanged.
- **A dropped connection during an upload:** the client's resume path (status query,
  then a PUT from `bytesReceived`) is exercised only through partial uploads and
  offset conflicts. A real mid-body disconnect is not injected.
- **Download stall eviction by write timeout** is inherited from cpp-httplib and was
  measured in R0, not rerun here.
- **RC06 call-site audit:** no project uses restcore yet, so there is nothing to audit.
- **Load:** the 100-client run remains R5.

## Decisions and plan differences

- **WebSocket, multipart and `sendfile`** are not in R2 (decisions 1, 3 and 4).
  RC25 and RC44 stay partial.
- **`getLog` semantics:** the glob matches file names, never paths. Every filter has
  to appear in a line. The newest `maxLines` matches are returned. Lines are capped at
  4096 bytes, and a scan reads each matching file in full, so its cost grows with file
  size (the route is authenticated).
- **Upload protocol:** `PUT /core/v1/copy/{transferId}` with `X-Upload-Offset`.
  Content-Length is required, and a body may be partial, which is what resume uses. A
  409, 408 or 413 reply carries the server's offset in `X-Upload-Offset`.
- **411 rule:** once a store is set, the transport accepts bodies up to
  `maxTransferBytes`. So that this does not weaken other routes, a body with no
  Content-Length on a route with a smaller limit is refused with 411 before it is read.
- **Pending transfers** are capped at four times `maxTransfers`. Abandoned ones are
  discarded lazily on the next copy request after the resume window, and all of them
  at stop.
- **Declaration after start:** `declareStream`, `setResourceStore` and the diagnostics
  setters have no effect once `start()` has been called, rather than only while running.
- **`getBuildVersion` renamed to `getIdentity`** by user decision on 2026-11-03. The
  route reports the identity fixed when the binary was built; nothing is built at run
  time. This changes an R1 route, so a caller of the old path gets 404.
- **Files beyond the plan:** `RestJson.cpp` (a strict JSON object reader for request
  bodies, since restcore has no JSON library), `ClientContext.{h,cpp}` (shared client
  cancellation, the planned helper), and `Router::addCommonUpload`. The planned
  `ResponseTools` helper was not needed: the existing error builder and header
  finisher are passed in instead.
