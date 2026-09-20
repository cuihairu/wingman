// JNI 边界层（A1，docs/android-agent-design.md §5.3）。
//
// 整个 Android 工程中唯一接触 JNIEnv 的翻译文件：Kotlin 前台服务经
// WingmanJni.nativeStart/nativeStop/nativeStatus 驱动 C++ 核心（AndroidAgent），
// 核心是纯 C++，不反向调用任何 Android API。
//
// 配置经 JSON 字符串传入（Kotlin 侧组装，nlohmann 解析）：
//   {"serverIp":"10.0.0.2","serverPort":8888,
//    "agentId":"android-pixel-8","hostname":"Pixel 8",
//    "capabilitiesJson":"{\"apiLevel\":34}","authToken":"..."}

#include <jni.h>

#include <android/log.h>

#include <nlohmann/json.hpp>

#include <memory>
#include <string>

#include "agent/android_agent.hpp"

namespace {

using wingman::android::AndroidAgent;

constexpr const char* kLogTag = "WingmanAgent";

// 单进程单 agent 实例：生命周期由 Kotlin 前台服务托管（§5.4）
std::unique_ptr<AndroidAgent>& agentInstance() {
    static std::unique_ptr<AndroidAgent> instance;
    return instance;
}

std::string toStdString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

jstring toJString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_wingman_agent_WingmanJni_nativeStart(JNIEnv* env, jclass /*clazz*/,
                                              jstring configJson) {
    const std::string configText = toStdString(env, configJson);
    nlohmann::json parsed = nlohmann::json::object();
    try {
        if (!configText.empty()) {
            parsed = nlohmann::json::parse(configText);
        }
    } catch (...) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "invalid config json");
        return JNI_FALSE;
    }

    AndroidAgent::Config config;
    config.serverIp = parsed.value("serverIp", config.serverIp);
    config.serverPort = parsed.value("serverPort", config.serverPort);
    config.agentId = parsed.value("agentId", "");
    config.hostname = parsed.value("hostname", "");
    config.platform = parsed.value("platform", "android");
    config.capabilitiesJson = parsed.value("capabilitiesJson", "");
    // 注册鉴权 token（可空；server 侧 token 白名单开启时必填，
    // docs/agent-token-auth-design.md §4.2）
    config.authToken = parsed.value("authToken", "");

    auto& agent = agentInstance();
    if (!agent) {
        agent = std::make_unique<AndroidAgent>();
    }
    const bool started = agent->start(config);
    __android_log_print(started ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        kLogTag, "nativeStart: %d", started ? 1 : 0);
    return started ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_wingman_agent_WingmanJni_nativeStop(JNIEnv* /*env*/, jclass /*clazz*/) {
    if (auto& agent = agentInstance(); agent) {
        agent->stop();
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "nativeStop");
    }
}

JNIEXPORT jstring JNICALL
Java_com_wingman_agent_WingmanJni_nativeStatus(JNIEnv* env, jclass /*clazz*/) {
    const auto& agent = agentInstance();
    if (!agent) {
        return toJString(env,
                         R"({"running":false,"connected":false,"script":{"running":false}})");
    }
    return toJString(env, agent->statusJson());
}

} // extern "C"
