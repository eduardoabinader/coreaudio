#pragma once
#include "gigante/SessionPreparation.h"
#if defined(__ANDROID__)
#include <jni.h>
#endif

namespace gigante
{
// Owns native resource leases; fake implementations permit hardware-free tests.
class AndroidSessionResourceDriver
{
public:
    virtual ~AndroidSessionResourceDriver() = default;
    virtual SessionResourceReport request(SessionResource resource) = 0;
    virtual SessionResourceReport release(SessionResource resource) = 0;
    virtual SessionResourceReport observe(SessionResource resource) = 0;
};
#if defined(__ANDROID__)
std::unique_ptr<AndroidSessionResourceDriver> makeAndroidSessionResourceDriver(
    JNIEnv* env, jobject context, jclass serviceClass, jclass wakeClass);
#endif
} // namespace gigante
