#include "agent/android_screenshot.hpp"

#include "agent/base64.hpp"
#include "platform/android/android_host_bridge.hpp"
#include "wingman/screen.hpp"

#include <nlohmann/json.hpp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace wingman::android {

namespace {

using platform::android::AndroidHostBridge;
using runtime::CommandData;
using runtime::CommandResult;

// 与桌面 screenshot_handler 一致的上限（4K 面 / 像素总量）
constexpr int kMaxCaptureWidth = 3840;
constexpr int kMaxCaptureHeight = 2160;
constexpr int kMaxCapturePixels = 3840 * 2160;

uint64_t nowMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

// region 以 JSON 字符串到达（RemoteClient 将命令 JSON 扁平化为
// CommandData，非字符串值经 value.dump()），容错解析；
// 缺省 = 全屏（空 Rect）
Rect parseRegion(const CommandData& data) {
    const auto it = data.find("region");
    if (it == data.end() || it->second.empty()) {
        return Rect();
    }
    try {
        const nlohmann::json region = nlohmann::json::parse(it->second);
        return Rect(region.value("x", 0), region.value("y", 0),
                    region.value("width", 0), region.value("height", 0));
    } catch (...) {
        return Rect();
    }
}

Rect clampRegion(const Rect& region, const Bitmap& frame) {
    if (region.isEmpty()) {
        return Rect(0, 0, frame.getWidth(), frame.getHeight());
    }
    Rect clamped;
    clamped.x = std::max(0, std::min(region.x, frame.getWidth() - 1));
    clamped.y = std::max(0, std::min(region.y, frame.getHeight() - 1));
    clamped.width = std::min(region.width, frame.getWidth() - clamped.x);
    clamped.height = std::min(region.height, frame.getHeight() - clamped.y);
    // 与桌面同款总量上限（越界裁剪）
    clamped.width = std::min(clamped.width, kMaxCaptureWidth);
    clamped.height = std::min(clamped.height, kMaxCaptureHeight);
    if (clamped.width * clamped.height > kMaxCapturePixels) {
        clamped.width = kMaxCaptureWidth;
        clamped.height = kMaxCaptureHeight;
    }
    return clamped;
}

} // namespace

runtime::CommandResult handleScreenshotCapture(const CommandData& data,
                                               AndroidHostBridge* bridge) {
    (void)data;  // region 下方解析；displayId 接受即忽略（单一虚拟显示）
    if (!bridge) {
        return CommandResult::error("screenshot capture requires the input bridge");
    }
    const auto frame = bridge->captureFrame();
    if (!frame) {
        return CommandResult::error(
            "Failed to capture screen (media projection not authorized or no frame yet)");
    }

    const Rect region = clampRegion(parseRegion(data), *frame);
    if (region.isEmpty()) {
        return CommandResult::error("Screenshot region out of bounds");
    }

    // BGRA 帧包一层 Mat（零拷贝），转 BGR 后 JPEG 编码（对齐桌面 q82）
    cv::Mat bgraMat(region.height, region.width, CV_8UC4,
                    frame->getData() + (static_cast<size_t>(region.y) * frame.getWidth() +
                                        region.x) * 4,
                    static_cast<size_t>(frame.getWidth()) * 4);
    cv::Mat bgrMat;
    cv::cvtColor(bgraMat, bgrMat, cv::COLOR_BGRA2BGR);

    std::vector<uchar> jpegBuffer;
    const std::vector<int> encodeParams = {cv::IMWRITE_JPEG_QUALITY, 82};
    if (!cv::imencode(".jpg", bgrMat, jpegBuffer, encodeParams)) {
        return CommandResult::error("Failed to encode screenshot");
    }

    const std::vector<uint8_t> bytes(jpegBuffer.begin(), jpegBuffer.end());
    const std::string image =
        "data:image/jpeg;base64," + base64Encode(bytes);

    nlohmann::json result = {
        {"image", image},
        {"width", region.width},
        {"height", region.height},
        {"timestamp", nowMillis()},
        {"region", {
            {"x", region.x},
            {"y", region.y},
            {"width", region.width},
            {"height", region.height}
        }}
    };
    return CommandResult::okData(result.dump());
}

} // namespace wingman::android
