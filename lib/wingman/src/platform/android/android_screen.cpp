// Screen::capture 的 Android 装配（仅 NDK 构建编，androidagent CMake
// if(ANDROID) 条件收口）。公共层 screen.cpp 的平台分区不含 __ANDROID__
// 段（边界守卫冻结不新增），而 AI provider（vision_ai.cpp）默认截帧路径
// 引用 Screen::capture 符号——NDK 链接面由此租户 TU 反向 include 公共头
// 提供（§5.5 租户方向：platform/* 可依赖 wingman 公共头，反向不成立）。
// registerAndroidApis 通常已注入 captureFrame provider，本装配是未注入
// 时的兜底链路（global bridge 缺失时返回 nullptr，走 provider 失败语义）。

#include "wingman/screen.hpp"

#include "platform/android/android_capture.hpp"

namespace wingman {

std::unique_ptr<Bitmap> Screen::capture() {
    platform::android::AndroidCaptureSource source;
    return source.capture(Rect());  // 空 region=整帧
}

std::unique_ptr<Bitmap> Screen::capture(const Rect& region) {
    platform::android::AndroidCaptureSource source;
    return source.capture(region);
}

} // namespace wingman
