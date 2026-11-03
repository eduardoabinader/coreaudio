#include "gigante/AndroidAudioSession.h"
#include "AndroidAudioFocusDriver.h"
#include "AndroidSessionJniSupport.h"

#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace gigante
{
namespace
{
using namespace androidSessionJni;

struct FocusCallbackRegistry
{
    std::mutex mutex;
    std::map<jlong, AndroidAudioFocusDriver::Callback> callbacks;
    jlong nextToken{1};
};
FocusCallbackRegistry& focusCallbackRegistry()
{
    // JNI callbacks and a retained product adapter may outlive static teardown
    // in another translation unit. Keep the registry anchor for process life;
    // release still removes every active sink and its consumer references.
    static auto* registry = new FocusCallbackRegistry;
    return *registry;
}
jlong registerCallback(AndroidAudioFocusDriver::Callback callback)
{
    auto& registry = focusCallbackRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    if (registry.nextToken == std::numeric_limits<jlong>::max())
        fail("registerCallback", "focus callback token space exhausted");
    const auto token = registry.nextToken++;
    registry.callbacks.emplace(token, std::move(callback));
    return token;
}
void forgetCallback(jlong token) noexcept
{
    auto& registry = focusCallbackRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.callbacks.erase(token);
}
void JNICALL onFocusChange(JNIEnv*, jclass, jlong token, jint change)
{
    // Never hold the registry lock while invoking the shared callback gate.
    // A copied sink carries a weak generation-checked session, never a raw this.
    try
    {
        AndroidAudioFocusDriver::Callback callback;
        {
            auto& registry = focusCallbackRegistry();
            std::lock_guard<std::mutex> lock(registry.mutex);
            const auto found = registry.callbacks.find(token);
            if (found == registry.callbacks.end()) return;
            callback = found->second;
        }
        callback(static_cast<int>(change));
    }
    catch (...) { /* Consumer exceptions are recorded by ManagedAudioSession. */ }
}
class JniAudioFocusDriver final : public AndroidAudioFocusDriver
{
public:
    JniAudioFocusDriver(JNIEnv* env, jobject contextIn, jclass listenerClassIn)
    {
        try
        {
            if (env) check(env, "resolve AudioFocusListener");
            if (!env || !contextIn || !listenerClassIn)
                fail("createAndroidAudioSession", "JNI environment, application context and listener class are required");
            const auto result = env->GetJavaVM(&vm);
            if (result != JNI_OK) fail("GetJavaVM", "Java VM is unavailable", result);
            context = required(env, env->NewGlobalRef(contextIn), "retain application context");
            listenerClass = static_cast<jclass>(required(env, env->NewGlobalRef(listenerClassIn), "retain listener class"));
            JNINativeMethod binding{const_cast<char*>("nativeOnFocusChange"),
                const_cast<char*>("(JI)V"), reinterpret_cast<void*>(&onFocusChange)};
            const auto registered = env->RegisterNatives(listenerClass, &binding, 1);
            check(env, "RegisterNatives");
            if (registered != JNI_OK) fail("RegisterNatives", "focus callback registration failed", registered);
        }
        catch (const NativeFailure& error) { initializationError = error.error; }
    }
    ~JniAudioFocusDriver() override
    {
        (void) release();
        try
        {
            Environment environment(vm);
            auto* env = environment.get();
            clearRequest(env);
            if (context) env->DeleteGlobalRef(context);
            if (listenerClass) env->DeleteGlobalRef(listenerClass);
        }
        catch (...) { /* Native failure was reported by explicit release. */ }
    }
    AndroidFocusOperationResult request(const SessionAudioFocusReport& options, Callback callback) override
    {
        if (initializationError) return {{}, initializationError};
        try
        {
            Environment environment(vm);
            auto* env = environment.get();
            auto contextClass = static_cast<jclass>(required(env, env->GetObjectClass(context), "context class"));
            auto audioName = static_cast<jstring>(required(env, env->NewStringUTF("audio"), "audio service name"));
            auto manager = required(env, env->CallObjectMethod(context,
                method(env, contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;"), audioName), "getSystemService");
            auto attributesClass = findClass(env, "android/media/AudioAttributes$Builder");
            auto attributes = required(env, env->NewObject(attributesClass,
                method(env, attributesClass, "<init>", "()V")), "AudioAttributes.Builder");
            env->CallObjectMethod(attributes, method(env, attributesClass, "setUsage",
                "(I)Landroid/media/AudioAttributes$Builder;"), static_cast<jint>(options.requestedUsage));
            check(env, "setUsage");
            env->CallObjectMethod(attributes, method(env, attributesClass, "setContentType",
                "(I)Landroid/media/AudioAttributes$Builder;"), static_cast<jint>(options.requestedContentType));
            check(env, "setContentType");
            auto builtAttributes = required(env, env->CallObjectMethod(attributes,
                method(env, attributesClass, "build", "()Landroid/media/AudioAttributes;")), "build audio attributes");
            token = registerCallback(std::move(callback));
            auto listener = required(env, env->NewObject(listenerClass,
                method(env, listenerClass, "<init>", "(J)V"), token), "construct focus listener");
            auto builderClass = findClass(env, "android/media/AudioFocusRequest$Builder");
            auto builder = required(env, env->NewObject(builderClass,
                method(env, builderClass, "<init>", "(I)V"), static_cast<jint>(options.requestedGain)), "AudioFocusRequest.Builder");
            env->CallObjectMethod(builder, method(env, builderClass, "setAudioAttributes",
                "(Landroid/media/AudioAttributes;)Landroid/media/AudioFocusRequest$Builder;"), builtAttributes);
            check(env, "setAudioAttributes");
            env->CallObjectMethod(builder, method(env, builderClass, "setAcceptsDelayedFocusGain",
                "(Z)Landroid/media/AudioFocusRequest$Builder;"), static_cast<jboolean>(options.acceptsDelayedGain));
            check(env, "setAcceptsDelayedFocusGain");
            env->CallObjectMethod(builder, method(env, builderClass, "setOnAudioFocusChangeListener",
                "(Landroid/media/AudioManager$OnAudioFocusChangeListener;)Landroid/media/AudioFocusRequest$Builder;"), listener);
            check(env, "setOnAudioFocusChangeListener");
            auto requestObject = required(env, env->CallObjectMethod(builder,
                method(env, builderClass, "build", "()Landroid/media/AudioFocusRequest;")), "build focus request");
            audioManager = required(env, env->NewGlobalRef(manager), "retain AudioManager");
            focusRequest = required(env, env->NewGlobalRef(requestObject), "retain AudioFocusRequest");
            auto managerClass = static_cast<jclass>(required(env, env->GetObjectClass(manager), "AudioManager class"));
            const auto requestMethod = method(env, managerClass, "requestAudioFocus", "(Landroid/media/AudioFocusRequest;)I");
            needsAbandon = true;
            const auto result = env->CallIntMethod(manager, requestMethod, requestObject);
            check(env, "requestAudioFocus");
            needsAbandon = result != 0; // Denied requests own no focus; delayed grants must be cancelled.
            return {static_cast<int>(result), {}};
        }
        catch (const NativeFailure& error) { return {{}, error.error}; }
    }
    AndroidFocusOperationResult release() noexcept override
    {
        forgetCallback(token);
        token = 0;
        if (!audioManager && !focusRequest) return {};
        try
        {
            Environment environment(vm);
            auto* env = environment.get();
            std::optional<int> result;
            if (needsAbandon)
            {
                auto cls = static_cast<jclass>(required(env, env->GetObjectClass(audioManager), "AudioManager class"));
                const auto nativeResult = env->CallIntMethod(audioManager,
                    method(env, cls, "abandonAudioFocusRequest", "(Landroid/media/AudioFocusRequest;)I"), focusRequest);
                check(env, "abandonAudioFocusRequest");
                result = static_cast<int>(nativeResult);
                if (nativeResult != 1)
                    return {result, SessionError{"abandonAudioFocusRequest", "Android.AudioManager", nativeResult, "focus abandon denied"}};
            }
            clearRequest(env);
            return {result, {}};
        }
        catch (const NativeFailure& error) { return {{}, error.error}; }
        catch (...) { return {{}, SessionError{"release", "JNI", 0, "unexpected native cleanup failure"}}; }
    }
private:
    void clearRequest(JNIEnv* env) noexcept
    {
        if (focusRequest) env->DeleteGlobalRef(focusRequest);
        if (audioManager) env->DeleteGlobalRef(audioManager);
        focusRequest = nullptr;
        audioManager = nullptr;
        needsAbandon = false;
    }
    JavaVM* vm{nullptr};
    jobject context{nullptr};
    jclass listenerClass{nullptr};
    jobject audioManager{nullptr};
    jobject focusRequest{nullptr};
    jlong token{0};
    bool needsAbandon{false};
    std::optional<SessionError> initializationError;
};
}
std::unique_ptr<AudioSession> createAndroidAudioSession(
    JNIEnv* env, jobject applicationContext, jclass listenerClass)
{
    return makeAndroidAudioSession(
        std::make_unique<JniAudioFocusDriver>(env, applicationContext, listenerClass));
}
std::unique_ptr<AudioSession> createAndroidAudioSession(
    JNIEnv* env, jobject applicationContext, jclass listenerClass,
    jclass foregroundServiceClass, jclass wakeLockClass)
{
    return makeAndroidAudioSession(
        std::make_unique<JniAudioFocusDriver>(env, applicationContext, listenerClass),
        makeAndroidSessionResourceDriver(env, applicationContext, foregroundServiceClass, wakeLockClass));
}
} // namespace gigante
