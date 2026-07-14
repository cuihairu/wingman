#ifdef __linux__

#include "wingman/platform/iwindow.hpp"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <cstring>

namespace wingman::platform::linux {

class X11Window : public IWindow {
public:
    X11Window() = default;
    ~X11Window() override { shutdown(); }

    bool initialize() override {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("X11Window: failed to open X display");
            return false;
        }
        root_ = DefaultRootWindow(display_);
        netWmName_ = XInternAtom(display_, "_NET_WM_NAME", False);
        utf8String_ = XInternAtom(display_, "UTF8_STRING", False);
        netWmPid_ = XInternAtom(display_, "_NET_WM_PID", False);
        wmName_ = XInternAtom(display_, "WM_NAME", False);
        wmState_ = XInternAtom(display_, "WM_STATE", False);
        netWmState_ = XInternAtom(display_, "_NET_WM_STATE", False);
        netWmStateHidden_ = XInternAtom(display_, "_NET_WM_STATE_HIDDEN", False);
        netWmStateMaximizedH_ = XInternAtom(display_, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        netActiveWindow_ = XInternAtom(display_, "_NET_ACTIVE_WINDOW", False);
        netClientList_ = XInternAtom(display_, "_NET_CLIENT_LIST", False);
        wmDeleteWindow_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (display_) { XCloseDisplay(display_); display_ = nullptr; }
        initialized_ = false;
    }

    WindowHandle find(const std::string& title) override {
        if (!initialized_) return NullWindowHandle;
        auto all = enumerate();
        for (auto& info : all) {
            if (info.title.find(title) != std::string::npos)
                return info.handle;
        }
        return NullWindowHandle;
    }

    std::vector<WindowHandle> findAll(const std::string& title) override {
        std::vector<WindowHandle> result;
        if (!initialized_) return result;
        auto all = enumerate();
        for (auto& info : all) {
            if (info.title.find(title) != std::string::npos)
                result.push_back(info.handle);
        }
        return result;
    }

    WindowHandle findByClassName(const std::string& className) override {
        if (!initialized_) return NullWindowHandle;
        auto all = enumerate();
        for (auto& info : all) {
            XClassHint hint;
            if (XGetClassHint(display_, info.handle, &hint)) {
                bool match = (className == hint.res_name || className == hint.res_class);
                XFree(hint.res_name); XFree(hint.res_class);
                if (match) return info.handle;
            }
        }
        return NullWindowHandle;
    }

    std::vector<WindowHandle> findByProcessId(uint32_t processId) override {
        std::vector<WindowHandle> result;
        if (!initialized_) return result;
        auto all = enumerate();
        for (auto& info : all) {
            if (info.processId == processId)
                result.push_back(info.handle);
        }
        return result;
    }

    std::vector<WindowInfo> enumerate() override {
        std::vector<WindowInfo> result;
        if (!initialized_) return result;

        Atom actualType;
        int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;

        if (XGetWindowProperty(display_, root_, netClientList_, 0, 1024, False,
                               XA_WINDOW, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            auto* windows = reinterpret_cast<Window*>(data);
            for (unsigned long i = 0; i < nItems; i++) {
                WindowInfo info;
                info.handle = windows[i];
                info.title = getTitle(windows[i]);
                info.bounds = getBounds(windows[i]);
                info.processId = getPid(windows[i]);
                result.push_back(info);
            }
            XFree(data);
        }
        return result;
    }

    WindowHandle getForeground() override {
        if (!initialized_) return NullWindowHandle;
        Atom actualType;
        int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        Window focused = 0;
        if (XGetWindowProperty(display_, root_, netActiveWindow_, 0, 1, False,
                               XA_WINDOW, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            focused = *reinterpret_cast<Window*>(data);
            XFree(data);
        }
        return focused;
    }

    bool activate(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.serial = 0;
        ev.xclient.send_event = True;
        ev.xclient.display = display_;
        ev.xclient.window = static_cast<Window>(hwnd);
        ev.xclient.message_type = netActiveWindow_;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 2; // source indication
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(display_, root_, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        XRaiseWindow(display_, static_cast<Window>(hwnd));
        XSetInputFocus(display_, static_cast<Window>(hwnd), RevertToParent, CurrentTime);
        XFlush(display_);
        return true;
    }

    bool minimize(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XIconifyWindow(display_, static_cast<Window>(hwnd), DefaultScreen(display_));
        XFlush(display_);
        return true;
    }

    bool maximize(WindowHandle hwnd) override {
        if (!initialized_) return false;
        sendStateMessage(hwnd, netWmState_, 1, netWmStateMaximizedH_);
        XFlush(display_);
        return true;
    }

    bool restore(WindowHandle hwnd) override {
        if (!initialized_) return false;
        sendStateMessage(hwnd, netWmState_, 0, netWmStateMaximizedH_);
        XFlush(display_);
        return true;
    }

    bool close(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.serial = 0;
        ev.xclient.send_event = True;
        ev.xclient.display = display_;
        ev.xclient.window = static_cast<Window>(hwnd);
        ev.xclient.message_type = XInternAtom(display_, "WM_PROTOCOLS", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = wmDeleteWindow_;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(display_, static_cast<Window>(hwnd), False, NoEventMask, &ev);
        XFlush(display_);
        return true;
    }

    bool forceClose(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XKillClient(display_, static_cast<Window>(hwnd));
        XFlush(display_);
        return true;
    }

    bool hide(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XUnmapWindow(display_, static_cast<Window>(hwnd));
        XFlush(display_);
        return true;
    }

    bool show(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XMapWindow(display_, static_cast<Window>(hwnd));
        XFlush(display_);
        return true;
    }

    std::string getTitle(WindowHandle hwnd) override {
        if (!initialized_) return "";
        // Try _NET_WM_NAME (UTF-8)
        Atom actualType; int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(display_, static_cast<Window>(hwnd), netWmName_, 0, 1024,
                               False, utf8String_, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            std::string title(reinterpret_cast<char*>(data), nItems);
            XFree(data);
            return title;
        }
        // Fallback to WM_NAME
        char* name = nullptr;
        if (XFetchName(display_, static_cast<Window>(hwnd), &name) && name) {
            std::string title(name);
            XFree(name);
            return title;
        }
        return "";
    }

    Rect getBounds(WindowHandle hwnd) override {
        if (!initialized_) return {};
        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, static_cast<Window>(hwnd), &attrs) == 0)
            return {};
        int x, y;
        Window child;
        XTranslateCoordinates(display_, static_cast<Window>(hwnd), root_,
                              0, 0, &x, &y, &child);
        return {x, y, attrs.width, attrs.height};
    }

    bool setBounds(WindowHandle hwnd, const Rect& bounds) override {
        if (!initialized_) return false;
        XMoveResizeWindow(display_, static_cast<Window>(hwnd),
                          bounds.x, bounds.y, bounds.width, bounds.height);
        XFlush(display_);
        return true;
    }

    bool move(WindowHandle hwnd, int x, int y) override {
        if (!initialized_) return false;
        XMoveWindow(display_, static_cast<Window>(hwnd), x, y);
        XFlush(display_);
        return true;
    }

    bool resize(WindowHandle hwnd, int width, int height) override {
        if (!initialized_) return false;
        XResizeWindow(display_, static_cast<Window>(hwnd), width, height);
        XFlush(display_);
        return true;
    }

    bool center(WindowHandle hwnd, int monitorIndex) override {
        if (!initialized_) return false;
        // Simplified: center on screen
        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, static_cast<Window>(hwnd), &attrs) == 0)
            return false;
        ::Screen* s = DefaultScreenOfDisplay(display_);
        int x = (s->width - attrs.width) / 2;
        int y = (s->height - attrs.height) / 2;
        XMoveWindow(display_, static_cast<Window>(hwnd), x, y);
        XFlush(display_);
        return true;
    }

    bool isValid(WindowHandle hwnd) override {
        if (!initialized_ || hwnd == 0) return false;
        XWindowAttributes attrs;
        return XGetWindowAttributes(display_, static_cast<Window>(hwnd), &attrs) != 0;
    }

    bool isVisible(WindowHandle hwnd) override {
        if (!initialized_) return false;
        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, static_cast<Window>(hwnd), &attrs) == 0)
            return false;
        return attrs.map_state == IsViewable;
    }

    bool isForeground(WindowHandle hwnd) override {
        return getForeground() == hwnd;
    }

    bool isMinimized(WindowHandle hwnd) override {
        if (!initialized_) return false;
        return hasState(hwnd, netWmStateHidden_);
    }

    bool isMaximized(WindowHandle hwnd) override {
        if (!initialized_) return false;
        return hasState(hwnd, netWmStateMaximizedH_);
    }

    std::optional<uint32_t> getProcessId(WindowHandle hwnd) override {
        if (!initialized_) return std::nullopt;
        uint32_t pid = getPid(hwnd);
        return pid > 0 ? std::make_optional(pid) : std::nullopt;
    }

    bool waitFor(const std::string& title, int timeoutMs) override {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (find(title) != NullWindowHandle) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    bool waitClose(const std::string& title, int timeoutMs) override {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (find(title) == NullWindowHandle) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    bool waitForForeground(WindowHandle hwnd, int timeoutMs) override {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (isForeground(hwnd)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    std::string getBackendName() const override { return "X11"; }
    BackendInfo getBackendInfo() const override {
        return {"X11", "1.0", initialized_, "X11 Window Management"};
    }

private:
    Display* display_ = nullptr;
    Window root_ = 0;
    bool initialized_ = false;
    Atom netWmName_, utf8String_, netWmPid_, wmName_, wmState_;
    Atom netWmState_, netWmStateHidden_, netWmStateMaximizedH_;
    Atom netActiveWindow_, netClientList_, wmDeleteWindow_;

    uint32_t getPid(Window w) {
        Atom actualType; int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        uint32_t pid = 0;
        if (XGetWindowProperty(display_, w, netWmPid_, 0, 1, False,
                               XA_CARDINAL, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            pid = *reinterpret_cast<uint32_t*>(data);
            XFree(data);
        }
        return pid;
    }

    bool hasState(Window w, Atom stateAtom) {
        Atom actualType; int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        bool found = false;
        if (XGetWindowProperty(display_, w, netWmState_, 0, 1024, False,
                               XA_ATOM, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            auto* atoms = reinterpret_cast<Atom*>(data);
            for (unsigned long i = 0; i < nItems; i++) {
                if (atoms[i] == stateAtom) { found = true; break; }
            }
            XFree(data);
        }
        return found;
    }

    void sendStateMessage(WindowHandle hwnd, Atom message, long action, Atom atom) {
        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.serial = 0;
        ev.xclient.send_event = True;
        ev.xclient.display = display_;
        ev.xclient.window = static_cast<Window>(hwnd);
        ev.xclient.message_type = message;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = action;
        ev.xclient.data.l[1] = atom;
        XSendEvent(display_, root_, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
