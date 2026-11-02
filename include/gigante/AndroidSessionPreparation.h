#pragma once
#include "gigante/SessionPreparation.h"
#if defined(__ANDROID__)
#include <jni.h>
namespace gigante
{
// Call on a consumer thread with the application class loader. Resolve the
// shared AudioFocusListener class there and pass it explicitly. Backend retains
// global refs; caller's local context/class refs need not outlive this call.
// Null context/env/class and JNI failures produce an explicit failed report.
[[nodiscard]] std::unique_ptr<SessionPreparation> createAndroidSessionPreparation(
    JNIEnv* env, jobject applicationContext, jclass listenerClass);
// Optional resources are explicitly injected; the three-argument focus factory
// remains compatible and reports resources unavailable. Resolve helper classes
// through the application loader and retain their methods in shrinker rules.
[[nodiscard]] std::unique_ptr<SessionPreparation> createAndroidSessionPreparation(
    JNIEnv* env, jobject applicationContext, jclass listenerClass,
    jclass foregroundServiceClass, jclass wakeLockClass);
} // namespace gigante
#endif
