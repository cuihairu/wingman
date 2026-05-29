#include "wingman/platform/iwindow.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>

#include <chrono>
#include <thread>
#include <vector>
#include <cmath>
#include <csignal>  // for kill()

namespace wingman::platform::mac {

/**
 * @brief macOS Cocoa window management implementation
 */
class CocoaWindow : public IWindow {
public:
    CocoaWindow() = default;
    ~CocoaWindow() override {
        shutdown();
    }

    bool initialize() override {
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    WindowHandle find(const std::string& title) override {
        @autoreleasepool {
            NSString* titleStr = [NSString stringWithUTF8String:title.c_str()];
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                NSString* windowTitle = windowInfo[(id)kCGWindowName];
                if (windowTitle && [windowTitle rangeOfString:titleStr options:NSCaseInsensitiveSearch].location != NSNotFound) {
                    CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                    CGWindowID windowID;
                    CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                    CFRelease(windows);
                    return static_cast<WindowHandle>(windowID);
                }
            }

            CFRelease(windows);
        }
        return NullWindowHandle;
    }

    std::vector<WindowHandle> findAll(const std::string& title) override {
        std::vector<WindowHandle> results;

        @autoreleasepool {
            NSString* titleStr = [NSString stringWithUTF8String:title.c_str()];
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                NSString* windowTitle = windowInfo[(id)kCGWindowName];
                if (windowTitle && [windowTitle rangeOfString:titleStr options:NSCaseInsensitiveSearch].location != NSNotFound) {
                    CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                    CGWindowID windowID;
                    CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                    results.push_back(static_cast<WindowHandle>(windowID));
                }
            }

            CFRelease(windows);
        }

        return results;
    }

    WindowHandle findByClassName(const std::string& className) override {
        // macOS does not use window class name concept
        return NullWindowHandle;
    }

    std::vector<WindowHandle> findByProcessId(uint32_t processId) override {
        std::vector<WindowHandle> results;

        @autoreleasepool {
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                if (pidRef) {
                    int pid;
                    CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
                    if (static_cast<uint32_t>(pid) == processId) {
                        CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                        CGWindowID windowID;
                        CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                        results.push_back(static_cast<WindowHandle>(windowID));
                    }
                }
            }

            CFRelease(windows);
        }

        return results;
    }

    std::vector<WindowInfo> enumerate() override {
        std::vector<WindowInfo> results;

        @autoreleasepool {
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                WindowInfo info;

                // Window title
                NSString* title = windowInfo[(id)kCGWindowName];
                if (title) {
                    info.title = [title UTF8String];
                }

                // Window handle
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID windowID;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                info.handle = static_cast<WindowHandle>(windowID);

                // Window bounds
                CFDictionaryRef bounds = (CFDictionaryRef)windowInfo[(id)kCGWindowBounds];
                if (bounds) {
                    CGRect rect;
                    CGRectMakeWithDictionaryRepresentation(bounds, &rect);
                    info.bounds = Rect{
                        static_cast<int>(rect.origin.x),
                        static_cast<int>(rect.origin.y),
                        static_cast<int>(rect.size.width),
                        static_cast<int>(rect.size.height)
                    };
                }

                // Process ID
                CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                if (pidRef) {
                    CFNumberGetValue(pidRef, kCFNumberIntType, &info.processId);
                }

                // Foreground window
                info.isForeground = false;  // Requires additional logic

                results.push_back(info);
            }

            CFRelease(windows);
        }

        return results;
    }

    WindowHandle getForeground() override {
        @autoreleasepool {
            NSRunningApplication* app = [NSWorkspace sharedWorkspace].frontmostApplication;
            if (app) {
                // Get application process ID
                pid_t pid = app.processIdentifier;

                // Find windows of this process
                NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
                for (NSDictionary* windowInfo in windows) {
                    CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                    if (pidRef) {
                        int windowPid;
                        CFNumberGetValue(pidRef, kCFNumberIntType, &windowPid);
                        if (windowPid == pid) {
                            CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                            CGWindowID windowID;
                            CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                            CFRelease(windows);
                            return static_cast<WindowHandle>(windowID);
                        }
                    }
                }
                CFRelease(windows);
            }
        }
        return NullWindowHandle;
    }

    bool activate(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = static_cast<CGWindowID>(hwnd);

            // Get the app the window belongs to
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == windowID) {
                    CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                    if (pidRef) {
                        pid_t pid;
                        CFNumberGetValue(pidRef, kCFNumberIntType, &pid);

                        NSRunningApplication* app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
                        if (app) {
                            [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
                            CFRelease(windows);
                            return true;
                        }
                    }
                    break;
                }
            }
            CFRelease(windows);
        }
        return false;
    }

    bool minimize(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            AXError err = AXUIElementPerformAction(element, CFSTR("AXMinimize"));
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool maximize(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // macOS has no direct "maximize" concept, use Zoom to simulate
            AXError err = AXUIElementPerformAction(element, CFSTR("AXZoom"));
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool restore(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // Check if minimized, restore if so
            CFBooleanRef minimized = nullptr;
            if (AXUIElementCopyAttributeValue(element, CFSTR("AXMinimized"), (CFTypeRef*)&minimized) == kAXErrorSuccess) {
                bool isMin = CFBooleanGetValue(minimized);
                CFRelease(minimized);

                if (isMin) {
                    AXError err = AXUIElementSetAttributeValue(element, CFSTR("AXMinimized"), kCFBooleanFalse);
                    CFRelease(element);
                    return err == kAXErrorSuccess;
                }
            }

            // If not minimized, may be maximized, call Zoom again to restore
            AXError err = AXUIElementPerformAction(element, CFSTR("AXZoom"));
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool close(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            AXError err = AXUIElementPerformAction(element, CFSTR("AXRaise"));
            if (err == kAXErrorSuccess) {
                err = AXUIElementPerformAction(element, CFSTR("AXClose"));
            }
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool forceClose(WindowHandle hwnd) override {
        // Get window process and terminate
        @autoreleasepool {
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == static_cast<CGWindowID>(hwnd)) {
                    CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                    if (pidRef) {
                        pid_t pid;
                        CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
                        CFRelease(windows);

                        // Terminate process (force close)
                        kill(pid, SIGKILL);
                        return true;
                    }
                    break;
                }
            }

            CFRelease(windows);
        }
        return false;
    }

    bool hide(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // Set minimized property to true to hide window
            AXError err = AXUIElementSetAttributeValue(element, CFSTR("AXMinimized"), kCFBooleanTrue);
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool show(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // Set minimized property to false to show window
            AXError err = AXUIElementSetAttributeValue(element, CFSTR("AXMinimized"), kCFBooleanFalse);

            // Also ensure window is visible
            if (err == kAXErrorSuccess) {
                AXUIElementSetAttributeValue(element, CFSTR("AXHidden"), kCFBooleanFalse);
            }

            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    std::string getTitle(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = static_cast<CGWindowID>(hwnd);
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == windowID) {
                    NSString* title = windowInfo[(id)kCGWindowName];
                    CFRelease(windows);
                    return title ? [title UTF8String] : "";
                }
            }

            CFRelease(windows);
        }
        return "";
    }

    Rect getBounds(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = static_cast<CGWindowID>(hwnd);
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == windowID) {
                    CFDictionaryRef bounds = (CFDictionaryRef)windowInfo[(id)kCGWindowBounds];
                    if (bounds) {
                        CGRect rect;
                        CGRectMakeWithDictionaryRepresentation(bounds, &rect);
                        CFRelease(windows);
                        return Rect{
                            static_cast<int>(rect.origin.x),
                            static_cast<int>(rect.origin.y),
                            static_cast<int>(rect.size.width),
                            static_cast<int>(rect.size.height)
                        };
                    }
                }
            }

            CFRelease(windows);
        }
        return Rect{};
    }

    bool setBounds(WindowHandle hwnd, const Rect& bounds) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // Set window position
            CGPoint point = CGPointMake(bounds.x, bounds.y);
            CFTypeRef positionValue = AXValueCreate(static_cast<AXValueType>(kAXValueCGPointType), &point);
            if (positionValue) {
                AXUIElementSetAttributeValue(element, CFSTR("AXPosition"), positionValue);
                CFRelease(positionValue);
            }

            // Set window size
            CGSize size = CGSizeMake(bounds.width, bounds.height);
            CFTypeRef sizeValue = AXValueCreate(static_cast<AXValueType>(kAXValueCGSizeType), &size);
            if (sizeValue) {
                AXUIElementSetAttributeValue(element, CFSTR("AXSize"), sizeValue);
                CFRelease(sizeValue);
            }

            CFRelease(element);
            return true;
        }
    }

    bool move(WindowHandle hwnd, int x, int y) override {
        Rect bounds = getBounds(hwnd);
        bounds.x = x;
        bounds.y = y;
        return setBounds(hwnd, bounds);
    }

    bool resize(WindowHandle hwnd, int width, int height) override {
        Rect bounds = getBounds(hwnd);
        bounds.width = width;
        bounds.height = height;
        return setBounds(hwnd, bounds);
    }

    bool center(WindowHandle hwnd, int monitorIndex) override {
        @autoreleasepool {
            // Get display bounds
            @autoreleasepool {
                CGDisplayCount displayCount;
                CGGetOnlineDisplayList(0, nullptr, &displayCount);

                if (monitorIndex >= displayCount) {
                    monitorIndex = 0;
                }

                std::vector<CGDirectDisplayID> displays(displayCount);
                CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

                CGRect displayRect = CGDisplayBounds(displays[monitorIndex]);

                // Get current window size
                Rect windowBounds = getBounds(hwnd);

                // Calculate centered position
                int x = static_cast<int>(displayRect.origin.x + (displayRect.size.width - windowBounds.width) / 2);
                int y = static_cast<int>(displayRect.origin.y + (displayRect.size.height - windowBounds.height) / 2);

                return move(hwnd, x, y);
            }
        }
    }

    bool isValid(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = static_cast<CGWindowID>(hwnd);
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == windowID) {
                    CFRelease(windows);
                    return true;
                }
            }

            CFRelease(windows);
        }
        return false;
    }

    bool isVisible(WindowHandle hwnd) override {
        return isValid(hwnd);
    }

    bool isForeground(WindowHandle hwnd) override {
        return getForeground() == hwnd;
    }

    bool isMinimized(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            CFBooleanRef minimized = nullptr;
            AXError err = AXUIElementCopyAttributeValue(element, CFSTR("AXMinimized"), (CFTypeRef*)&minimized);
            CFRelease(element);

            if (err == kAXErrorSuccess && minimized) {
                bool result = CFBooleanGetValue(minimized);
                CFRelease(minimized);
                return result;
            }
        }
        return false;
    }

    bool isMaximized(WindowHandle hwnd) override {
        @autoreleasepool {
            // macOS uses zoom for maximize, need to compare window size with screen size
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // Get window size
            CFTypeRef sizeValue = nullptr;
            CGSize windowSize;
            if (AXUIElementCopyAttributeValue(element, CFSTR("AXSize"), &sizeValue) == kAXErrorSuccess) {
                AXValueGetValue(reinterpret_cast<AXValueRef>(sizeValue), static_cast<AXValueType>(kAXValueCGSizeType), &windowSize);
                CFRelease(sizeValue);
            } else {
                CFRelease(element);
                return false;
            }

            // Get main screen size
            CGRect mainDisplayRect = CGDisplayBounds(CGMainDisplayID());

            // Check if window size is close to screen size (allow small margin)
            bool isMax = (std::abs(windowSize.width - mainDisplayRect.size.width) < 10 &&
                         std::abs(windowSize.height - mainDisplayRect.size.height) < 10);

            CFRelease(element);
            return isMax;
        }
    }

    std::optional<uint32_t> getProcessId(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = static_cast<CGWindowID>(hwnd);
            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == windowID) {
                    CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                    if (pidRef) {
                        uint32_t pid;
                        CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
                        CFRelease(windows);
                        return pid;
                    }
                }
            }

            CFRelease(windows);
        }
        return std::nullopt;
    }

    bool waitFor(const std::string& title, int timeoutMs) override {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (find(title) != NullWindowHandle) {
                return true;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool waitClose(const std::string& title, int timeoutMs) override {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (find(title) == NullWindowHandle) {
                return true;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool waitForForeground(WindowHandle hwnd, int timeoutMs) override {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            if (isForeground(hwnd)) {
                return true;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeoutMs) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::string getBackendName() const override {
        return "Cocoa";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Cocoa",
            "1.0",
            initialized_,
            "macOS Cocoa Window Manager"
        };
    }

private:
    bool initialized_ = false;

    /**
     * @brief Create application AXUIElementRef from window handle
     * @param hwnd Window handle (CGWindowID)
     * @return AXUIElementRef, returns nullptr on failure
     * @note Caller must CFRelease the returned element after use
     */
    static AXUIElementRef createAppAXUIElement(WindowHandle hwnd) {
        @autoreleasepool {
            CGWindowID windowID = static_cast<CGWindowID>(hwnd);

            NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

            for (NSDictionary* windowInfo in windows) {
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID wid;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                if (wid == windowID) {
                    CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                    if (pidRef) {
                        pid_t pid;
                        CFNumberGetValue(pidRef, kCFNumberIntType, &pid);
                        CFRelease(windows);
                        return AXUIElementCreateApplication(pid);
                    }
                    break;
                }
            }

            CFRelease(windows);
        }
        return nullptr;
    }

    /**
     * @brief Get specific window AXUIElementRef from window handle
     * @param hwnd Window handle (CGWindowID)
     * @return AXUIElementRef, returns nullptr on failure
     * @note Caller must CFRelease the returned element after use
     */
    static AXUIElementRef createWindowAXUIElement(WindowHandle hwnd) {
        @autoreleasepool {
            AXUIElementRef appElement = createAppAXUIElement(hwnd);
            if (!appElement) return nullptr;

            // Get application window list
            CFArrayRef windowList = nullptr;
            if (AXUIElementCopyAttributeValues(appElement, CFSTR("AXWindows"), 0, 1024, &windowList) != kAXErrorSuccess) {
                CFRelease(appElement);
                return nullptr;
            }

            // Get target window title (via CGWindowListCopyWindowInfo)
            std::string targetTitle;
            @autoreleasepool {
                CGWindowID windowID = static_cast<CGWindowID>(hwnd);
                NSArray* windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);

                for (NSDictionary* windowInfo in windows) {
                    CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                    CGWindowID wid;
                    CFNumberGetValue(windowNumber, kCFNumberIntType, &wid);

                    if (wid == windowID) {
                        NSString* title = windowInfo[(id)kCGWindowName];
                        if (title) {
                            targetTitle = [title UTF8String];
                        }
                        break;
                    }
                }

                CFRelease(windows);
            }

            // Find matching window (by title match)
            AXUIElementRef resultElement = nullptr;
            CFIndex count = CFArrayGetCount(windowList);
            for (CFIndex i = 0; i < count; ++i) {
                AXUIElementRef windowElement = (AXUIElementRef)CFArrayGetValueAtIndex(windowList, i);

                // If target title is empty, return first window directly
                if (targetTitle.empty() && i == 0) {
                    CFRetain(windowElement);
                    resultElement = windowElement;
                    break;
                }

                // Get window title
                CFStringRef titleRef = nullptr;
                if (AXUIElementCopyAttributeValue(windowElement, CFSTR("AXTitle"), (CFTypeRef*)&titleRef) == kAXErrorSuccess && titleRef) {
                    const char* titlePtr = CFStringGetCStringPtr(titleRef, kCFStringEncodingUTF8);
                    if (titlePtr) {
                        std::string title = titlePtr;
                        if (title == targetTitle) {
                            CFRetain(windowElement);
                            resultElement = windowElement;
                            CFRelease(titleRef);
                            break;
                        }
                    }
                    CFRelease(titleRef);
                }
            }

            // If no matching window found, return first window
            if (!resultElement && count > 0) {
                AXUIElementRef windowElement = (AXUIElementRef)CFArrayGetValueAtIndex(windowList, 0);
                CFRetain(windowElement);
                resultElement = windowElement;
            }

            CFRelease(windowList);
            CFRelease(appElement);
            return resultElement;
        }
    }
};

} // namespace wingman::platform::mac
#endif // __APPLE__
