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
        // macOS 不使用窗口类名概念
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

                // 窗口标题
                NSString* title = windowInfo[(id)kCGWindowName];
                if (title) {
                    info.title = [title UTF8String];
                }

                // 窗口句柄
                CFNumberRef windowNumber = (CFNumberRef)windowInfo[(id)kCGWindowNumber];
                CGWindowID windowID;
                CFNumberGetValue(windowNumber, kCFNumberIntType, &windowID);
                info.handle = static_cast<WindowHandle>(windowID);

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

            // macOS 没有直接的 "最大化" 概念，使用 Zoom 来模拟
            AXError err = AXUIElementPerformAction(element, CFSTR("AXZoom"));
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool restore(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // 检查是否最小化，如果是则还原
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

            // 如果不是最小化，可能是最大化状态，再次调用 Zoom 还原
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
        // 获取窗口所属进程并终止
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

                        // 终止进程（强制关闭）
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

            // 设置 minimized 属性为 true 来隐藏窗口
            AXError err = AXUIElementSetAttributeValue(element, CFSTR("AXMinimized"), kCFBooleanTrue);
            CFRelease(element);
            return err == kAXErrorSuccess;
        }
    }

    bool show(WindowHandle hwnd) override {
        @autoreleasepool {
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // 设置 minimized 属性为 false 来显示窗口
            AXError err = AXUIElementSetAttributeValue(element, CFSTR("AXMinimized"), kCFBooleanFalse);

            // 同时确保窗口可见
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

            // 设置窗口位置
            CGPoint point = CGPointMake(bounds.x, bounds.y);
            CFTypeRef positionValue = AXValueCreate(static_cast<AXValueType>(kAXValueCGPointType), &point);
            if (positionValue) {
                AXUIElementSetAttributeValue(element, CFSTR("AXPosition"), positionValue);
                CFRelease(positionValue);
            }

            // 设置窗口大小
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
            // 获取显示器边界
            @autoreleasepool {
                CGDisplayCount displayCount;
                CGGetOnlineDisplayList(0, nullptr, &displayCount);

                if (monitorIndex >= displayCount) {
                    monitorIndex = 0;
                }

                std::vector<CGDirectDisplayID> displays(displayCount);
                CGGetOnlineDisplayList(displayCount, displays.data(), &displayCount);

                CGRect displayRect = CGDisplayBounds(displays[monitorIndex]);

                // 获取当前窗口大小
                Rect windowBounds = getBounds(hwnd);

                // 计算居中位置
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
            // macOS 使用 zoom 来实现最大化，需要通过比较窗口大小和屏幕大小来判断
            AXUIElementRef element = createWindowAXUIElement(hwnd);
            if (!element) return false;

            // 获取窗口大小
            CFTypeRef sizeValue = nullptr;
            CGSize windowSize;
            if (AXUIElementCopyAttributeValue(element, CFSTR("AXSize"), &sizeValue) == kAXErrorSuccess) {
                AXValueGetValue(reinterpret_cast<AXValueRef>(sizeValue), static_cast<AXValueType>(kAXValueCGSizeType), &windowSize);
                CFRelease(sizeValue);
            } else {
                CFRelease(element);
                return false;
            }

            // 获取主屏幕大小
            CGRect mainDisplayRect = CGDisplayBounds(CGMainDisplayID());

            // 检查窗口大小是否接近屏幕大小（允许小误差）
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
     * @brief 从窗口句柄创建应用程序 AXUIElementRef
     * @param hwnd 窗口句柄 (CGWindowID)
     * @return AXUIElementRef，失败返回 nullptr
     * @note 调用者需要在使用后 CFRelease 返回的元素
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
     * @brief 从窗口句柄获取具体的窗口 AXUIElementRef
     * @param hwnd 窗口句柄 (CGWindowID)
     * @return AXUIElementRef，失败返回 nullptr
     * @note 调用者需要在使用后 CFRelease 返回的元素
     */
    static AXUIElementRef createWindowAXUIElement(WindowHandle hwnd) {
        @autoreleasepool {
            AXUIElementRef appElement = createAppAXUIElement(hwnd);
            if (!appElement) return nullptr;

            // 获取应用程序的窗口列表
            CFArrayRef windowList = nullptr;
            if (AXUIElementCopyAttributeValues(appElement, CFSTR("AXWindows"), 0, 1024, &windowList) != kAXErrorSuccess) {
                CFRelease(appElement);
                return nullptr;
            }

            // 获取目标窗口标题（通过 CGWindowListCopyWindowInfo）
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

            // 查找匹配的窗口（通过标题匹配）
            AXUIElementRef resultElement = nullptr;
            CFIndex count = CFArrayGetCount(windowList);
            for (CFIndex i = 0; i < count; ++i) {
                AXUIElementRef windowElement = (AXUIElementRef)CFArrayGetValueAtIndex(windowList, i);

                // 如果目标标题为空，直接返回第一个窗口
                if (targetTitle.empty() && i == 0) {
                    CFRetain(windowElement);
                    resultElement = windowElement;
                    break;
                }

                // 获取窗口标题
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

            // 如果没有找到匹配的窗口，返回第一个窗口
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
