// JNI 边界层（A1 建立反向扩展于 A2，docs/android-agent-design.md §5.3/§5.6）。
//
// 整个 Android 工程中唯一接触 JNIEnv 的翻译文件：
//   - Kotlin 前台服务经 WingmanJni.nativeStart/nativeStop/nativeStatus
//     驱动 C++ 核心（AndroidAgent）；
//   - A2 反向桥：platform/android 的 AndroidHostBridge 抽象由本文件的
//     JniHostBridge 实现（手势经无障碍服务 performGesture、屏幕帧经
//     ImageReader 推送的缓存读取），注册进全局 setter 供脚本 API 与
//     screenshot.capture 命令消费。
//
// 线程规则：native 方法由 JVM 已 attach 的线程调用；C++ 脚本线程经
// JniEnvGuard 按需 attach/detach（thread_local RAII）。不使用 FindClass
// （非主线程 FindClass 应用类走 system classloader 会失败）：Kotlin 在
// onServiceConnected 传入 jobject，此处 GetObjectClass + GlobalRef +
// 缓存 jmethodID。
//
// 配置经 JSON 字符串传入（Kotlin 侧组装，nlohmann 解析）：
//   {"serverIp":"10.0.0.2","serverPort":8888,
//    "agentId":"android-pixel-8","hostname":"Pixel 8",
//    "capabilitiesJson":"{\"apiLevel\":34}","authToken":"...",
//    "filesDir":"/sdcard/Android/data/com.wingman.agent/files"}

#include <jni.h>

#include <android/log.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "agent/android_agent.hpp"
#include "platform/android/android_host_bridge.hpp"

namespace {

using wingman::android::AndroidAgent;
using wingman::platform::android::AndroidHostBridge;

constexpr const char* kLogTag = "WingmanAgent";

// 进程内唯一 JavaVM（JNI_OnLoad 存入）
JavaVM* g_javaVm = nullptr;

// 脚本线程按需 attach 的 RAII 守卫：GetEnv 返回 EDETACHED 才 attach，
// 析构时仅当本守卫 attach 过才 detach（JVM 线程/handler 线程不受影响）
class JniEnvGuard {
public:
    explicit JniEnvGuard(JavaVM* vm) : vm_(vm) {
        if (!vm_) {
            return;
        }
        void* envPtr = nullptr;
        const jint state = vm_->GetEnv(&envPtr, JNI_VERSION_1_6);
        if (state == JNI_OK) {
            env_ = static_cast<JNIEnv*>(envPtr);
        } else if (state == JNI_EDETACHED) {
            if (vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
                owns_ = true;
            }
        }
    }
    ~JniEnvGuard() {
        if (owns_ && vm_) {
            vm_->DetachCurrentThread();
        }
    }
    JniEnvGuard(const JniEnvGuard&) = delete;
    JniEnvGuard& operator=(const JniEnvGuard&) = delete;

    JNIEnv* env() const { return env_; }

private:
    JavaVM* vm_ = nullptr;
    JNIEnv* env_ = nullptr;
    bool owns_ = false;
};

// ---------- 帧缓存（生产者：Java handler 线程；消费者：脚本线程） ----------

struct FrameCache {
    std::mutex mutex;
    std::vector<uint8_t> rgba;  // 原始 RGBA（含 rowStride 行距）
    int width = 0;
    int height = 0;
    int rowStride = 0;
    std::atomic<int> screenW{0};
    std::atomic<int> screenH{0};
};

FrameCache& frameCache() {
    static FrameCache cache;
    return cache;
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

// ---------- JniHostBridge：AndroidHostBridge 的 JNI 实现 ----------

class JniHostBridge : public AndroidHostBridge {
public:
    // ---- 输入桥生命周期（nativeSetInputBridge，主线程调用） ----

    void attachService(JNIEnv* env, jobject service) {
        std::lock_guard<std::mutex> lock(serviceMutex_);
        releaseServiceLocked(env);
        if (service == nullptr) {
            return;
        }
        serviceRef_ = env->NewGlobalRef(service);
        jclass clazz = env->GetObjectClass(service);
        // Kotlin: fun performGesture(x1,y1,x2,y2,durationMs: Int): Int
        performGestureMethod_ = env->GetMethodID(
            clazz, "performGesture", "(IIIII)I");
        env->DeleteLocalRef(clazz);
        if (!performGestureMethod_) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "performGesture method not found");
        }
    }

    // ---- 手势（同步阻塞直至完成/失败/超时，任意线程调用） ----

    bool dispatchGesture(int x1, int y1, int x2, int y2, int durationMs) {
        JNIEnv* rawEnv = nullptr;
        {
            JniEnvGuard guard(g_javaVm);
            rawEnv = guard.env();
            if (!rawEnv) {
                return false;
            }
            // 注册 seq → promise，随后在持锁外等待（避免阻塞回调线程取锁）
            const int seq = nextSeq_.fetch_add(1);
            auto promise = std::make_shared<std::promise<bool>>();
            auto future = promise->get_future();
            {
                std::lock_guard<std::mutex> lock(pendingMutex_);
                pending_.emplace(seq, std::move(promise));
            }

            bool dispatched = false;
            {
                std::lock_guard<std::mutex> lock(serviceMutex_);
                if (serviceRef_ && performGestureMethod_) {
                    const jint rc = rawEnv->CallIntMethod(
                        serviceRef_, performGestureMethod_,
                        static_cast<jint>(x1), static_cast<jint>(y1),
                        static_cast<jint>(x2), static_cast<jint>(y2),
                        static_cast<jint>(durationMs));
                    if (rawEnv->ExceptionCheck()) {
                        rawEnv->ExceptionDescribe();
                        rawEnv->ExceptionClear();
                    } else {
                        dispatched = rc >= 0;
                    }
                }
            }
            if (!dispatched) {
                std::lock_guard<std::mutex> lock(pendingMutex_);
                pending_.erase(seq);
                return false;
            }
            // 超时余量 2s：覆盖主线程调度与手势收尾
            const auto timeout = std::chrono::milliseconds(durationMs) +
                                 std::chrono::milliseconds(2000);
            const bool ok = future.wait_for(timeout) == std::future_status::ready &&
                            future.get();
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pending_.erase(seq);  // 超时残留清理（回调晚到为 no-op）
            return ok;
        }
    }

    bool tap(int x, int y, int durationMs) override {
        return dispatchGesture(x, y, x, y, durationMs);
    }

    bool swipe(int x1, int y1, int x2, int y2, int durationMs) override {
        return dispatchGesture(x1, y1, x2, y2, durationMs);
    }

    bool longPress(int x, int y, int durationMs) override {
        return dispatchGesture(x, y, x, y, durationMs);
    }

    void onGestureResult(int seq, bool completed) {
        std::shared_ptr<std::promise<bool>> promise;
        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            const auto it = pending_.find(seq);
            if (it != pending_.end()) {
                promise = it->second;
            }
        }
        if (promise) {
            promise->set_value(completed);
        }
    }

    // ---- 采集（读 C++ 侧缓存，无 JNI 调用，任意线程安全） ----

    std::unique_ptr<wingman::Bitmap> captureFrame() override {
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
        int rowStride = 0;
        {
            auto& cache = frameCache();
            std::lock_guard<std::mutex> lock(cache.mutex);
            if (cache.rgba.empty() || cache.width <= 0 || cache.height <= 0) {
                return nullptr;
            }
            rgba = cache.rgba;
            width = cache.width;
            height = cache.height;
            rowStride = cache.rowStride;
        }
        // RGBA → BGRA（Bitmap 像素序）+ rowStride 逐行拷贝
        auto frame = std::make_unique<wingman::Bitmap>(width, height);
        auto* data = frame->getData();
        for (int row = 0; row < height; ++row) {
            const uint8_t* src = rgba.data() + static_cast<size_t>(row) * rowStride;
            uint8_t* dst = data + static_cast<size_t>(row) * width * 4;
            for (int col = 0; col < width; ++col) {
                dst[col * 4 + 0] = src[col * 4 + 2];  // B
                dst[col * 4 + 1] = src[col * 4 + 1];  // G
                dst[col * 4 + 2] = src[col * 4 + 0];  // R
                dst[col * 4 + 3] = src[col * 4 + 3];  // A
            }
        }
        return frame;
    }

    bool screenSize(int& width, int& height) override {
        width = frameCache().screenW.load(std::memory_order_acquire);
        height = frameCache().screenH.load(std::memory_order_acquire);
        return width > 0 && height > 0;
    }

private:
    void releaseServiceLocked(JNIEnv* env) {
        if (serviceRef_) {
            env->DeleteGlobalRef(serviceRef_);
            serviceRef_ = nullptr;
        }
        performGestureMethod_ = nullptr;
    }

    std::mutex serviceMutex_;
    jobject serviceRef_ = nullptr;
    jmethodID performGestureMethod_ = nullptr;

    std::atomic<int> nextSeq_{1};
    std::mutex pendingMutex_;
    std::unordered_map<int, std::shared_ptr<std::promise<bool>>> pending_;
};

JniHostBridge& hostBridge() {
    static JniHostBridge bridge;
    return bridge;
}

// AndroidAgent 进程级单例（nativeStart/nativeStop/nativeStatus 共享同一实例）
std::unique_ptr<AndroidAgent>& agentInstance() {
    static std::unique_ptr<AndroidAgent> instance;
    return instance;
}

} // namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    g_javaVm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* /*vm*/, void* /*reserved*/) {
    g_javaVm = nullptr;
}

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
    // 模板图根目录（A2：findImage 相对路径解析根）
    config.filesDir = parsed.value("filesDir", "");

    // 注入宿主桥（A2）：生命周期为本进程单例，服务未连接时各项能力
    // 返回 false/空帧（脚本 API 降级路径）
    wingman::platform::android::setGlobalHostBridge(&hostBridge());

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
    wingman::platform::android::setGlobalHostBridge(nullptr);
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

// ---------- A2 反向桥入口（Kotlin → C++） ----------

// 无障碍服务连接/断开（主线程）：object 方法见 JniHostBridge::attachService
JNIEXPORT void JNICALL
Java_com_wingman_agent_WingmanJni_nativeSetInputBridge(JNIEnv* env, jclass /*clazz*/,
                                                       jobject service) {
    hostBridge().attachService(env, service);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "input bridge %s",
                        service ? "attached" : "released");
}

// 手势完成回调（主线程 Handler）：seq 对应 dispatchGesture 的注册项
JNIEXPORT void JNICALL
Java_com_wingman_agent_WingmanJni_nativeOnGestureResult(JNIEnv* /*env*/, jclass /*clazz*/,
                                                        jint seq, jboolean completed) {
    hostBridge().onGestureResult(static_cast<int>(seq), completed == JNI_TRUE);
}

// 屏幕帧推送（ImageReader handler 线程）：拷入缓存后立即返回，
// Image 生命周期归 Kotlin（调用前已 close）
JNIEXPORT void JNICALL
Java_com_wingman_agent_WingmanJni_nativeOnFrame(JNIEnv* env, jclass /*clazz*/,
                                                jbyteArray rgba, jint width, jint height,
                                                jint rowStride) {
    if (!rgba || width <= 0 || height <= 0 || rowStride <= 0) {
        return;
    }
    const jsize length = env->GetArrayLength(rgba);
    if (static_cast<jsize>(rowStride * height) > length) {
        return;  // Kotlin 侧契约破坏，防御性忽略
    }
    auto& cache = frameCache();
    std::vector<uint8_t> buffer(static_cast<size_t>(rowStride * height));
    env->GetByteArrayRegion(rgba, 0, length,
                            reinterpret_cast<jbyte*>(buffer.data()));
    std::lock_guard<std::mutex> lock(cache.mutex);
    cache.rgba = std::move(buffer);
    cache.width = width;
    cache.height = height;
    cache.rowStride = rowStride;
    cache.screenW.store(width, std::memory_order_release);
    cache.screenH.store(height, std::memory_order_release);
}

} // extern "C"
