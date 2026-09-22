#pragma once

// screenshot.capture 命令处理（A2，NDK-only）。
//
// 返回与桌面 screenshot_handler 相同的 JSON shape（JPEG q82 + base64
// data URI），Go server workflow 的 screenshot 步骤（executeScreenshotStep
// → screenshotPayload）零改动消费。

#include <string>

#include "platform/android/android_host_bridge.hpp"
#include "wingman/agentcore/remote_client.hpp"

namespace wingman::android {

// data：server 下发的命令参数（CommandData 扁平化，region 为 JSON 字符串）
// bridge：宿主桥（null/投屏未授权 → CommandResult::error）
runtime::CommandResult handleScreenshotCapture(
    const runtime::CommandData& data,
    platform::android::AndroidHostBridge* bridge);

} // namespace wingman::android
