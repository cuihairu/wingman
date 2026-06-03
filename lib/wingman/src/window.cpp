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

namespace wingman {

WindowHandle Window::find(const std::string& /*title*/) {
    return 0;
}

std::vector<WindowHandle> Window::findAll(const std::string& /*title*/) {
    return {};
}

WindowHandle Window::getForeground() {
    return 0;
}

std::vector<WindowInfo> Window::enumerate() {
    return {};
}

bool Window::activate(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::minimize(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::maximize(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::restore(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::close(WindowHandle /*hwnd*/) {
    return false;
}

std::string Window::getTitle(WindowHandle /*hwnd*/) {
    return "";
}

Rect Window::getBounds(WindowHandle /*hwnd*/) {
    return Rect();
}

bool Window::setBounds(WindowHandle /*hwnd*/, const Rect& /*bounds*/) {
    return false;
}

bool Window::isValid(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::isForeground(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::isVisible(WindowHandle /*hwnd*/) {
    return false;
}

bool Window::move(WindowHandle /*hwnd*/, int /*x*/, int /*y*/) {
    return false;
}

bool Window::resize(WindowHandle /*hwnd*/, int /*width*/, int /*height*/) {
    return false;
}

bool Window::waitFor(const std::string& /*title*/, int /*timeoutMs*/) {
    return false;
}

bool Window::waitClose(const std::string& /*title*/, int /*timeoutMs*/) {
    return false;
}

} // namespace wingman

#endif
