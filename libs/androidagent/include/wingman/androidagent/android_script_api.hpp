#pragma once

// Android 脚本能力 API（A2，docs/android-agent-design.md §5.6）。
//
// 把宿主桥（手势注入 + MediaProjection 采集）包装为与桌面同名同形的
// wingman.input / wingman.screen / wingman.vision Lua API：
//   - 找色找图走 lib/wingman 的 ImageAnalyzer（bitmap-first，帧取一次
//     分析一次，不在每次查找时重复截屏——与桌面 Vision 每调用重新截屏
//     的结构性差异，见设计文档）；
//   - 桥为 null（无障碍/投屏未授权或宿主未注册）时函数降级返回
//     false/nil，不抛错（防御性，脚本可用返回值自行兜底）。
//
// 纯 C++：只依赖 sol2 + AndroidHostBridge 抽象 + ImageAnalyzer，
// 不触碰 JNI（翻译在 jni_bridge.cpp），桌面 lua_tests 用 Fake 桥直测。

#include <atomic>
#include <string>

#include <sol/sol.hpp>

#include "platform/android/android_host_bridge.hpp"

namespace wingman::android {

// 在 lua["wingman"] 下挂 input/screen/vision 三张子表。
// stopFlag 用于 input.delay 的分片睡眠中断；bridge 可为 null。
void registerAndroidApis(sol::state& lua, std::atomic<bool>& stopFlag,
                         platform::android::AndroidHostBridge* bridge,
                         const std::string& filesDir);

} // namespace wingman::android
