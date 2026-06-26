#include "wingman/platform/icapture.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <vector>

namespace wingman::platform::mac {

namespace {

bool isValidMonitorIndex(int monitorIndex, CGDisplayCount displayCount) {
    return monitorIndex >= 0 &&
           static_cast<CGDisplayCount>(monitorIndex) < displayCount;
}

std::vector<CGDirectDisplayID> getDisplays() {
    CGDisplayCount displayCount = 0;
    CGGetOnlineDisplayList(0, nullptr, &displayCount);

    std::vector<CGDirectDisplayID> displays(displayCount);
    if (displayCount > 0) {
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);
        displays.resize(displayCount);
    }
    return displays;
}

} // namespace

class SCCapture : public ICapture {
public:
    SCCapture() = default;
    ~SCCapture() override { shutdown(); }

    bool initialize(const CaptureConfig& config) override {
        if (@available(macOS 12.3, *)) {
            config_ = config;
            initialized_ = true;
            return true;
        }

        spdlog::warn("[SCCapture] ScreenCaptureKit requires macOS 12.3+");
        return false;
    }

    void shutdown() override {
        initialized_ = false;
    }

    std::unique_ptr<Bitmap> captureScreen(int monitorIndex) override {
        if (!initialized_) {
            return nullptr;
        }

        return captureRegion(getMonitorBounds(monitorIndex));
    }

    std::unique_ptr<Bitmap> captureRegion(const Rect& region) override {
        if (!initialized_ || region.isEmpty()) {
            return nullptr;
        }

        // Reuse the stable macOS screen fallback already wired through Screen::capture.
        return Screen::capture(wingman::Rect(region.x, region.y, region.width, region.height));
    }

    std::unique_ptr<Bitmap> captureRegion(int /*monitorIndex*/, const Rect& region) override {
        return captureRegion(region);
    }

    std::unique_ptr<Bitmap> captureWindow(WindowHandle /*hwnd*/) override {
        // ScreenCaptureKit window capture is not wired into the current ICapture surface yet.
        return nullptr;
    }

    std::unique_ptr<Bitmap> captureWindowRegion(WindowHandle /*hwnd*/, const Rect& /*region*/) override {
        return nullptr;
    }

    int getMonitorCount() override {
        CGDisplayCount count = 0;
        CGGetOnlineDisplayList(0, nullptr, &count);
        return static_cast<int>(count);
    }

    Rect getMonitorBounds(int monitorIndex) override {
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return Rect{};
        }

        const CGRect rect = CGDisplayBounds(displays[static_cast<size_t>(monitorIndex)]);
        return Rect{
            static_cast<int>(rect.origin.x),
            static_cast<int>(rect.origin.y),
            static_cast<int>(rect.size.width),
            static_cast<int>(rect.size.height)
        };
    }

    std::string getMonitorName(int monitorIndex) override {
        return "Display " + std::to_string(monitorIndex);
    }

    bool isAvailable() const override {
        return initialized_;
    }

    std::string getBackendName() const override {
        return "ScreenCaptureKit";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "ScreenCaptureKit",
            "1.0",
            initialized_,
            "macOS ScreenCaptureKit placeholder backed by Screen::capture for region grabs"
        };
    }

private:
    CaptureConfig config_{};
    bool initialized_ = false;
};

} // namespace wingman::platform::mac

#endif // __APPLE__
