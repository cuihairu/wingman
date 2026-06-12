#include "wingman/runtime/rpc/screenshot_handler.hpp"

#include "wingman/crypt.hpp"
#include "wingman/screen.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

#if defined(WINGMAN_ENABLE_VISION)
#include <opencv2/opencv.hpp>
#endif

namespace wingman::rpc {

namespace {

constexpr int kMaxCaptureWidth = 3840;
constexpr int kMaxCaptureHeight = 2160;
constexpr int kMaxCapturePixels = 3840 * 2160;

Rect requestedRegion(const nlohmann::json& params) {
    const auto bounds = Screen::getScreenBounds();
    const auto region = params.value("region", nlohmann::json::object());

    Rect result(
        region.value("x", bounds.x),
        region.value("y", bounds.y),
        region.value("width", bounds.width),
        region.value("height", bounds.height)
    );

    if (result.isEmpty()) {
        result = bounds;
    }

    if (result.width > kMaxCaptureWidth) {
        result.width = kMaxCaptureWidth;
    }
    if (result.height > kMaxCaptureHeight) {
        result.height = kMaxCaptureHeight;
    }
    if (result.width * result.height > kMaxCapturePixels) {
        result.height = std::max(1, kMaxCapturePixels / std::max(1, result.width));
    }

    return result;
}

uint64_t nowMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // namespace

void registerScreenshotHandlers(RpcDispatcher& dispatcher) {
    using json = nlohmann::json;

    dispatcher.registerHandler("screenshot.capture", [](const json& params) -> json {
#if !defined(WINGMAN_ENABLE_VISION)
        (void)params;
        return {
            {"success", false},
            {"error", "Screenshot capture requires WINGMAN_ENABLE_VISION/OpenCV"}
        };
#else
        const Rect region = requestedRegion(params);
        auto bitmap = Screen::capture(region);
        if (!bitmap) {
            return {
                {"success", false},
                {"error", "Failed to capture screen"}
            };
        }

        const int width = bitmap->getWidth();
        const int height = bitmap->getHeight();
        cv::Mat bgraMat(height, width, CV_8UC4, bitmap->getData());
        cv::Mat bgrMat;
        cv::cvtColor(bgraMat, bgrMat, cv::COLOR_BGRA2BGR);

        std::vector<uchar> jpegBuffer;
        std::vector<int> encodeParams = {cv::IMWRITE_JPEG_QUALITY, 82};
        if (!cv::imencode(".jpg", bgrMat, jpegBuffer, encodeParams)) {
            return {
                {"success", false},
                {"error", "Failed to encode screenshot"}
            };
        }

        const std::vector<uint8_t> bytes(jpegBuffer.begin(), jpegBuffer.end());
        const std::string image = "data:image/jpeg;base64," + crypt::base64Encode(bytes);

        return {
            {"image", image},
            {"width", width},
            {"height", height},
            {"timestamp", nowMillis()},
            {"region", {
                {"x", region.x},
                {"y", region.y},
                {"width", region.width},
                {"height", region.height}
            }}
        };
#endif
    });
}

} // namespace wingman::rpc
