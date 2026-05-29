#include "wingman/platform/iscreen.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <vector>

namespace wingman::platform::mac {

/**
 * @brief macOS Cocoa screen management implementation
 */
class CocoaScreen : public IScreen {
public:
    CocoaScreen() = default;
    ~CocoaScreen() override {
        shutdown();
    }

    bool initialize() override {
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    int getMonitorCount() override {
        uint32_t count;
        CGGetOnlineDisplayList(0, nullptr, &count);
        return static_cast<int>(count);
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

    Rect getPrimaryMonitorBounds() override {
        return getMonitorBounds(getPrimaryMonitorIndex());
    }

    Rect getMonitorBounds(int monitorIndex) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return Rect{};
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGRect rect = CGDisplayBounds(displays[monitorIndex]);

        return Rect{
            static_cast<int>(rect.origin.x),
            static_cast<int>(rect.origin.y),
            static_cast<int>(rect.size.width),
            static_cast<int>(rect.size.height)
        };
    }

    Rect getMonitorWorkArea(int monitorIndex) override {
        // macOS workspace needs NSScreen, simplified here
        return getMonitorBounds(monitorIndex);
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
            return std::string(buffer);
        }

        return "Display " + std::to_string(monitorIndex);
    }

    bool isPrimaryMonitor(int monitorIndex) override {
        return monitorIndex == getPrimaryMonitorIndex();
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

        // Calculate DPI (assuming 1 inch = 25.4 mm)
        double dpiX = (rect.size.width / size.width) * 25.4;
        return dpiX / 96.0;
    }

    int getDpi(int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        return static_cast<int>(scale * 96.0);
    }

    Point logicalToPhysical(const Point& point, int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        return Point{
            static_cast<int>(point.x * scale),
            static_cast<int>(point.y * scale)
        };
    }

    Point physicalToLogical(const Point& point, int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        return Point{
            static_cast<int>(point.x / scale),
            static_cast<int>(point.y / scale)
        };
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
                    32
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

    bool setDisplayMode(int monitorIndex, const DisplayMode& mode) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return false;
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGDirectDisplayID displayID = displays[monitorIndex];

        // Find matching display mode
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);
        if (modes) {
            CFIndex count = CFArrayGetCount(modes);
            for (CFIndex i = 0; i < count; ++i) {
                CGDisplayModeRef cgMode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);

                if (CGDisplayModeGetWidth(cgMode) == mode.width &&
                    CGDisplayModeGetHeight(cgMode) == mode.height &&
                    CGDisplayModeGetRefreshRate(cgMode) == mode.refreshRate) {

                    CGDisplayConfigRef config;
                    if (CGBeginDisplayConfiguration(&config) == kCGErrorSuccess) {
                        CGConfigureDisplayWithDisplayMode(config, displayID, cgMode, nullptr);
                        CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
                        CFRelease(modes);
                        return true;
                    }
                }
            }
            CFRelease(modes);
        }

        return false;
    }

    bool resetDisplayMode(int monitorIndex) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        if (monitorIndex >= displayCount) {
            return false;
        }

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGDisplayRestoreDisplaySettings(displays[monitorIndex]);
        return true;
    }

    bool isScreenSaverRunning() override {
        @autoreleasepool {
            NSDictionary* info = [[NSWorkspace sharedWorkspace] screenSaverIsRunning] ? @{@(YES) : @NO} : nil;
            return [info[@(YES)] boolValue];
        }
        return false;
    }

    void startScreenSaver() override {
        @autoreleasepool {
            [[NSWorkspace sharedWorkspace] openFile:@"/System/Library/Frameworks/ScreenSaver.framework/Resources/ScreenSaverEngine.app"];
        }
    }

    bool isMonitorOff() override {
        return false;
    }

    void wakeUpMonitor() override {
        // macOS requires Power Management API
    }

    Rect getVirtualScreenBounds() override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        int minX = INT_MAX, minY = INT_MAX;
        int maxX = INT_MIN, maxY = INT_MIN;

        for (CGDisplayCount i = 0; i < displayCount; ++i) {
            CGRect rect = CGDisplayBounds(displays[i]);
            minX = std::min(minX, static_cast<int>(rect.origin.x));
            minY = std::min(minY, static_cast<int>(rect.origin.y));
            maxX = std::max(maxX, static_cast<int>(rect.origin.x + rect.size.width));
            maxY = std::max(maxY, static_cast<int>(rect.origin.y + rect.size.height));
        }

        return Rect{minX, minY, maxX - minX, maxY - minY};
    }

    int getMonitorFromPoint(const Point& point) override {
        CGDisplayCount displayCount;
        CGGetOnlineDisplayList(0, nullptr, &displayCount);

        std::vector<CGDirectDisplayID> displays(displayCount);
        CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

        CGPoint cgPoint = CGPointMake(point.x, point.y);

        for (CGDisplayCount i = 0; i < displayCount; ++i) {
            CGRect rect = CGDisplayBounds(displays[i]);
            if (CGRectContainsPoint(rect, cgPoint)) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    int getMonitorFromWindow(WindowHandle hwnd) override {
        // macOS needs to get via NSWindow
        CGDisplayID display = CGMainDisplayID();
        return getPrimaryMonitorIndex();
    }

    std::string getBackendName() const override {
        return "Cocoa";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Cocoa",
            "1.0",
            initialized_,
            "macOS Cocoa Screen API"
        };
    }

private:
    bool initialized_ = false;
};

} // namespace wingman::platform::mac
#endif // __APPLE__
