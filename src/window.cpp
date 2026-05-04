#include "wingman/window.hpp"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif

#include <cstring>
#include <algorithm>

namespace wingman {

// ============================================================================
// Windows 实现
// ============================================================================

#ifdef _WIN32

// 用于枚举窗口的数据结构
struct EnumWindowsData {
    std::string title;
    std::vector<WindowHandle> results;
    std::vector<WindowInfo>* windowInfos;
};

BOOL CALLBACK Window::enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<EnumWindowsData*>(lParam);

    // 检查窗口是否可见
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    // 获取窗口标题
    wchar_t titleBuf[512];
    int len = GetWindowTextW(hwnd, titleBuf, 512);
    if (len == 0) {
        return TRUE;
    }

    // 转换为 UTF-8
    char titleUtf8[1024];
    WideCharToMultiByte(CP_UTF8, 0, titleBuf, -1,
                       titleUtf8, 1024, nullptr, nullptr);
    std::string title(titleUtf8);

    // 检查是否匹配搜索标题
    if (!data->title.empty()) {
        if (title.find(data->title) == std::string::npos) {
            return TRUE;
        }
    }

    // 保存结果
    data->results.push_back(hwnd);

    if (data->windowInfos) {
        WindowInfo info;
        info.handle = hwnd;
        info.title = title;
        info.bounds = getBounds(hwnd);
        info.isForeground = (GetForegroundWindow() == hwnd);
        data->windowInfos->push_back(info);
    }

    return TRUE;
}

WindowHandle Window::find(const std::string& title) {
    EnumWindowsData data;
    data.title = title;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));

    return data.results.empty() ? nullptr : data.results[0];
}

std::vector<WindowHandle> Window::findAll(const std::string& title) {
    EnumWindowsData data;
    data.title = title;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return data.results;
}

WindowHandle Window::getForeground() {
    return GetForegroundWindow();
}

std::vector<WindowInfo> Window::enumerate() {
    EnumWindowsData data;
    std::vector<WindowInfo> infos;
    data.windowInfos = &infos;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));
    return infos;
}

bool Window::activate(WindowHandle hwnd) {
    if (!isValid(hwnd)) {
        return false;
    }

    // 检查是否已经最小化
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    // 附加到当前线程的输入处理
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    AttachThreadInput(GetCurrentThreadId(), threadId, TRUE);

    // 显示窗口
    ShowWindow(hwnd, SW_SHOW);

    // 设置为前台窗口
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    // 分离输入处理
    AttachThreadInput(GetCurrentThreadId(), threadId, FALSE);

    return true;
}

bool Window::minimize(WindowHandle hwnd) {
    return ShowWindow(hwnd, SW_MINIMIZE) != 0;
}

bool Window::maximize(WindowHandle hwnd) {
    return ShowWindow(hwnd, SW_MAXIMIZE) != 0;
}

bool Window::restore(WindowHandle hwnd) {
    return ShowWindow(hwnd, SW_RESTORE) != 0;
}

bool Window::close(WindowHandle hwnd) {
    return PostMessage(hwnd, WM_CLOSE, 0, 0) != 0;
}

std::string Window::getTitle(WindowHandle hwnd) {
    if (!isValid(hwnd)) {
        return "";
    }

    wchar_t titleBuf[512];
    int len = GetWindowTextW(hwnd, titleBuf, 512);

    char titleUtf8[1024];
    WideCharToMultiByte(CP_UTF8, 0, titleBuf, -1,
                       titleUtf8, 1024, nullptr, nullptr);

    return std::string(titleUtf8);
}

Rect Window::getBounds(WindowHandle hwnd) {
    RECT rect = {};
    GetWindowRect(hwnd, &rect);
    return Rect(rect.left, rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top);
}

bool Window::setBounds(WindowHandle hwnd, const Rect& bounds) {
    return SetWindowPos(hwnd, nullptr,
                       bounds.x, bounds.y,
                       bounds.width, bounds.height,
                       SWP_NOZORDER) != 0;
}

bool Window::isValid(WindowHandle hwnd) {
    return IsWindow(hwnd) != 0;
}

bool Window::isForeground(WindowHandle hwnd) {
    return GetForegroundWindow() == hwnd;
}

bool Window::isVisible(WindowHandle hwnd) {
    return IsWindowVisible(hwnd) != 0;
}

bool Window::move(WindowHandle hwnd, int x, int y) {
    Rect bounds = getBounds(hwnd);
    bounds.x = x;
    bounds.y = y;
    return setBounds(hwnd, bounds);
}

bool Window::resize(WindowHandle hwnd, int width, int height) {
    Rect bounds = getBounds(hwnd);
    bounds.width = width;
    bounds.height = height;
    return setBounds(hwnd, bounds);
}

bool Window::waitFor(const std::string& title, int timeoutMs) {
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

bool Window::waitClose(const std::string& title, int timeoutMs) {
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

// ============================================================================
// Linux/X11 实现
// ============================================================================

#else

static Display* getWindowDisplay() {
    static Display* display = nullptr;
    if (!display) {
        display = XOpenDisplay(nullptr);
    }
    return display;
}

WindowHandle Window::find(const std::string& title) {
    auto results = findAll(title);
    return results.empty() ? None : results[0];
}

std::vector<WindowHandle> Window::findAll(const std::string& title) {
    std::vector<WindowHandle> results;
    Display* display = getWindowDisplay();
    if (!display) {
        return results;
    }

    Window root = DefaultRootWindow(display);
    Atom wmNameAtom = XInternAtom(display, "WM_NAME", False);
    Atom netWmNameAtom = XInternAtom(display, "_NET_WM_NAME", False);

    // 递归搜索窗口
    std::vector<Window> windows;
    windows.push_back(root);

    while (!windows.empty()) {
        Window w = windows.back();
        windows.pop_back();

        Window root_return, parent_return;
        Window* children = nullptr;
        unsigned int nchildren;

        if (XQueryTree(display, w, &root_return, &parent_return,
                      &children, &nchildren) == 0) {
            if (children) {
                XFree(children);
            }
            continue;
        }

        for (unsigned int i = 0; i < nchildren; ++i) {
            Window child = children[i];

            // 获取窗口标题
            Atom actualType;
            int actualFormat;
            unsigned long nitems, bytesAfter;
            unsigned char* prop = nullptr;

            // 首先尝试 _NET_WM_NAME (UTF-8)
            if (XGetWindowProperty(display, child, netWmNameAtom, 0, 1024,
                                 False, AnyPropertyType, &actualType,
                                 &actualFormat, &nitems, &bytesAfter,
                                 &prop) == Success && prop) {
                std::string windowTitle(reinterpret_cast<char*>(prop));
                XFree(prop);
                if (windowTitle.find(title) != std::string::npos) {
                    results.push_back(child);
                }
            } else if (XGetWindowProperty(display, child, wmNameAtom, 0, 1024,
                                        False, AnyPropertyType, &actualType,
                                        &actualFormat, &nitems, &bytesAfter,
                                        &prop) == Success && prop) {
                std::string windowTitle(reinterpret_cast<char*>(prop));
                XFree(prop);
                if (windowTitle.find(title) != std::string::npos) {
                    results.push_back(child);
                }
            }

            windows.push_back(child);
        }

        XFree(children);
    }

    return results;
}

WindowHandle Window::getForeground() {
    Display* display = getWindowDisplay();
    if (!display) {
        return None;
    }

    Window focusedWindow = None;
    int revertTo;
    XGetInputFocus(display, &focusedWindow, &revertTo);
    return focusedWindow;
}

std::vector<WindowInfo> Window::enumerate() {
    std::vector<WindowInfo> results;
    // TODO: 实现窗口枚举
    return results;
}

bool Window::activate(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    // 映射并提升窗口
    XMapRaised(display, hwnd);
    XSetInputFocus(display, hwnd, RevertToParent, CurrentTime);
    XFlush(display);
    return true;
}

bool Window::minimize(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XIconifyWindow(display, hwnd, DefaultScreen(display));
    XFlush(display);
    return true;
}

bool Window::maximize(WindowHandle hwnd) {
    // TODO: 实现窗口最大化
    return false;
}

bool Window::restore(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XMapWindow(display, hwnd);
    XFlush(display);
    return true;
}

bool Window::close(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = hwnd;
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", True);
    event.xclient.format = 32;
    event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
    event.xclient.data.l[1] = CurrentTime;

    XSendEvent(display, hwnd, False, NoEventMask, &event);
    XFlush(display);
    return true;
}

std::string Window::getTitle(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return "";
    }

    Atom netWmNameAtom = XInternAtom(display, "_NET_WM_NAME", False);
    Atom actualType;
    int actualFormat;
    unsigned long nitems, bytesAfter;
    unsigned char* prop = nullptr;

    if (XGetWindowProperty(display, hwnd, netWmNameAtom, 0, 1024,
                         False, AnyPropertyType, &actualType,
                         &actualFormat, &nitems, &bytesAfter,
                         &prop) == Success && prop) {
        std::string title(reinterpret_cast<char*>(prop));
        XFree(prop);
        return title;
    }

    return "";
}

Rect Window::getBounds(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return Rect();
    }

    XWindowAttributes attr;
    XGetWindowAttributes(display, hwnd, &attr);
    int screen = DefaultScreen(display);

    Window root;
    int x, y;
    unsigned int width, height, border_width, depth;
    XGetGeometry(display, hwnd, &root, &x, &y, &width, &height,
                 &border_width, &depth);

    return Rect(x, y, static_cast<int>(width), static_cast<int>(height));
}

bool Window::setBounds(WindowHandle hwnd, const Rect& bounds) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XMoveResizeWindow(display, hwnd, bounds.x, bounds.y,
                     bounds.width, bounds.height);
    XFlush(display);
    return true;
}

bool Window::isValid(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XWindowAttributes attr;
    return XGetWindowAttributes(display, hwnd, &attr) != 0;
}

bool Window::isForeground(WindowHandle hwnd) {
    return getForeground() == hwnd;
}

bool Window::isVisible(WindowHandle hwnd) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XWindowAttributes attr;
    if (XGetWindowAttributes(display, hwnd, &attr) == 0) {
        return false;
    }

    return attr.map_state == IsViewable;
}

bool Window::move(WindowHandle hwnd, int x, int y) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XMoveWindow(display, hwnd, x, y);
    XFlush(display);
    return true;
}

bool Window::resize(WindowHandle hwnd, int width, int height) {
    Display* display = getWindowDisplay();
    if (!display || hwnd == None) {
        return false;
    }

    XResizeWindow(display, hwnd, width, height);
    XFlush(display);
    return true;
}

bool Window::waitFor(const std::string& title, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(title) != None) {
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

bool Window::waitClose(const std::string& title, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (find(title) == None) {
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

#endif

} // namespace wingman
