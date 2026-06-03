#include "wingman/platform/iscreen.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include <cstdlib>
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

NSScreen* getScreenForDisplay(CGDirectDisplayID displayID) {
    for (NSScreen* screen in [NSScreen screens]) {
        NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
        if (screenNumber && static_cast<CGDirectDisplayID>(screenNumber.unsignedIntValue) == displayID) {
            return screen;
        }
    }
    return nil;
}

} // namespace

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
        const auto displays = getDisplays();

        for (size_t i = 0; i < displays.size(); ++i) {
            if (displays[i] == mainDisplay) {
                return static_cast<int>(i);
            }
        }

        return 0;
    }

    Rect getPrimaryMonitorBounds() override {
        return getMonitorBounds(getPrimaryMonitorIndex());
    }

    Rect getMonitorBounds(int monitorIndex) override {
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return Rect{};
        }

        CGRect rect = CGDisplayBounds(displays[static_cast<size_t>(monitorIndex)]);

        return Rect{
            static_cast<int>(rect.origin.x),
            static_cast<int>(rect.origin.y),
            static_cast<int>(rect.size.width),
            static_cast<int>(rect.size.height)
        };
    }

    Rect getMonitorWorkArea(int monitorIndex) override {
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return Rect{};
        }

        @autoreleasepool {
            NSScreen* screen = getScreenForDisplay(displays[static_cast<size_t>(monitorIndex)]);
            if (!screen) {
                return getMonitorBounds(monitorIndex);
            }

            NSRect visible = screen.visibleFrame;
            return Rect{
                static_cast<int>(visible.origin.x),
                static_cast<int>(visible.origin.y),
                static_cast<int>(visible.size.width),
                static_cast<int>(visible.size.height)
            };
        }
    }

    std::string getMonitorName(int monitorIndex) override {
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return "";
        }

        @autoreleasepool {
            NSScreen* screen = getScreenForDisplay(displays[static_cast<size_t>(monitorIndex)]);
            if (screen && [screen respondsToSelector:@selector(localizedName)]) {
                NSString* localizedName = screen.localizedName;
                if (localizedName.length > 0) {
                    return std::string(localizedName.UTF8String);
                }
            }
        }
        return "Display " + std::to_string(monitorIndex);
    }

    bool isPrimaryMonitor(int monitorIndex) override {
        return monitorIndex == getPrimaryMonitorIndex();
    }

    double getDpiScale(int monitorIndex) override {
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return 1.0;
        }

        CGDirectDisplayID displayID = displays[static_cast<size_t>(monitorIndex)];
        CGSize size = CGDisplayScreenSize(displayID);
        CGRect rect = CGDisplayBounds(displayID);
        if (size.width <= 0.0) {
            return 1.0;
        }

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
        std::vector<DisplayMode> displayModes;
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return displayModes;
        }

        CGDirectDisplayID displayID = displays[static_cast<size_t>(monitorIndex)];
        CFArrayRef cgModes = CGDisplayCopyAllDisplayModes(displayID, nullptr);

        if (cgModes) {
            CFIndex count = CFArrayGetCount(cgModes);
            for (CFIndex i = 0; i < count; ++i) {
                CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(cgModes, i);

                DisplayMode dm{
                    static_cast<int>(CGDisplayModeGetWidth(mode)),
                    static_cast<int>(CGDisplayModeGetHeight(mode)),
                    static_cast<int>(CGDisplayModeGetRefreshRate(mode)),
                    32
                };
                displayModes.push_back(dm);
            }
            CFRelease(cgModes);
        }

        return displayModes;
    }

    std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) override {
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return std::nullopt;
        }

        CGDirectDisplayID displayID = displays[static_cast<size_t>(monitorIndex)];
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
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return false;
        }

        CGDirectDisplayID displayID = displays[static_cast<size_t>(monitorIndex)];

        // Find matching display mode
        CFArrayRef modes = CGDisplayCopyAllDisplayModes(displayID, nullptr);
        if (modes) {
            CFIndex count = CFArrayGetCount(modes);
            for (CFIndex i = 0; i < count; ++i) {
                CGDisplayModeRef cgMode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);

                if (static_cast<int>(CGDisplayModeGetWidth(cgMode)) == mode.width &&
                    static_cast<int>(CGDisplayModeGetHeight(cgMode)) == mode.height &&
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
        const auto displays = getDisplays();
        if (!isValidMonitorIndex(monitorIndex, static_cast<CGDisplayCount>(displays.size()))) {
            return false;
        }
        CGRestorePermanentDisplayConfiguration();
        return true;
    }

    bool isScreenSaverRunning() override {
        @autoreleasepool {
            NSArray<NSRunningApplication*>* apps =
                [NSRunningApplication runningApplicationsWithBundleIdentifier:@"com.apple.ScreenSaver.Engine"];
            return apps.count > 0;
        }
    }

    void startScreenSaver() override {
        @autoreleasepool {
            NSURL* url = [NSURL fileURLWithPath:@"/System/Library/CoreServices/ScreenSaverEngine.app"];
            if (url) {
                [[NSWorkspace sharedWorkspace] openURL:url];
            }
        }
    }

    bool isMonitorOff() override {
        return false;
    }

    void wakeUpMonitor() override {
        std::system("/usr/bin/caffeinate -u -t 1 >/dev/null 2>&1");
    }

    Rect getVirtualScreenBounds() override {
        const auto displays = getDisplays();
        if (displays.empty()) {
            return Rect{};
        }

        int minX = INT_MAX, minY = INT_MAX;
        int maxX = INT_MIN, maxY = INT_MIN;

        for (size_t i = 0; i < displays.size(); ++i) {
            CGRect rect = CGDisplayBounds(displays[i]);
            minX = std::min(minX, static_cast<int>(rect.origin.x));
            minY = std::min(minY, static_cast<int>(rect.origin.y));
            maxX = std::max(maxX, static_cast<int>(rect.origin.x + rect.size.width));
            maxY = std::max(maxY, static_cast<int>(rect.origin.y + rect.size.height));
        }

        return Rect{minX, minY, maxX - minX, maxY - minY};
    }

    int getMonitorFromPoint(const Point& point) override {
        const auto displays = getDisplays();

        CGPoint cgPoint = CGPointMake(point.x, point.y);

        for (size_t i = 0; i < displays.size(); ++i) {
            CGRect rect = CGDisplayBounds(displays[i]);
            if (CGRectContainsPoint(rect, cgPoint)) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    int getMonitorFromWindow(WindowHandle hwnd) override {
        if (hwnd == NullWindowHandle) {
            return getPrimaryMonitorIndex();
        }

        CGWindowID windowID = static_cast<CGWindowID>(hwnd);
        CFArrayRef windowIds = CFArrayCreate(nullptr, reinterpret_cast<const void**>(&windowID), 1, nullptr);
        if (!windowIds) {
            return getPrimaryMonitorIndex();
        }

        CFArrayRef descriptions = CGWindowListCreateDescriptionFromArray(windowIds);
        CFRelease(windowIds);
        if (!descriptions || CFArrayGetCount(descriptions) == 0) {
            if (descriptions) {
                CFRelease(descriptions);
            }
            return getPrimaryMonitorIndex();
        }

        CFDictionaryRef windowInfo = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(descriptions, 0));
        CFDictionaryRef boundsRef = static_cast<CFDictionaryRef>(CFDictionaryGetValue(windowInfo, kCGWindowBounds));
        CGRect bounds = CGRectNull;
        if (boundsRef) {
            CGRectMakeWithDictionaryRepresentation(boundsRef, &bounds);
        }
        CFRelease(descriptions);

        if (CGRectIsNull(bounds)) {
            return getPrimaryMonitorIndex();
        }

        return getMonitorFromPoint(Point{
            static_cast<int>(CGRectGetMidX(bounds)),
            static_cast<int>(CGRectGetMidY(bounds))
        });
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
