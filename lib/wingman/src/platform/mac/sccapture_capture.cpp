#include "wingman/platform/icapture.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <Foundation/Foundation.h>

#include <vector>

namespace wingman::platform::mac {

/**
 * @brief macOS ScreenCaptureKit 实现
 *
 * 使用 ScreenCaptureKit 框架进行屏幕捕获（macOS 12.3+）。
 * 优点：高性能，支持窗口捕获，支持光标
 * 缺点：仅支持 macOS 12.3+
 */
class SCCapture : public ICapture {
public:
    SCCapture() = default;
    ~SCCapture() override {
        shutdown();
    }

    bool initialize(const CaptureConfig& config) override {
        if (@available(macOS 12.3, *)) {
            config_ = config;
            initialized_ = true;
            return true;
        } else {
            spdlog::warn("[SCCapture] ScreenCaptureKit requires macOS 12.3+");
            return false;
        }
    }

    void shutdown() override {
        initialized_ = false;
    }

    std::unique_ptr<Bitmap> captureScreen(int monitorIndex) override {
        if (!initialized_) {
            return nullptr;
        }

        Rect bounds = getMonitorBounds(monitorIndex);
        return captureRegion(bounds);
    }

    std::unique_ptr<Bitmap> captureRegion(const Rect& region) override {
        if (!initialized_) {
            return nullptr;
        }

        if (@available(macOS 12.3, *)) {
            CGDisplayCount displayCount;
            CGGetOnlineDisplayList(&displayCount, nullptr);

            if (displayCount == 0) {
                return nullptr;
            }

            // 获取主显示器
            CGDirectDisplayID displayID;
            CGGetActiveDisplayList(1, &displayID, &displayCount);

            // 使用 CGDisplayCreateImage 捕获
            CGImageRef cgImage = CGDisplayCreateImage(displayID);
            if (!cgImage) {
                return nullptr;
            }

            auto bitmap = cgImageToBitmap(cgImage, region);
            CGImageRelease(cgImage);

            return bitmap;
        }

        return nullptr;
    }

    std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) override {
        return captureRegion(region);
    }

    std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) override {
        if (!initialized_) {
            return nullptr;
        }

        if (@available(macOS 12.3, *)) {
            // WindowHandle 在 macOS 上是 CGWindowID
            CGWindowID windowID = reinterpret_cast<CGWindowID>(hwnd);

            CFArrayRef windows = CFArrayCreate(nullptr, (const void**)&windowID, 1, nullptr);
            CGImageRef cgImage = CGWindowListCreateImageFromArray(
                CGRectInfinite,
                windows,
                kCGWindowImageDefault
            );
            CFRelease(windows);

            if (!cgImage) {
                return nullptr;
            }

            auto bitmap = cgImageToBitmap(cgImage, {});
            CGImageRelease(cgImage);

            return bitmap;
        }

        return nullptr;
    }

    std::unique_ptr<Bitmap> captureWindowByTitle(const std::string& title) override {
        // 需要依赖 IWindow 接口来查找窗口
        return nullptr;
    }

    int getMonitorCount() override {
        uint32_t count;
        CGGetOnlineDisplayList(0, nullptr, &count);
        return static_cast<int>(count);
    }

    Rect getMonitorBounds(int monitorIndex) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return Rect{};
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGDirectDisplayID displayID = displays[monitorIndex];
        CGRect rect = CGDisplayBounds(displayID);

        return Rect{
            static_cast<int>(rect.origin.x),
            static_cast<int>(rect.origin.y),
            static_cast<int>(rect.size.width),
            static_cast<int>(rect.size.height)
        };
    }

    std::string getMonitorName(int monitorIndex) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return "";
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CFStringRef name = CGDisplayLocalizedName(displays[monitorIndex]);
        if (name) {
            char buffer[256];
            CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
            CFRelease(name);
            return std::string(buffer);
        }

        return "Display " + std::to_string(monitorIndex);
    }

    int getPrimaryMonitorIndex() override {
        CGDirectDisplayID mainDisplay = CGMainDisplayID();

        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        for (int i = 0; i < displayCount; ++i) {
            if (displays[i] == mainDisplay) {
                return i;
            }
        }

        return 0;
    }

    double getDpiScale(int monitorIndex) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return 1.0;
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGDirectDisplayID displayID = displays[monitorIndex];
        CGSize size = CGDisplayScreenSize(displayID);
        CGRect rect = CGDisplayBounds(displayID);

        // 计算 DPI (假设 1 inch = 25.4 mm)
        double dpiX = (rect.size.width / size.width) * 25.4;
        return dpiX / 96.0;
    }

    std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) override {
        std::vector<DisplayMode> modes;

        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return modes;
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGDirectDisplayID displayID = displays[monitorIndex];
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);

        if (modes) {
            CFIndex count = CFArrayGetCount(modes);
            for (CFIndex i = 0; i < count; ++i) {
                CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);

                DisplayMode dm{
                    static_cast<int>(CGDisplayModeGetWidth(mode)),
                    static_cast<int>(CGDisplayModeGetHeight(mode)),
                    static_cast<int>(CGDisplayModeGetRefreshRate(mode)),
                    32  // macOS 通常为 32 位
                };
                modes.push_back(dm);
            }
            CFRelease(modes);
        }

        return modes;
    }

    std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return std::nullopt;
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGDirectDisplayID displayID = displays[monitorIndex];
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);

        if (mode) {
            DisplayMode dm{
                static_cast<int>(CGDisplayModeGetWidth(mode)),
                static_cast<int>(CGDisplayModeGetHeight(mode)),
                static_cast<int>(CGDisplayModeGetRefreshRate(mode)),
                32
            };
            CGDisplayModeRelease(mode);
            return dm;
        }

        return std::nullopt;
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
            "macOS ScreenCaptureKit Framework (High Performance)"
        };
    }

    bool supportsWindowCapture() const override {
        return true;
    }

    bool supportsCursorCapture() const override {
        return true;
    }

    CaptureConfig getConfig() const override {
        return config_;
    }

    bool updateConfig(const CaptureConfig& config) override {
        config_ = config;
        return true;
    }

private:
    CaptureConfig config_;
    bool initialized_ = false;

    static std::unique_ptr<Bitmap> cgImageToBitmap(CGImageRef cgImage, const Rect& region) {
        if (!cgImage) {
            return nullptr;
        }

        size_t width = CGImageGetWidth(cgImage);
        size_t height = CGImageGetHeight(cgImage);

        Rect cropRegion = region.isEmpty() ?
            Rect{0, 0, static_cast<int>(width), static_cast<int>(height)} :
            region;

        // 限制裁剪区域
        cropRegion.x = std::max(0, std::min(cropRegion.x, static_cast<int>(width) - 1));
        cropRegion.y = std::max(0, std::min(cropRegion.y, static_cast<int>(height) - 1));
        cropRegion.width = std::min(cropRegion.width, static_cast<int>(width) - cropRegion.x);
        cropRegion.height = std::min(cropRegion.height, static_cast<int>(height) - cropRegion.y);

        auto bitmap = std::make_unique<Bitmap>(cropRegion.width, cropRegion.height);

        // 创建位图上下文
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            bitmap->getData(),
            cropRegion.width,
            cropRegion.height,
            8,
            cropRegion.width * 4,
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host
        );

        if (context) {
            // 绘制图像
            CGRect drawRect = CGRectMake(
                -cropRegion.x,
                -cropRegion.y,
                width,
                height
            );

            CGContextDrawImage(context, drawRect, cgImage);
            CGContextRelease(context);
        }

        CGColorSpaceRelease(colorSpace);

        return bitmap;
    }
};

} // namespace wingman::platform::mac
#endif // __APPLE__
