#pragma once
#include "gigante/AudioSession.h"
#include <jni.h>
#include <string>

namespace gigante::androidSessionJni
{
struct NativeFailure
{
    SessionError error;
};
[[noreturn]] inline void fail(const char* operation, const char* message, int code = 0)
{
    throw NativeFailure{SessionError{operation, "JNI", code, message}};
}
inline void check(JNIEnv* env, const char* operation)
{
    if (!env->ExceptionCheck()) return;
    auto exception = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string message{"Java exception (description unavailable)"};
    auto cls = env->GetObjectClass(exception);
    auto method = cls ? env->GetMethodID(cls, "toString", "()Ljava/lang/String;") : nullptr;
    auto description = method ? static_cast<jstring>(env->CallObjectMethod(exception, method)) : nullptr;
    if (!env->ExceptionCheck() && description)
    {
        const auto* utf = env->GetStringUTFChars(description, nullptr);
        if (utf) { message = utf; env->ReleaseStringUTFChars(description, utf); }
    }
    // Describing an exception must not leave a second pending JNI exception.
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (description) env->DeleteLocalRef(description);
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(exception);
    const auto colon = message.find(':');
    throw NativeFailure{SessionError{operation, message.substr(0, colon), 0, message}};
}
inline jobject required(JNIEnv* env, jobject value, const char* operation)
{
    check(env, operation);
    if (!value) fail(operation, "JNI returned null");
    return value;
}
inline jclass findClass(JNIEnv* env, const char* name)
{
    return static_cast<jclass>(required(env, env->FindClass(name), name));
}
inline jmethodID method(JNIEnv* env, jclass cls, const char* name, const char* signature)
{
    auto id = env->GetMethodID(cls, name, signature);
    check(env, name);
    if (!id) fail(name, "JNI method was not found");
    return id;
}
class Environment final
{
public:
    explicit Environment(JavaVM* vmIn) : vm(vmIn)
    {
        if (!vm) fail("GetEnv", "Java VM is unavailable");
        const auto result = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED)
        {
            const auto attachedResult = vm->AttachCurrentThread(&env, nullptr);
            if (attachedResult != JNI_OK) fail("AttachCurrentThread", "thread attachment failed", attachedResult);
            attached = true;
        }
        else if (result != JNI_OK) fail("GetEnv", "JNI environment is unavailable", result);
        if (env->PushLocalFrame(48) != JNI_OK)
        {
            // Detach on constructor failure as its destructor will not run.
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (attached) vm->DetachCurrentThread();
            fail("PushLocalFrame", "could not allocate JNI local reference frame");
        }
    }
    ~Environment()
    {
        env->PopLocalFrame(nullptr);
        if (attached) vm->DetachCurrentThread();
    }
    JNIEnv* get() const noexcept { return env; }
private:
    JavaVM* vm;
    JNIEnv* env{nullptr};
    bool attached{false};
};

}
