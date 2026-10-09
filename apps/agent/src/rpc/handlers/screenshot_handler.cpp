#include "wingman/runtime/rpc/screenshot_handler.hpp"

#include "wingman/crypt.hpp"
#include "wingman/platform/iscreen.hpp"
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

uint64_t nowMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// Resolve the capture region against the legacy primary-monitor bounds.
// Kept for the no-IScreen overload and as the displayId-less fallback.
Rect requestedRegionPrimary(const nlohmann::json& params) {
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

// Resolve the capture region for a specific displayId via IScreen.
// If the caller also provides an explicit region, it is interpreted as
// relative to that monitor's origin.
Rect requestedRegionForDisplay(const nlohmann::json& params,
                                wingman::platform::IScreen& screen,
                                int displayId) {
    const auto monitorBounds = screen.getMonitorBounds(displayId);
    const auto region = params.value("region", nlohmann::json::object());

    const bool hasRegion = region.is_object() &&
        (region.contains("x") || region.contains("y") ||
         region.contains("width") || region.contains("height"));

    Rect result(
        region.value("x", monitorBounds.x),
        region.value("y", monitorBounds.y),
        region.value("width", monitorBounds.width),
        region.value("height", monitorBounds.height)
    );

    if (result.isEmpty() || !hasRegion) {
        // No explicit region: capture the entire monitor.
        result = Rect(monitorBounds.x, monitorBounds.y,
                      monitorBounds.width, monitorBounds.height);
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

nlohmann::json encodeScreenshot(const Rect& region) {
#if !defined(WINGMAN_ENABLE_VISION)
    (void)region;
    return {
        {"success", false},
        {"error", "Screenshot capture requires WINGMAN_ENABLE_VISION/OpenCV"}
    };
#else
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
}

nlohmann::json rectToJson(const wingman::platform::Rect& r) {
    return {
        {"x", r.x},
        {"y", r.y},
        {"width", r.width},
        {"height", r.height}
    };
}

void registerListMonitors(RpcDispatcher& dispatcher,
                          wingman::platform::IScreen& screen) {
    dispatcher.registerHandler("screen.listMonitors", [&screen](const nlohmann::json& /*params*/) -> nlohmann::json {
        const int count = screen.getMonitorCount();
        const int primary = screen.getPrimaryMonitorIndex();
        nlohmann::json::array_t monitors;
        for (int i = 0; i < count; ++i) {
            monitors.push_back({
                {"id", i},
                {"name", screen.getMonitorName(i)},
                {"isPrimary", i == primary},
                {"bounds", rectToJson(screen.getMonitorBounds(i))}
            });
        }
        return {
            {"monitors", std::move(monitors)},
            {"primaryId", primary}
        };
    });
}

} // namespace

void registerScreenshotHandlers(RpcDispatcher& dispatcher) {
    dispatcher.registerHandler("screenshot.capture", [](const nlohmann::json& params) -> nlohmann::json {
#if !defined(WINGMAN_ENABLE_VISION)
        (void)params;
        return {
            {"success", false},
            {"error", "Screenshot capture requires WINGMAN_ENABLE_VISION/OpenCV"}
        };
#else
        const Rect region = requestedRegionPrimary(params);
        return encodeScreenshot(region);
#endif
    });
}

void registerScreenshotHandlers(RpcDispatcher& dispatcher,
                                wingman::platform::IScreen& screen) {
    registerListMonitors(dispatcher, screen);

    dispatcher.registerHandler("screenshot.capture", [&screen](const nlohmann::json& params) -> nlohmann::json {
#if !defined(WINGMAN_ENABLE_VISION)
        (void)params; (void)screen;
        return {
            {"success", false},
            {"error", "Screenshot capture requires WINGMAN_ENABLE_VISION/OpenCV"}
        };
#else
        // Optional displayId targets a specific monitor. -1 / omitted → primary.
        const auto displayId = params.value("displayId", -1);
        Rect region = (displayId >= 0)
            ? requestedRegionForDisplay(params, screen, displayId)
            : requestedRegionPrimary(params);
        return encodeScreenshot(region);
#endif
    });
}

} // namespace wingman::rpc
