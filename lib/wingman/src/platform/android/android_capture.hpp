#pragma once

// Android MediaProjection 采集源（A2，docs/android-agent-design.md §5.6）。
//
// 将 AndroidHostBridge 的 captureFrame 包装为通用 ICaptureSource，
// 供 wingman.screen / wingman.vision 的脚本 API 与 screenshot.capture
// 命令消费（ImageAnalyzer 直接吃本源产出的 Bitmap）。
// 投屏未授权/无宿主桥时 isAvailable()=false，capture() 返回 nullptr。

#include "platform/android/android_host_bridge.hpp"
#include "wingman/capture/capture_source.hpp"

namespace wingman::platform::android {

class AndroidCaptureSource : public capture::ICaptureSource {
public:
    // bridge 为空（默认取 globalHostBridge()）时各项能力降级不可用
    explicit AndroidCaptureSource(AndroidHostBridge* bridge = nullptr);

    std::unique_ptr<Bitmap> capture(const Rect& region = {}) override;
    Rect getBounds() const override;
    bool isAvailable() const override;
    std::string getName() const override;

private:
    AndroidHostBridge* resolveBridge() const;

    AndroidHostBridge* bridge_;  // 空 = 每次调用取全局桥
};

} // namespace wingman::platform::android
