#pragma once

// Android 宿主桥（A2，docs/android-agent-design.md §5.6 反向 JNI 桥）。
//
// lib/wingman 不依赖任何 JNI/Android 头：C++ 核心（ScriptRunner 的
// wingman.input/screen/vision API、screenshot.capture 命令）只面向本抽象。
// 实现由 apps/android/cpp/jni_bridge.cpp 提供（全工程唯一 JNIEnv 翻译层），
// 在 native 启动前经 setGlobalHostBridge 注入（全局 setter 模式，与
// script/modules 的 setGlobalRecorder 同款先例）。
//
// 契约：所有方法同步阻塞直至完成/失败/超时，可从任意线程调用（实现方
// 负责 JNI attach）；宿主能力不可用时（无障碍未授权/投屏未授权）返回
// false/空帧，调用方据此降级，不抛异常。

#include "wingman/screen.hpp"

#include <memory>

namespace wingman::platform::android {

class AndroidHostBridge {
public:
    virtual ~AndroidHostBridge() = default;

    // 手势注入（AccessibilityService#dispatchGesture，主线程执行）
    virtual bool tap(int x, int y, int durationMs) = 0;
    virtual bool swipe(int x1, int y1, int x2, int y2, int durationMs) = 0;
    virtual bool longPress(int x, int y, int durationMs) = 0;

    // 屏幕采集（MediaProjection 最新帧，BGRA 像素序，与 Bitmap 布局一致）；
    // 无新帧/未授权时返回 nullptr
    virtual std::unique_ptr<Bitmap> captureFrame() = 0;

    // 虚拟显示尺寸（物理像素）；不可用时 w/h 置 0 并返回 false
    virtual bool screenSize(int& width, int& height) = 0;
};

// 全局桥注册（jni_bridge 在核心启动前注入；null = 清除）。
// 裸指针：所有权归 jni_bridge（GlobalRef 生命周期），此处仅观察。
void setGlobalHostBridge(AndroidHostBridge* bridge);
AndroidHostBridge* globalHostBridge();

} // namespace wingman::platform::android
