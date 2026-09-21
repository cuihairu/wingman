#include "platform/android/android_capture.hpp"

#include <algorithm>

namespace wingman::platform::android {

AndroidCaptureSource::AndroidCaptureSource(AndroidHostBridge* bridge)
    : bridge_(bridge) {}

AndroidHostBridge* AndroidCaptureSource::resolveBridge() const {
    return bridge_ ? bridge_ : globalHostBridge();
}

std::unique_ptr<Bitmap> AndroidCaptureSource::capture(const Rect& region) {
    auto* bridge = resolveBridge();
    if (!bridge) {
        return nullptr;
    }
    auto frame = bridge->captureFrame();
    if (!frame) {
        return nullptr;
    }
    if (region.isEmpty()) {
        return frame;
    }
    // region 裁剪（与桌面 Screen::capture(region) 语义一致：越界部分截断）
    const int x = std::max(0, region.x);
    const int y = std::max(0, region.y);
    const int width = std::min(region.width, frame->getWidth() - x);
    const int height = std::min(region.height, frame->getHeight() - y);
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    auto cropped = std::make_unique<Bitmap>(width, height);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            cropped->setPixel(col, row, frame->getPixel(x + col, y + row));
        }
    }
    return cropped;
}

Rect AndroidCaptureSource::getBounds() const {
    auto* bridge = resolveBridge();
    int width = 0;
    int height = 0;
    if (bridge && bridge->screenSize(width, height)) {
        return Rect(0, 0, width, height);
    }
    return Rect(0, 0, 0, 0);
}

bool AndroidCaptureSource::isAvailable() const {
    auto* bridge = resolveBridge();
    if (!bridge) {
        return false;
    }
    int width = 0;
    int height = 0;
    return bridge->screenSize(width, height) && width > 0 && height > 0;
}

std::string AndroidCaptureSource::getName() const {
    return "android-media-projection";
}

} // namespace wingman::platform::android
