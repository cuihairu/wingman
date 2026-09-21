#include "platform/android/android_host_bridge.hpp"

#include <atomic>

namespace wingman::platform::android {

namespace {
// 与 macro_module 的全局录制器同款：atomic 裸指针观察，所有权在注入方
std::atomic<AndroidHostBridge*> g_bridge{nullptr};
} // namespace

void setGlobalHostBridge(AndroidHostBridge* bridge) {
    g_bridge.store(bridge, std::memory_order_release);
}

AndroidHostBridge* globalHostBridge() {
    return g_bridge.load(std::memory_order_acquire);
}

} // namespace wingman::platform::android
