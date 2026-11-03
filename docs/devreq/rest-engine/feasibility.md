# R0: cpp-httplib feasibility

Date: 2026-11-02. Requirements: Group R (`REQ-RC`) in
[requirements.md](../../../requirements.md).

## Decision

**Go: use cpp-httplib `v0.64.0` (`24e58ce5b25d55045af4cd6f463e83a8082fbe6b`).**

The user stopped R0 early and made this decision on the evidence below. R0 is
closed with partial coverage. Android and iOS builds, phone capacity and fuzzing
were not completed, and R1 starts by building for Android and iOS.

Use is limited to trusted networks until the idle-connection and stalled-client
gaps below are mitigated. libhv remains the fallback for a concrete blocker or
for a later need for thousands of connections or MQTT.

## Coverage

All results are from one arm64 Mac, with client and server on loopback.

| Roadmap check | Result |
|---|---|
| 1. Review | Done as an AI reading, not an audit. No confirmed defect; the areas not covered are listed below |
| 2. Build matrix | macOS arm64 only, with OpenSSL 3.5.9 and Mbed TLS 3.6.7. Android, iOS and ARMv7 were stopped before a result. Linux was not run |
| 3. TLS | Pass on both backends: 17 probe checks and 11 external `curl` and `openssl s_client` checks |
| 4. Capacity | Pass with the gaps below. No phone was run |
| 5. Slow request bodies | Pass: a total body deadline can be enforced |
| 6. Stream delivery policies | Pass on both backends, except stalled TLS clients (below) |
| 7. Lifetime and sanitizers | Lifetime pass, 63 checks on both backends; open races below |
| 8. Client | Pass, 14 checks on both backends |
| 9. Fuzzing | **Not run.** Targets compile with sanitizers but were never executed. Moved to R5 |
| 10. Mbed TLS coexistence | Conflict confirmed (below) |
| 11. `git diff --check` | Pass, plus a whitespace check of new files |

## Measurements

| Measure | OpenSSL | Mbed TLS |
|---|---|---|
| 100 mixed clients, `health` p99 | 17–38 ms | 28–51 ms |
| 100 clients, `health` failures | 0 | 0 in most runs; 1 in one run |
| 1 GiB upload / download, digest verified | 3.2 s / 3.5 s | 5.4 s / 5.2 s |
| Peak process memory across the client probe | 12 MiB | 8 MiB |

With 150 clients attempting streams, transfers and requests, the server held at
most 70 streams, used at most 104 threads, and `health` kept answering. Slow
request bodies ended at about 3.5 s against a 3 s deadline, and slow headers
released their thread after about 2 s.

## Findings that shape R1

| Finding | Evidence | Action |
|---|---|---|
| `SIGPIPE` can kill a client-only process | Reproduced; upstream ignores it only in `Server`'s constructor | Fixed in `CppHttplibClientTransport`, with `SO_NOSIGPIPE` on Apple and `SIG_IGN` elsewhere if the disposition is default |
| Range responses from a provider need status 206 | Reproduced; the library otherwise sends the whole body with 200 | Fixed in `CppHttplibTransport` |
| Mbed TLS needs a specific configuration | TSan reports on the default configuration | Threading on, self-test off, ROM AES tables, cipher list built up front (`mbedtlsThreadingConfig.h`) |
| Two Mbed TLS builds cannot share a process | 206 shared global symbols with Kuaru's crypto-only 3.6.3 build | Link one build per process. Kuaru adoption must resolve this |
| Kuaru's Mbed TLS 3.6.3.1 lacks later security fixes | Advisory review of the 3.6.4–3.6.7 releases | R0 uses 3.6.7. Reviewing Kuaru's own pin is a separate decision |
| Client reconnects are not limited by the library | Source: attempts reset on connect, and `retry:` comes from the server | Limits enforced in `subscribeEvents` |
| Path parameters arrive decoded | 13 attack values rejected, none reached the handler | Keep `Router` validation |

## Known gaps

| Gap | Evidence | Mitigation |
|---|---|---|
| **Idle connections block other requests.** The pool assigns a thread at accept, so reserved capacity protects only against streams and transfers | 150 idle sockets with 100 threads and a 32-entry queue: 9 failed `health` calls, until the 2 s read timeout cleared them. An attacker could repeat this | A 200-thread pool gave 0 failures in the same test, at about 14 MiB of memory. This is not a complete defense |
| **Stalled TLS clients hold a thread longer than the write timeout** | At 2 s: about 10–11 s on TLS and 4.2 s on plain HTTP. TLS writes retry up to 1,000 times, so the worst case may be longer | A 500 ms write timeout freed them in about 1.8 s, but may cut off slow legitimate clients |
| **Intermittent connection drops during bursts** | Mbed TLS runs only: 7–12 drops in 2 runs, below the thread limit, so the queue is the likely cause | Unproven: the larger-queue test was stopped before it finished |
| **Upstream data race** on `SSLServer::last_ssl_error_` | TSan on failed handshakes, both backends. The engine never reads it | Report upstream. Patch if needed (RC74) |
| **Mbed TLS cipher-list race** | TSan in client processes | Fixed in source, but the recheck was stopped |
| **Stuck handlers block `destroy`** | `stop()` reports a timeout correctly | By design (RC56); document it for consumers |

ASan and UBSan reported nothing. TSan was clean on the TLS, lifetime and client
probes. Under capacity load it reports the two races above. The failed memory
checks in sanitizer builds are from sanitizer overhead (184 MiB, against 12 MiB
in the normal build). The idle-flood and stalled-client failures remain real.

## Dependency trust

- **Advisories:** 23 were published, 17 of them in 2026. None is open at the pin
  by patched version. A grep shows fix code is present, not that it works.
  The redirect and IP-literal verification advisories were tested directly and pass.
- **Licenses:** cpp-httplib is MIT, and Mbed TLS is used under Apache-2.0.
  OpenSSL 3.x is Apache-2.0, from general knowledge rather than a fetched check.
  Mbed TLS's third-party section was not read, so RC75 stays open.
- **Malicious code:** a pattern scan found no process execution, dynamic loading,
  startup code, hidden characters or encoded blobs. It cannot rule out a subtle
  backdoor.
- **Unsolicited traffic:** about 2,700 socket observations during lifetime runs
  showed only loopback connections. `lsof` sampled every 0.1 s and no packet capture
  was made, so a very short connection could be missed.
- **Maintenance:** the commit is unsigned, the tag is lightweight, there is one
  dominant maintainer and the pace is high, so RC70's pin and review stay required.

The code review did not cover decompression, the file server, proxy support,
wolfSSL, the WebSocket client and handshake, stream classes, response writing or
the rest of the client.

## Roadmap reconciliation

Added as planned: 26 diagram files, this report, and the `src/rest/` and
`tests/rest_feasibility/` sources.

| Difference | Reason |
|---|---|
| `tests/rest_feasibility/Dockerfile.fuzz` not added; `mutationFuzzDriver.cpp` added | The user removed containers. Apple clang's libFuzzer did not link |
| `tests/rest_feasibility/mbedtlsThreadingConfig.h` added | Mbed TLS needs it (see findings) |
| Linux and fuzz runs dropped; Pixel not run | User direction; no Android device connected |
| `tests/rest_feasibility/buildMatrix.sh` written but its run stopped | User stopped R0 |
| Earlier documentation edits made before the roadmap | Outside the `DEV_REQ` order; kept because they record user decisions |

Raw logs were temporary session files and are not retained in the repository.
To reproduce, generate certificates with `generateTestCerts.sh` to a directory
outside the repository, build `tests/rest_feasibility` with
`-DREST_FEASIBILITY_TLS_BACKEND=OPENSSL` or `MBEDTLS`, and run each probe with
that directory. `buildMatrix.sh` covers the unverified platforms.
