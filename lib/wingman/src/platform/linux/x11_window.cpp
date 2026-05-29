#include "wingman/platform/iwindow.hpp"
#include <spdlog/spdlog.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include <chrono>
#include <thread>
#include <vector>
#include <string>

// Undefine linux macro
#undef linux

namespace wingman::platform::linux {

/**
 * @brief Linux X11 window management implementation
 */
class X11Window : public IWindow {
public:
    X11Window() : display_(nullptr), initialized_(false) {}

    ~X11Window() override {
        shutdown();
    }

    bool initialize() override {
        if (initialized_) return true;

        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("[X11Window] Failed to open X display");
            return false;
        }

        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }
        initialized_ = false;
    }

    WindowHandle find(const std::string& title) override {
        if (!initialized_) return NullWindowHandle;

        Window result = searchWindows(title);
        return result != None ? static_cast<WindowHandle>(result) : NullWindowHandle;
    }

    std::vector<WindowHandle> findAll(const std::string& title) override {
        std::vector<WindowHandle> results;

        if (!initialized_) return results;

        searchWindowsRecursive(title, XDefaultRootWindow(display_), results);

        return results;
    }

    WindowHandle findByClassName(const std::string& className) override {
        if (!initialized_) return NullWindowHandle;

        Window result = searchWindowsByClassName(className);
        return result != None ? static_cast<WindowHandle>(result) : NullWindowHandle;
    }

    std::vector<WindowHandle> findByProcessId(uint32_t processId) override {
        std::vector<WindowHandle> results;

        if (!initialized_) return results;

        searchWindowsByPid(processId, XDefaultRootWindow(display_), results);

        return results;
    }

    std::vector<WindowInfo> enumerate() override {
        std::vector<WindowInfo> results;

        if (!initialized_) return results;

        enumerateWindows(XDefaultRootWindow(display_), results);

        return results;
    }

    WindowHandle getForeground() override {
        if (!initialized_) return NullWindowHandle;

        Window window = getActiveWindow();
        return window != None ? static_cast<WindowHandle>(window) : NullWindowHandle;
    }

    bool activate(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Get screen info
        int screen = DefaultScreen(display_);
        Window rootWindow = XRootWindow(display_, screen);

        // Send activation event
        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.serial = 0;
        event.xclient.send_event = True;
        event.xclient.display = display_;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display_, "_NET_ACTIVE_WINDOW", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = 2;  // 2 = normal application
        event.xclient.data.l[1] = CurrentTime;
        event.xclient.data.l[2] = 0;

        XSendEvent(display_, rootWindow, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);

        // Also raise the window
        XRaiseWindow(display_, window);
        XFlush(display_);

        return true;
    }

    bool minimize(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Use _NET_WM_STATE_HIDDEN property
        Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
        Atom wmStateHidden = XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False);

        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.serial = 0;
        event.xclient.send_event = True;
        event.xclient.display = display_;
        event.xclient.window = window;
        event.xclient.message_type = wmState;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;  // _NET_WM_STATE_ADD
        event.xclient.data.l[1] = wmStateHidden;
        event.xclient.data.l[2] = 0;
        event.xclient.data.l[3] = 0;
        event.xclient.data.l[4] = 0;

        int screen = DefaultScreen(display_);
        Window rootWindow = XRootWindow(display_, screen);
        XSendEvent(display_, rootWindow, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(display_);

        return true;
    }

    bool maximize(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Use _NET_WM_STATE_MAXIMIZED_HORZ and _NET_WM_STATE_MAXIMIZED_VERT
        Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
        Atom wmMaxHorz = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        Atom wmMaxVert = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False);

        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.serial = 0;
        event.xclient.send_event = True;
        event.xclient.display = display_;
        event.xclient.window = window;
        event.xclient.message_type = wmState;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 1;  // _NET_WM_STATE_ADD
        event.xclient.data.l[1] = wmMaxHorz;
        event.xclient.data.l[2] = wmMaxVert;
        event.xclient.data.l[3] = 0;
        event.xclient.data.l[4] = 0;

        int screen = DefaultScreen(display_);
        Window rootWindow = XRootWindow(display_, screen);
        XSendEvent(display_, rootWindow, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(display_);

        return true;
    }

    bool restore(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Check if minimized, restore if so
        if (isMinimized(hwnd)) {
            Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
            Atom wmStateHidden = XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False);

            XEvent event = {};
            event.xclient.type = ClientMessage;
            event.xclient.serial = 0;
            event.xclient.send_event = True;
            event.xclient.display = display_;
            event.xclient.window = window;
            event.xclient.message_type = wmState;
            event.xclient.format = 32;
            event.xclient.data.l[0] = 0;  // _NET_WM_STATE_REMOVE
            event.xclient.data.l[1] = wmStateHidden;
            event.xclient.data.l[2] = 0;
            event.xclient.data.l[3] = 0;
            event.xclient.data.l[4] = 0;

            int screen = DefaultScreen(display_);
            Window rootWindow = XRootWindow(display_, screen);
            XSendEvent(display_, rootWindow, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
            XFlush(display_);
        }

        // Unmaximize
        if (isMaximized(hwnd)) {
            Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
            Atom wmMaxHorz = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
            Atom wmMaxVert = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False);

            XEvent event = {};
            event.xclient.type = ClientMessage;
            event.xclient.serial = 0;
            event.xclient.send_event = True;
            event.xclient.display = display_;
            event.xclient.window = window;
            event.xclient.message_type = wmState;
            event.xclient.format = 32;
            event.xclient.data.l[0] = 0;  // _NET_WM_STATE_REMOVE
            event.xclient.data.l[1] = wmMaxHorz;
            event.xclient.data.l[2] = wmMaxVert;
            event.xclient.data.l[3] = 0;
            event.xclient.data.l[4] = 0;

            int screen = DefaultScreen(display_);
            Window rootWindow = XRootWindow(display_, screen);
            XSendEvent(display_, rootWindow, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
            XFlush(display_);
        }

        return true;
    }

    bool close(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Send WM_DELETE_WINDOW message
        Atom wmProtocols = XInternAtom(display_, "WM_PROTOCOLS", False);
        Atom wmDeleteWindow = XInternAtom(display_, "WM_DELETE_WINDOW", False);

        Atom* protocols = nullptr;
        int numProtocols = 0;
        if (XGetWMProtocols(display_, window, &protocols, &numProtocols) != 0) {
            for (int i = 0; i < numProtocols; ++i) {
                if (protocols[i] == wmDeleteWindow) {
                    XEvent event = {};
                    event.xclient.type = ClientMessage;
                    event.xclient.serial = 0;
                    event.xclient.send_event = True;
                    event.xclient.display = display_;
                    event.xclient.window = window;
                    event.xclient.message_type = wmProtocols;
                    event.xclient.format = 32;
                    event.xclient.data.l[0] = wmDeleteWindow;
                    event.xclient.data.l[1] = CurrentTime;

                    XSendEvent(display_, window, False, NoEventMask, &event);
                    XFree(protocols);
                    XFlush(display_);
                    return true;
                }
            }
            XFree(protocols);
        }

        // If WM_DELETE_WINDOW not supported, try KillClient
        XKillClient(display_, window);
        XFlush(display_);

        return true;
    }

    bool forceClose(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);
        XKillClient(display_, window);
        XFlush(display_);

        return true;
    }

    bool hide(WindowHandle hwnd) override {
        // X11 has no direct hide, use minimize instead
        return minimize(hwnd);
    }

    bool show(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);
        XMapWindow(display_, window);
        XFlush(display_);

        return true;
    }

    std::string getTitle(WindowHandle hwnd) override {
        if (!initialized_) return "";

        Window window = static_cast<Window>(hwnd);

        Atom wmName = XInternAtom(display_, "_NET_WM_NAME", False);
        Atom utf8String = XInternAtom(display_, "UTF8_STRING", False);

        Atom actualType;
        int actualFormat;
        unsigned long numItems, bytesAfter;
        unsigned char* prop = nullptr;

        std::string result;

        // First try _NET_WM_NAME (supports UTF-8)
        if (XGetWindowProperty(display_, window, wmName, 0, 1024, False, utf8String,
                              &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success) {
            if (prop && numItems > 0) {
                result = std::string(reinterpret_cast<char*>(prop), numItems);
                XFree(prop);
                return result;
            }
            if (prop) XFree(prop);
        }

        // Fall back to WM_NAME (legacy property)
        char* windowName = nullptr;
        if (XFetchName(display_, window, &windowName) != 0 && windowName) {
            result = std::string(windowName);
            XFree(windowName);
        }

        return result;
    }

    Rect getBounds(WindowHandle hwnd) override {
        if (!initialized_) return Rect{};

        Window window = static_cast<Window>(hwnd);

        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, window, &attrs) == 0) {
            return Rect{};
        }

        int screen = DefaultScreen(display_);
        Window rootWindow = XRootWindow(display_, screen);

        // Get coordinates relative to root window
        int x, y;
        Window child;
        XTranslateCoordinates(display_, window, rootWindow, 0, 0, &x, &y, &child);

        return Rect{
            x,
            y,
            attrs.width,
            attrs.height
        };
    }

    bool setBounds(WindowHandle hwnd, const Rect& bounds) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        XMoveResizeWindow(display_, window, bounds.x, bounds.y, bounds.width, bounds.height);
        XFlush(display_);

        return true;
    }

    bool move(WindowHandle hwnd, int x, int y) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);
        XMoveWindow(display_, window, x, y);
        XFlush(display_);

        return true;
    }

    bool resize(WindowHandle hwnd, int width, int height) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);
        XResizeWindow(display_, window, width, height);
        XFlush(display_);

        return true;
    }

    bool center(WindowHandle hwnd, int monitorIndex) override {
        if (!initialized_) return false;

        int screenCount = ScreenCount(display_);
        if (monitorIndex >= screenCount) {
            monitorIndex = 0;
        }

        ::Screen* screenInfo = ScreenOfDisplay(display_, monitorIndex);
        int screenWidth = WidthOfScreen(screenInfo);
        int screenHeight = HeightOfScreen(screenInfo);

        Rect windowBounds = getBounds(hwnd);

        int x = (screenWidth - windowBounds.width) / 2;
        int y = (screenHeight - windowBounds.height) / 2;

        return move(hwnd, x, y);
    }

    bool isValid(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        XWindowAttributes attrs;
        return XGetWindowAttributes(display_, window, &attrs) != 0;
    }

    bool isVisible(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, window, &attrs) == 0) {
            return false;
        }

        return attrs.map_state != IsUnmapped;
    }

    bool isForeground(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window activeWindow = getActiveWindow();
        return activeWindow == static_cast<Window>(hwnd);
    }

    bool isMinimized(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Check _NET_WM_STATE_HIDDEN property
        Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
        Atom wmStateHidden = XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False);

        Atom actualType;
        int actualFormat;
        unsigned long numItems, bytesAfter;
        unsigned char* prop = nullptr;

        bool result = false;
        if (XGetWindowProperty(display_, window, wmState, 0, 1024, False, AnyPropertyType,
                              &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success) {
            if (prop && actualFormat == 32) {
                Atom* states = reinterpret_cast<Atom*>(prop);
                for (unsigned long i = 0; i < numItems; ++i) {
                    if (states[i] == wmStateHidden) {
                        result = true;
                        break;
                    }
                }
            }
            if (prop) XFree(prop);
        }

        return result;
    }

    bool isMaximized(WindowHandle hwnd) override {
        if (!initialized_) return false;

        Window window = static_cast<Window>(hwnd);

        // Check _NET_WM_STATE_MAXIMIZED_HORZ and _NET_WM_STATE_MAXIMIZED_VERT
        Atom wmState = XInternAtom(display_, "_NET_WM_STATE", False);
        Atom wmMaxHorz = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        Atom wmMaxVert = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_VERT", False);

        Atom actualType;
        int actualFormat;
        unsigned long numItems, bytesAfter;
        unsigned char* prop = nullptr;

        bool horzMax = false;
        bool vertMax = false;

        if (XGetWindowProperty(display_, window, wmState, 0, 1024, False, AnyPropertyType,
                              &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success) {
            if (prop && actualFormat == 32) {
                Atom* states = reinterpret_cast<Atom*>(prop);
                for (unsigned long i = 0; i < numItems; ++i) {
                    if (states[i] == wmMaxHorz) horzMax = true;
                    if (states[i] == wmMaxVert) vertMax = true;
                }
            }
            if (prop) XFree(prop);
        }

        return horzMax && vertMax;
    }

    std::optional<uint32_t> getProcessId(WindowHandle hwnd) override {
        if (!initialized_) return std::nullopt;

        Window window = static_cast<Window>(hwnd);

        Atom wmPid = XInternAtom(display_, "_NET_WM_PID", False);

        Atom actualType;
        int actualFormat;
        unsigned long numItems, bytesAfter;
        unsigned char* prop = nullptr;

        std::optional<uint32_t> result;
        if (XGetWindowProperty(display_, window, wmPid, 0, 1, False, XA_CARDINAL,
                              &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success) {
            if (prop && actualFormat == 32 && numItems > 0) {
                result = static_cast<uint32_t>(*reinterpret_cast<unsigned long*>(prop));
            }
            if (prop) XFree(prop);
        }

        return result;
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
        return "X11";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "X11",
            "1.0",
            initialized_,
            "Linux X11 Window Manager"
        };
    }

private:
    Display* display_;
    bool initialized_;

    Window getActiveWindow() {
        Window activeWindow = None;
        Atom actualType;
        int actualFormat;
        unsigned long numItems, bytesAfter;
        unsigned char* prop = nullptr;

        Atom netActiveWindow = XInternAtom(display_, "_NET_ACTIVE_WINDOW", False);
        int screen = DefaultScreen(display_);
        Window rootWindow = XRootWindow(display_, screen);

        if (XGetWindowProperty(display_, rootWindow, netActiveWindow, 0, 1, False, XA_WINDOW,
                              &actualType, &actualFormat, &numItems, &bytesAfter, &prop) == Success) {
            if (prop && actualFormat == 32 && numItems > 0) {
                activeWindow = *reinterpret_cast<Window*>(prop);
            }
            if (prop) XFree(prop);
        }

        return activeWindow;
    }

    Window searchWindows(const std::string& title) {
        return searchWindowRecursive(title, XDefaultRootWindow(display_));
    }

    Window searchWindowRecursive(const std::string& title, Window root) {
        Window parent;
        Window* children = nullptr;
        unsigned int numChildren;

        if (XQueryTree(display_, root, &root, &parent, &children, &numChildren) == 0) {
            for (unsigned int i = 0; i < numChildren; ++i) {
                Window child = children[i];

                // Check current window
                std::string windowTitle = getTitle(static_cast<WindowHandle>(child));
                if (!windowTitle.empty() && windowTitle.find(title) != std::string::npos) {
                    XFree(children);
                    return child;
                }

                // Recursively check child windows
                Window found = searchWindowRecursive(title, child);
                if (found != None) {
                    XFree(children);
                    return found;
                }
            }

            if (children) XFree(children);
        }

        return None;
    }

    void searchWindowsRecursive(const std::string& title, Window root, std::vector<WindowHandle>& results) {
        Window parent;
        Window* children = nullptr;
        unsigned int numChildren;

        if (XQueryTree(display_, root, &root, &parent, &children, &numChildren) == 0) {
            for (unsigned int i = 0; i < numChildren; ++i) {
                Window child = children[i];

                // Check current window
                std::string windowTitle = getTitle(static_cast<WindowHandle>(child));
                if (!windowTitle.empty() && windowTitle.find(title) != std::string::npos) {
                    results.push_back(static_cast<WindowHandle>(child));
                }

                // Recursively check child windows
                searchWindowsRecursive(title, child, results);
            }

            if (children) XFree(children);
        }
    }

    Window searchWindowsByClassName(const std::string& className) {
        return searchWindowByClassRecursive(className, XDefaultRootWindow(display_));
    }

    Window searchWindowByClassRecursive(const std::string& className, Window root) {
        Window parent;
        Window* children = nullptr;
        unsigned int numChildren;

        if (XQueryTree(display_, root, &root, &parent, &children, &numChildren) == 0) {
            for (unsigned int i = 0; i < numChildren; ++i) {
                Window child = children[i];

                // Check current window class name
                XClassHint classHint;
                if (XGetClassHint(display_, child, &classHint) != 0) {
                    std::string resClass = classHint.res_class ? classHint.res_class : "";
                    std::string resName = classHint.res_name ? classHint.res_name : "";

                    if (resClass.find(className) != std::string::npos ||
                        resName.find(className) != std::string::npos) {
                        XFree(classHint.res_class);
                        XFree(classHint.res_name);
                        XFree(children);
                        return child;
                    }

                    XFree(classHint.res_class);
                    XFree(classHint.res_name);
                }

                // Recursively check child windows
                Window found = searchWindowByClassRecursive(className, child);
                if (found != None) {
                    XFree(children);
                    return found;
                }
            }

            if (children) XFree(children);
        }

        return None;
    }

    void searchWindowsByPid(uint32_t processId, Window root, std::vector<WindowHandle>& results) {
        Window parent;
        Window* children = nullptr;
        unsigned int numChildren;

        if (XQueryTree(display_, root, &root, &parent, &children, &numChildren) == 0) {
            for (unsigned int i = 0; i < numChildren; ++i) {
                Window child = children[i];

                // Check current window PID
                auto pid = getProcessId(static_cast<WindowHandle>(child));
                if (pid && pid == processId) {
                    results.push_back(static_cast<WindowHandle>(child));
                }

                // Recursively check child windows
                searchWindowsByPid(processId, child, results);
            }

            if (children) XFree(children);
        }
    }

    void enumerateWindows(Window root, std::vector<WindowInfo>& results) {
        Window parent;
        Window* children = nullptr;
        unsigned int numChildren;

        if (XQueryTree(display_, root, &root, &parent, &children, &numChildren) == 0) {
            for (unsigned int i = 0; i < numChildren; ++i) {
                Window child = children[i];

                XWindowAttributes attrs;
                if (XGetWindowAttributes(display_, child, &attrs) != 0 && attrs.map_state != IsUnmapped) {
                    WindowInfo info;
                    info.handle = static_cast<WindowHandle>(child);
                    info.title = getTitle(static_cast<WindowHandle>(child));
                    info.bounds = getBounds(static_cast<WindowHandle>(child));

                    auto pid = getProcessId(static_cast<WindowHandle>(child));
                    info.processId = pid.value_or(0);

                    info.isForeground = (child == getActiveWindow());

                    results.push_back(info);
                }

                // Recursively enumerate child windows
                enumerateWindows(child, results);
            }

            if (children) XFree(children);
        }
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
