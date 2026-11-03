# R1: first working `gigante::restcore`

Date: 2026-11-02. Requirements: Group R (`REQ-RC`) in
[requirements.md](../../../requirements.md). Decision record: R.13 there.

## Result

**Implemented and passing on macOS arm64, with both TLS backends.** A C++ project
can link `gigante::restcore` and get a server and client with HTTP in development,
HTTPS in production, routes, always-on common endpoints and opt-in service lifecycle.

Android and iOS are compile-only: nothing has run on a device. No project uses the
library yet. It remains limited to trusted networks because of the gaps below.

## Verification

| Roadmap check | Result |
|---|---|
| 1. Cross builds | Pass: Android arm64-v8a and x86_64 (NDK 29), iOS device and Simulator arm64, macOS x86_64 with Mbed TLS. Library target only |
| 2. Off by default | Pass: with the option off there is no `giganteRestCore` target, no fetched dependency, and the 11 existing tests pass |
| 3. Header isolation | Pass: public headers compile without cpp-httplib on the include path |
| 4. Native tests | Pass: 17 of 17 tests on OpenSSL 3.5.9 and Mbed TLS 3.6.7. The REST tests passed 5 of 5 repeat runs on each backend. After the final test fix below, the full suite passed once more on each |
| 5. Security | Pass, except as noted under "Not verified" |
| 6. Lifecycle | Pass: 202 before the callback, self-stop without deadlock, 10 restarts, repeat stop, stuck-handler timeout, no handler after destroy |
| 7. Optional dispatcher | Pass: injected thread, 503 when full, 504 at the deadline, recovery afterward |
| 8. `curl` interoperability | Pass: 14 checks, including HEAD and TLS rejections |
| 9. Sanitizers | Pass: no ASan, UBSan or TSan report in 3 rounds on each backend |
| 10. `git diff --check` | Pass, plus a whitespace check of new files |

The REST tests compile as C++17. The rest of the repository still needs C++20
because `Logger.h` uses `std::span`, so the full build used
`-DCMAKE_CXX_STANDARD=20`.

The R0 probes still build and pass against the changed classes (TLS 17, lifetime 63),
on Mbed TLS only. Capacity and client probes were not rerun.

## Bugs found by the tests

| Bug | Fix |
|---|---|
| A second `requestStop()` after a restart aborted the process by reassigning an unjoined thread | Join the finished thread first; refuse during destruction |
| Test flags declared after the server were destroyed while a handler could still read them (ASan) | Test order fixed. The rule is now in `RestServer.h`, since users can make the same mistake |

## Not verified

- **Device runtime:** nothing ran on Android or iOS. This belongs to R4.
- **Linux:** not built here. To reproduce on a Linux host:
  `cmake -S . -B b -G Ninja -DCMAKE_CXX_STANDARD=20 -DGIGANTE_BUILD_REST=ON && cmake --build b && ctest --test-dir b -L rest`
- **Plaintext removed at build time:** `GIGANTE_REST_ALLOW_PLAINTEXT=OFF` builds, but
  its refusal has no test.
- **TLS minimum version:** the engine relies on cpp-httplib setting TLS 1.2. R0's
  external checks confirmed the rejection on the probes, but they were not repeated
  against `RestServer`, so RC15's "enforce it itself" is not yet met.
- **Concurrent failed handshakes:** R1's tests do not reproduce upstream's
  `last_ssl_error_` race, so a clean TSan run here does not mean it has gone.
- **Load:** R1 did not rerun capacity tests. The default sizing of 64 base threads
  and a 128-entry queue is unproven, because R0's larger-queue test was stopped.

## Known gaps carried from R0

The idle-connection flood and the slow stalled-TLS release remain, as recorded in the
[R0 report](feasibility.md). R1 uses the same thread model, so production use stays
limited to trusted networks.

## Decisions made during R1

- **Logging:** restcore takes an injected log callback instead of linking
  `gigante::log`, because `Logger.h` needs C++20 and restcore is C++17. It also keeps
  spdlog out of the engine. Projects can forward the callback to their `Logger`.
- **JSON:** restcore has no JSON library. Handlers supply serialized text, and common
  endpoints build their own bodies. Typed client calls return the raw JSON body.
- **Middleware:** the chain is fixed (CORS, routing check, body limit, authentication).
  Project-supplied middleware is not in R1.
- **Method names:** `GET`, `POST` and so on stay uppercase, as an agreed exception to
  the camelCase rule, because they name HTTP methods.

## Roadmap reconciliation

Added as planned: the public headers, the listed `src/rest` files, the listed tests,
`mbedtlsRestConfig.h` and this file. `tests/rest_feasibility/mbedtlsThreadingConfig.h`
was removed as planned.

| Difference | Reason |
|---|---|
| Added `cmake/GiganteRest.cmake` | Keeps the optional target out of the main `CMakeLists.txt` |
| Added `src/rest/RestJson.h` | Shared escaping and the error body |
| Added `tests/RestTestSupport.h` | Shared test helpers |
| `src/rest/HandlerDispatcher.h` holds `DispatchGate`; the interface is the public `RestDispatcher` | Projects must implement it, so it has to be public |
| Doc updates were made last, not first | So the docs record tested results |

## Reproduce

```bash
cmake -S modules/coreaudio -B build -G Ninja \
  -DCMAKE_CXX_STANDARD=20 -DGIGANTE_LOG_COMPILED_LEVEL=OFF -DGIGANTE_BUILD_REST=ON \
  -DGIGANTE_REST_TLS_BACKEND=OPENSSL -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
cmake --build build
ctest --test-dir build -L rest --output-on-failure
```

Use `-DGIGANTE_REST_TLS_BACKEND=MBEDTLS` for Mbed TLS. For Android add
`-DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a
-DANDROID_PLATFORM=android-29 -DGIGANTE_BUILD_TESTS=OFF` and build `--target
giganteRestCore`. For iOS add `-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos
-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DGIGANTE_BUILD_TESTS=OFF`.
