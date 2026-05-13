#include "wingman/platform/iwindow.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include <chrono>
#include <thread>
#include <vector>

namespace wingman::platform::mac {

/**
 * @brief macOS Cocoa 窗口管理实现
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
                    return reinterpret_cast<WindowHandle>(windowID);
                }
            }

            CFRelease(windows);
        }
        return nullptr;
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
                    results.push_back(reinterpret_cast<WindowHandle>(windowID));
                }
            }

            CFRelease(windows);
        }

        return results;
    }

    WindowHandle findByClassName(const std::string& className) override {
        // macOS 不使用窗口类名概念
        return nullptr;
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
                        results.push_back(reinterpret_cast<WindowHandle>(windowID));
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

                // 窗口标题
                NSString* title = windowInfo[(id)kCGWindowName];
                if (title) {
                    info.title = [title UTF8String];
                }

                // 窗口句柄
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID windowID;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                info.handle = reinterpret_cast<WindowHandle>(windowID);

                // 窗口边界
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

                // 进程 ID
                CFNumberRef pidRef = (CFNumberRef)windowInfo[(id)kCGWindowOwnerPID];
                if (pidRef) {
                    CFNumberGetValue(pidRef, kCFNumberIntType, &info.processId);
                }

                // 前台窗口
                info.isForeground = false;  // 需要额外逻辑判断

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
                // 获取应用的进程 ID
                pid_t pid = app.processIdentifier;

                // 查找该进程的窗口
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
                            return reinterpret_cast<WindowHandle>(windowID);
                        }
                    }
                }
                CFRelease(windows);
            }
        }
        return nullptr;
    }

    bool activate(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = reinterpret_cast<CGWindowID>(hwnd);

            // 获取窗口所属的应用
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
        // macOS 需要使用 Accessibility API
        return false;
    }

    bool maximize(WindowHandle hwnd) override {
        // macOS 需要使用 Accessibility API
        return false;
    }

    bool restore(WindowHandle hwnd) override {
        // macOS 需要使用 Accessibility API
        return false;
    }

    bool close(WindowHandle hwnd) override {
        // macOS 需要发送 AppleScript 或使用 Accessibility API
        return false;
    }

    bool forceClose(WindowHandle hwnd) override {
        return false;
    }

    bool hide(WindowHandle hwnd) override {
        return false;
    }

    bool show(WindowHandle hwnd) override {
        return false;
    }

    std::string getTitle(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = reinterpret_cast<CGWindowID>(hwnd);
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
            CGWindowID windowID = reinterpret_cast<CGWindowID>(hwnd);
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
        // macOS 需要使用 Accessibility API
        return false;
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
        // macOS 需要使用 Accessibility API
        return false;
    }

    bool isValid(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = reinterpret_cast<CGWindowID>(hwnd);
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
        // macOS 需要使用 Accessibility API
        return false;
    }

    bool isMaximized(WindowHandle hwnd) override {
        // macOS 需要使用 Accessibility API
        return false;
    }

    std::optional<uint32_t> getProcessId(WindowHandle hwnd) override {
        @autoreleasepool {
            CGWindowID windowID = reinterpret_cast<CGWindowID>(hwnd);
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
            if (find(title) != nullptr) {
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
            if (find(title) == nullptr) {
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
};

} // namespace wingman::platform::mac
#endif // __APPLE__
