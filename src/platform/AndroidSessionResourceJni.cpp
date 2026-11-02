#include "AndroidSessionResourceDriver.h"
#include "AndroidSessionJniSupport.h"
#include <array>
#include <mutex>
#include <utility>

namespace gigante
{
namespace
{
using namespace androidSessionJni;
struct ResourceLeases
{
    std::mutex mutex;
    std::array<std::weak_ptr<int>, 2> owners;
};
ResourceLeases& resourceLeases()
{
    static auto* leases = new ResourceLeases;
    return *leases;
}
// Hold the helper's class monitor across observe-and-act. Its existing Java
// state mutations use the same monitor, so a legacy call cannot race adoption.
class ClassMonitor final
{
public:
    ClassMonitor(JNIEnv* envIn, jclass clsIn) : env(envIn), cls(clsIn)
    {
        const auto result = env->MonitorEnter(cls);
        check(env, "MonitorEnter resource helper");
        if (result != JNI_OK) fail("MonitorEnter resource helper", "could not lock helper state", result);
    }
    ~ClassMonitor() { (void) env->MonitorExit(cls); }
private:
    JNIEnv* env;
    jclass cls;
};
class JniSessionResourceDriver final : public AndroidSessionResourceDriver
{
public:
    JniSessionResourceDriver(JNIEnv* env, jobject contextIn, jclass serviceIn, jclass wakeIn)
    {
        try
        {
            if (env) check(env, "resolve session resource helpers");
            if (!env || !contextIn || !serviceIn || !wakeIn)
                fail("create session resources", "JNI environment, application context and both resource helper classes are required");
            if (env->GetJavaVM(&vm) != JNI_OK) fail("GetJavaVM", "Java VM is unavailable");
            context = required(env, env->NewGlobalRef(contextIn), "retain resource context");
            classes[0] = static_cast<jclass>(required(env, env->NewGlobalRef(serviceIn), "retain service class"));
            classes[1] = static_cast<jclass>(required(env, env->NewGlobalRef(wakeIn), "retain wake class"));
        }
        catch (const NativeFailure& failure) { initializationError = failure.error; }
    }
    ~JniSessionResourceDriver() override
    {
        // Explicit release reports failures. Destruction is best effort only.
        for (auto resource : {SessionResource::cpuAwake, SessionResource::backgroundAudio})
            try { if (reports[index(resource)].owned) (void) release(resource); } catch (...) {}
        try
        {
            Environment environment(vm);
            auto* env = environment.get();
            if (context) env->DeleteGlobalRef(context);
            for (auto cls : classes) if (cls) env->DeleteGlobalRef(cls);
        }
        catch (...) {}
    }
    SessionResourceReport request(SessionResource resource) override { return operate(resource, 0); }
    SessionResourceReport release(SessionResource resource) override { return operate(resource, 1); }
    SessionResourceReport observe(SessionResource resource) override { return operate(resource, 2); }
private:
    static std::size_t index(SessionResource resource)
    { return resource == SessionResource::cpuAwake ? 1U : 0U; }
    static jmethodID staticMethod(JNIEnv* env, jclass cls, const char* name, const char* signature)
    {
        auto result = env->GetStaticMethodID(cls, name, signature);
        check(env, name);
        if (!result) fail(name, "resource helper method unavailable");
        return result;
    }
    static std::string stringValue(JNIEnv* env, jobjectArray array, jsize position)
    {
        auto value = static_cast<jstring>(required(env, env->GetObjectArrayElement(array, position), "snapshot field"));
        const auto* utf = env->GetStringUTFChars(value, nullptr);
        check(env, "snapshot string");
        if (!utf) fail("snapshot string", "string unavailable");
        std::string result(utf);
        env->ReleaseStringUTFChars(value, utf);
        return result;
    }
    void snapshot(JNIEnv* env, std::size_t slot)
    {
        auto array = static_cast<jobjectArray>(required(env, env->CallStaticObjectMethod(classes[slot],
            staticMethod(env, classes[slot], "snapshot", "()[Ljava/lang/String;")), "resource snapshot"));
        if (env->GetArrayLength(array) != 2) fail("resource snapshot", "invalid resource snapshot schema");
        const auto state = stringValue(env, array, 0);
        auto& report = reports[slot];
        report.supported = true;
        if (state == "inactive") report.observed = SessionResourceState::inactive;
        else if (state == "active") report.observed = SessionResourceState::active;
        else if (state == "pending") report.observed = SessionResourceState::pending;
        else if (state == "stopPending") report.observed = SessionResourceState::stopPending;
        else fail("resource snapshot", "unknown observed state");
        const auto error = stringValue(env, array, 1);
        if (!error.empty()) report.error = SessionError{"resource snapshot", "Android.AudioResource", 0, error};
        if (report.owned && report.observed == SessionResourceState::inactive)
        {
            report.owned = false;
            resourceLeases().owners[slot].reset();
        }
    }
    SessionResourceReport operate(SessionResource resource, int operation)
    {
        auto& leases = resourceLeases();
        std::lock_guard<std::mutex> lock(leases.mutex);
        const auto slot = index(resource);
        auto& report = reports[slot];
        if (operation != 2)
        {
            report.requestedActive = operation == 0;
            report.operationAccepted.reset();
            report.error.reset();
            report.cleanupError.reset();
        }
        if (initializationError)
        {
            report.error = initializationError;
            report.observed = SessionResourceState::unavailable;
            return report;
        }
        try
        {
            Environment environment(vm);
            auto* env = environment.get();
            ClassMonitor monitor(env, classes[slot]);
            if (operation == 1 && report.owned)
            {
                // Observation failure must not prevent an owned stop attempt.
                try { snapshot(env, slot); }
                catch (const NativeFailure& failure) { report.error = failure.error; }
            }
            else snapshot(env, slot);
            if (operation == 2) return report;
            if (operation == 0 && report.owned)
            {
                // A stop-pending lease cannot be restarted implicitly.
                if (report.observed == SessionResourceState::stopPending)
                {
                    report.operationAccepted = false;
                    report.error = SessionError{"request resource", "Android.AudioResource", 0,
                        "owned resource is stopping; observe completion before requesting again"};
                }
                // Otherwise this is an idempotent observation, not another native start.
                return report;
            }
            if (operation == 1 && !report.owned)
            {
                // Observation may show a legacy or another session's resource.
                // Never stop it, and never claim a native stop was accepted.
                return report;
            }
            if (operation == 0)
            {
                if (!leases.owners[slot].expired() || report.observed != SessionResourceState::inactive)
                {
                    report.operationAccepted = false;
                    report.error = SessionError{"request resource", "Android.AudioResource", 0,
                        "resource is already active or pending outside this session"};
                    return report;
                }
                leases.owners[slot] = owner;
                report.owned = true; // Retain cleanup responsibility across a partial JNI failure.
            }
            report.error.reset();
            const char* name = slot == 0 ? (operation == 0 ? "start" : "stop")
                                         : (operation == 0 ? "acquire" : "release");
            jboolean accepted;
            if (slot == 1 && operation == 0)
            {
                auto tag = static_cast<jstring>(required(env, env->NewStringUTF("gigantelabs:audio"), "wake lock tag"));
                accepted = env->CallStaticBooleanMethod(classes[slot],
                    staticMethod(env, classes[slot], name, "(Landroid/content/Context;Ljava/lang/String;)Z"), context, tag);
            }
            else accepted = env->CallStaticBooleanMethod(classes[slot],
                staticMethod(env, classes[slot], name, "(Landroid/content/Context;)Z"), context);
            check(env, name);
            report.operationAccepted = accepted == JNI_TRUE;
            snapshot(env, slot);
            if (!accepted && !report.error)
                report.error = SessionError{name, "Android.AudioResource", 0, "native resource operation was not accepted"};
            if (operation == 1 && report.error) report.cleanupError = report.error;
        }
        catch (const NativeFailure& failure)
        {
            report.error = failure.error;
            if (operation == 1) report.cleanupError = failure.error;
            // Unknown observation after a JNI failure, not a fabricated stopped state.
            report.observed = SessionResourceState::failed;
        }
        return report;
    }
    JavaVM* vm{nullptr};
    jobject context{nullptr};
    std::array<jclass, 2> classes{};
    SessionResourceReports reports{initialSessionResources()};
    std::shared_ptr<int> owner{std::make_shared<int>(0)};
    std::optional<SessionError> initializationError;
};
}
std::unique_ptr<AndroidSessionResourceDriver> makeAndroidSessionResourceDriver(
    JNIEnv* env, jobject context, jclass serviceClass, jclass wakeClass)
{
    return std::make_unique<JniSessionResourceDriver>(env, context, serviceClass, wakeClass);
}
} // namespace gigante
