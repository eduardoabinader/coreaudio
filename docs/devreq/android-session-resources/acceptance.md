# Android session resources — C2A acceptance

Date: 2026-11-02. Requirement: REQ-O20. Core baseline: `3c174ed`.

C2A is implemented and verified with host fixtures and cross-builds. REQ-O20
remains partial: physical Android resource behavior and Kuaru adoption are C2B,
not acceptance inferred from this phase. No Kuaru or AHIO owner was switched.

## Scope and behavior

The common SessionPreparation interface adds independent background-audio and
CPU-awake requests, explicit release and observation. Reports separate intent,
optional operation acceptance, native observations, support, ownership and
operation/cleanup causes. No native call on an idempotent operation is reported
as a newly accepted native start. Focus grant, service start accepted and actual
service running remain different facts.

ManagedSession serializes resource operations and keeps their reports through
focus prepare/release. Teardown attempts owned wake lock, owned service, then
focus. Resource failure leaves other independent resources/focus available.
Pending service stop retains ownership until observation sees inactive. An
original partial-request error survives a later cleanup error separately.

The optional Android driver holds process resource leases and the Java helper's
class monitor across observation and mutation. It refuses external active or
pending resources and never releases an unowned resource. Its application
context and helper classes are explicitly injected and retained through global
JNI references. Observation failures do not block an owned cleanup attempt.
The existing three-argument factory stays focus-only. No resource callbacks,
polling timer, main-loop wait or consumer readiness/recovery policy is added.

Shared Java helpers add atomic two-string snapshots (state, last native error),
retaining their existing method signatures and boolean/exception semantics.
Stop-during-start enters the required foreground state and stays stop-pending
until destruction. Release acceptance never substitutes for isHeld/onDestroy.

## Verification

- macOS standalone strict-warning build and CTest: **11/11 pass**, including
  existing focus lifetime/callback tests, new resource contract tests, DSP/golden
  vector checks, and public-header isolation compilation. No consumer-provided
  JUCE target is used in this standalone build.
- New resource tests cover requested/accepted/observed separation, pending stop,
  explicit observation, resource-only destructor cleanup, release ordering,
  failure independence, explicit cleanup retry, original plus cleanup causes,
  unsupported results and unowned teardown refusal.
- Real JNI driver on a host JVM with `-Xcheck:jni`: passes against fixture helper
  classes. It checks repeated request, competing leases, external ownership,
  pending stop, Java SecurityException provenance, failed-release retry,
  malformed pre-stop observation, missing helpers, retained JNI references and
  attachment/detachment from a native worker thread. This is not Android OS
  service verification.
- The **actual shared Java helpers** pass one JVM fixture test covering pending
  start/stop/onDestroy, legacy isRunning, denied start/acquire/release, held-state
  accuracy and retry/error clearing. They also compile against Android API 36
  with `javac -Xlint:all -Werror`. JVM fixtures do not emulate OS restrictions.
- Android arm64/API 29: gigantePlatform cross-build passes with NDK
  29.0.13599879, including both focus and resource JNI sources.
- iOS arm64/iPhoneOS minimum 16: gigantePlatform build passes, including the
  existing Objective-C++ session implementation. This is compile compatibility,
  not iOS runtime reacceptance.
- Four SVGs parse, HTML previews reference their SVG siblings, and
  `git diff --check` passes in coreaudio and Kuaru.

Reproduction from the coreaudio root (host SDK/JDK/NDK paths are local examples):

```bash
cmake -S . -B build -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build build -j 6
ctest --test-dir build --output-on-failure
python3 tests/android_session_resources/run_jni_tests.py \
  --ndk /Users/U464276/Library/Android/sdk/ndk/29.0.13599879
cmake -S . -B /tmp/coreaudio-c2a-android \
  -DCMAKE_TOOLCHAIN_FILE=/Users/U464276/Library/Android/sdk/ndk/29.0.13599879/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
  -DGIGANTE_BUILD_TESTS=OFF -DGIGANTE_LOG_COMPILED_LEVEL=OFF
cmake --build /tmp/coreaudio-c2a-android --target gigantePlatform -j 6
cmake -S . -B /tmp/coreaudio-c2a-ios -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DGIGANTE_BUILD_TESTS=OFF -DGIGANTE_LOG_COMPILED_LEVEL=OFF
cmake --build /tmp/coreaudio-c2a-ios --target gigantePlatform -j 6
```

The offline host command assumes the existing pinned spdlog checkout is already
available in build/_deps. The optional JNI runner needs NDK headers and a JDK
supporting `--enable-native-access` (JDK 27 used here); its macOS execution was
verified. Linux runner execution was not verified.

From the Kuaru root:

```bash
python3 -m unittest discover -s modules/android_audio_runtime/tests -v
javac -Xlint:all -Werror \
  -cp /Users/U464276/Library/Android/sdk/platforms/android-36/android.jar \
  -d /tmp/coreaudio-c2a-java \
  modules/android_audio_runtime/java/com/gigantelabs/audio/AudioForegroundService.java \
  modules/android_audio_runtime/java/com/gigantelabs/audio/AudioPartialWakeLock.java
```

## File plan and reconciliation

All changes remain within the approved resource-contract/driver/helper scope.
Private JNI support was extracted to share the existing C1 attachment/error code
without duplication. The additional JNI runner/fixtures and actual-helper JVM
test provide independent boundary evidence; they do not add consumer adoption.
No files are removed. Existing captures, unrelated untracked files and build
outputs are preserved and excluded. No files were staged or committed.

Coreaudio:

```text
[MODIFY] CMakeLists.txt
[MODIFY] CLAUDE.md
[MODIFY] requirements.md
[MODIFY] architecture.md
[MODIFY] Memory.md
[MODIFY] include/gigante/{SessionPreparation,AndroidSessionPreparation}.h
[MODIFY] src/platform/SessionPreparation.cpp
[MODIFY] src/platform/SessionPreparationImplementation.h
[MODIFY] src/platform/AndroidAudioFocusDriver.h
[MODIFY] src/platform/AndroidSessionPreparation.cpp
[MODIFY] src/platform/AndroidSessionJni.cpp
[ADD]    src/platform/AndroidSessionJniSupport.h
[ADD]    src/platform/AndroidSessionResourceDriver.h
[ADD]    src/platform/AndroidSessionResourceJni.cpp
[MODIFY] tests/CMakeLists.txt
[ADD]    tests/AndroidSessionResourceTests.cpp
[ADD]    tests/AndroidSessionResourceJniTests.cpp
[ADD]    tests/android_session_resources/run_jni_tests.py
[ADD]    tests/android_session_resources/java/android/content/Context.java
[ADD]    tests/android_session_resources/java/com/gigantelabs/tests/ResourceDriverHarness.java
[ADD]    docs/devreq/android-session-resources/acceptance.md
[ADD]    docs/devreq/android-session-resources/{resource-class,resource-prepare,pending-stop,resource-failure-release}.{svg,html}
[REMOVE] (none)
```

Kuaru (shared helpers/documentation only):

```text
[MODIFY] modules/android_audio_runtime/java/com/gigantelabs/audio/AudioForegroundService.java
[MODIFY] modules/android_audio_runtime/java/com/gigantelabs/audio/AudioPartialWakeLock.java
[MODIFY] modules/android_audio_runtime/README.md
[MODIFY] memory/MEMORY.md
[ADD]    modules/android_audio_runtime/tests/test_resource_snapshots.py
[REMOVE] (none)
```

## Deferred boundary

C2B must review product ordering, eliminate overlapping legacy/shared owners,
preserve nonfatal startup policy, wire manifest/R8 integration and qualify on a
physical Android device before claiming Kuaru migration. No APK was built,
installed or launched here. Multicast locks, scheduling, screen-awake, Bluetooth
output exclusion, DSP changes and AHIO adoption remain outside C2A. This phase
provides no automatic Android restart or iOS device-recovery behavior.
