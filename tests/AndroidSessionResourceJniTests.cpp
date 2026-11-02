#include "AndroidSessionResourceDriver.h"
#include <stdexcept>
#include <thread>

namespace
{
void require(bool condition, const char* reason)
{ if (!condition) throw std::runtime_error(reason); }
void setState(JNIEnv* env, jclass cls, const char* state)
{
    auto field = env->GetStaticFieldID(cls, "state", "Ljava/lang/String;");
    auto text = env->NewStringUTF(state);
    env->SetStaticObjectField(cls, field, text);
    env->DeleteLocalRef(text);
}
}
extern "C" JNIEXPORT void JNICALL Java_com_gigantelabs_tests_ResourceDriverHarness_nativeRun(
    JNIEnv* env, jclass, jobject context, jclass service, jclass wake)
{
    using namespace gigante;
    try
    {
        auto first = makeAndroidSessionResourceDriver(env, context, service, wake);
        auto second = makeAndroidSessionResourceDriver(env, context, service, wake);
        SessionResourceReport workerReport;
        std::thread worker([&] { workerReport = first->observe(SessionResource::cpuAwake); });
        worker.join();
        require(workerReport.supported && !workerReport.error
                && workerReport.observed == SessionResourceState::inactive,
                "detached caller thread could not attach and observe via retained references");
        auto result = first->request(SessionResource::backgroundAudio);
        require(result.owned && result.operationAccepted == true && result.observed == SessionResourceState::pending,
                "service acceptance invented running state");
        result = first->request(SessionResource::backgroundAudio);
        require(result.owned && !result.operationAccepted && env->GetStaticIntField(service, env->GetStaticFieldID(service, "starts", "I")) == 1,
                "repeated request started service twice");
        result = second->request(SessionResource::backgroundAudio);
        require(!result.owned && result.error && result.operationAccepted == false, "another session adopted the lease");
        result = first->release(SessionResource::backgroundAudio);
        require(result.owned && result.observed == SessionResourceState::stopPending, "pending stop reported stopped");
        result = first->request(SessionResource::backgroundAudio);
        require(result.operationAccepted == false && result.error, "pending stop allowed implicit restart or lost cause");
        setState(env, service, "inactive");
        result = first->observe(SessionResource::backgroundAudio);
        require(!result.owned && result.observed == SessionResourceState::inactive, "completed stop did not release lease");
        setState(env, service, "active");
        result = second->request(SessionResource::backgroundAudio);
        require(!result.owned && result.error, "externally running service silently adopted");
        (void) second->release(SessionResource::backgroundAudio);
        require(env->GetStaticIntField(service, env->GetStaticFieldID(service, "stops", "I")) == 1,
                "unowned release stopped an external service");
        (void) first->request(SessionResource::cpuAwake);
        require(!second->request(SessionResource::cpuAwake).owned, "wake lease adopted by another session");
        auto throwing = env->GetStaticFieldID(wake, "throwRelease", "Z");
        env->SetStaticBooleanField(wake, throwing, JNI_TRUE);
        result = first->release(SessionResource::cpuAwake);
        require(result.owned && result.error && result.cleanupError
                && result.error->domain == "java.lang.SecurityException", "JNI cleanup lost ownership or exception cause");
        env->SetStaticBooleanField(wake, throwing, JNI_FALSE);
        result = first->release(SessionResource::cpuAwake);
        require(!result.owned && !result.error, "explicit JNI cleanup retry failed");
        (void) first->request(SessionResource::cpuAwake);
        setState(env, wake, "invalid-snapshot-state");
        result = first->release(SessionResource::cpuAwake);
        require(!result.owned && !result.error && result.observed == SessionResourceState::inactive,
                "pre-stop snapshot failure prevented an owned cleanup attempt");
        auto unavailable = makeAndroidSessionResourceDriver(env, context, nullptr, nullptr);
        result = unavailable->request(SessionResource::cpuAwake);
        require(!result.supported && result.error && !result.owned, "missing helper reported success");
        require(!env->ExceptionCheck(), "handled JNI failure left a pending exception");
    }
    catch (const std::exception& error)
    {
        env->ThrowNew(env->FindClass("java/lang/AssertionError"), error.what());
    }
}
