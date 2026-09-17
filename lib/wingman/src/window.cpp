#include "wingman/window.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <chrono>
#include <thread>

// ============================================================================
// Data structure and callback for window enumeration
// ============================================================================

namespace {

struct EnumWindowsData {
    std::string title;
    std::vector<wingman::WindowHandle> results;
    std::vector<wingman::WindowInfo>* windowInfos;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* data = reinterpret_cast<EnumWindowsData*>(lParam);

    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }

    wchar_t titleBuf[512];
    int len = GetWindowTextW(hwnd, titleBuf, 512);
    if (len == 0) {
        return TRUE;
    }

    char titleUtf8[1024];
    WideCharToMultiByte(CP_UTF8, 0, titleBuf, -1,
                       titleUtf8, 1024, nullptr, nullptr);
    std::string title(titleUtf8);

    if (!data->title.empty()) {
        if (title.find(data->title) == std::string::npos) {
            return TRUE;
        }
    }

    data->results.push_back(hwnd);

    if (data->windowInfos) {
        wingman::WindowInfo info;
        info.handle = hwnd;
        info.title = title;
        info.bounds = wingman::Window::getBounds(hwnd);
        info.isForeground = (GetForegroundWindow() == hwnd);
        data->windowInfos->push_back(info);
    }

    return TRUE;
}

}  // anonymous namespace

namespace wingman {

// ============================================================================
// Window implementation
// ============================================================================

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

    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    AttachThreadInput(GetCurrentThreadId(), threadId, TRUE);

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

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
    GetWindowTextW(hwnd, titleBuf, 512);

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

} // namespace wingman

#endif // _WIN32

#ifndef _WIN32

#include "wingman/platform/iwindow.hpp"
#include <memory>

// 平台窗口后端工厂接线（前向声明消费，无公开头文件，同 clipboard.cpp 模式）。
// Linux 2026-09-15、macOS 2026-09-16 接线；此前两平台此分支恒空 stub，
// X11Window/CocoaWindow 有完整实现却全库零消费者（装配断链）。其余平台
// windowBackend 恒空，转发方法的空守卫自然给出 stub 语义。
#if defined(__linux__)
// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间限定需要 undo（同 x11_factory.cpp）
#if defined(linux)
#undef linux
#endif

namespace wingman::platform::linux {
std::unique_ptr<IWindow> createX11Window();
}
#elif defined(__APPLE__)
namespace wingman::platform::mac {
std::unique_ptr<IWindow> createCocoaWindow();
}
#endif

namespace wingman {

namespace {

// 每次调用经工厂独立创建 IWindow（自带平台连接），同 screen.cpp 先例：规避跨
// 线程共享 Display/连接的线程安全问题；连接建立走本地通道，开销亚毫秒。
// 工厂内部已 initialize()，环境缺失时方法随 !initialized_ 路径优雅返回
// 空值，与原 stub 语义一致。
std::unique_ptr<platform::IWindow> windowBackend() {
#if defined(__linux__)
    return platform::linux::createX11Window();
#elif defined(__APPLE__)
    return platform::mac::createCocoaWindow();
#else
    return nullptr;
#endif
}

} // namespace

WindowHandle Window::find(const std::string& title) {
    auto backend = windowBackend();
    return backend ? backend->find(title) : 0;
}

std::vector<WindowHandle> Window::findAll(const std::string& title) {
    auto backend = windowBackend();
    return backend ? backend->findAll(title) : std::vector<WindowHandle>{};
}

WindowHandle Window::getForeground() {
    auto backend = windowBackend();
    return backend ? backend->getForeground() : 0;
}

std::vector<WindowInfo> Window::enumerate() {
    auto backend = windowBackend();
    std::vector<WindowInfo> result;
    if (!backend) {
        return result;
    }
    // 语义对齐 Windows 分支：只列用户可见的顶层窗口（Windows 走
    // IsWindowVisible 过滤；X11 侧 _NET_CLIENT_LIST 亦含最小化/隐藏窗口）
    for (auto& info : backend->enumerate()) {
        if (!backend->isVisible(info.handle)) {
            continue;
        }
        WindowInfo wi;
        wi.handle = info.handle;
        wi.title = info.title;
        wi.bounds = Rect(info.bounds.x, info.bounds.y,
                         info.bounds.width, info.bounds.height);
        wi.isForeground = info.isForeground;
        result.push_back(wi);
    }
    return result;
}

// 写操作（activate/close/move/...）统一前置 isValid：Windows 分支各 API 对
// 无效句柄天然返回失败，X11 侧 BadWindow 是异步 error（函数本体恒 true），
// 归一为 false 避免脚本拿到假成功。

bool Window::activate(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->activate(hwnd);
}

bool Window::minimize(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->minimize(hwnd);
}

bool Window::maximize(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->maximize(hwnd);
}

bool Window::restore(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->restore(hwnd);
}

bool Window::close(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->close(hwnd);
}

std::string Window::getTitle(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend ? backend->getTitle(hwnd) : std::string{};
}

Rect Window::getBounds(WindowHandle hwnd) {
    auto backend = windowBackend();
    if (!backend) {
        return Rect();
    }
    const auto bounds = backend->getBounds(hwnd);
    return Rect(bounds.x, bounds.y, bounds.width, bounds.height);
}

bool Window::setBounds(WindowHandle hwnd, const Rect& bounds) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->setBounds(
        hwnd, platform::Rect{bounds.x, bounds.y, bounds.width, bounds.height});
}

bool Window::isValid(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd);
}

bool Window::isForeground(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isForeground(hwnd);
}

bool Window::isVisible(WindowHandle hwnd) {
    auto backend = windowBackend();
    return backend && backend->isVisible(hwnd);
}

bool Window::move(WindowHandle hwnd, int x, int y) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->move(hwnd, x, y);
}

bool Window::resize(WindowHandle hwnd, int width, int height) {
    auto backend = windowBackend();
    return backend && backend->isValid(hwnd) && backend->resize(hwnd, width, height);
}

bool Window::waitFor(const std::string& title, int timeoutMs) {
    auto backend = windowBackend();
    return backend && backend->waitFor(title, timeoutMs);
}

bool Window::waitClose(const std::string& title, int timeoutMs) {
    auto backend = windowBackend();
    return backend && backend->waitClose(title, timeoutMs);
}

} // namespace wingman

#endif
