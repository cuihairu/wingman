#include "wingman/platform/iwindow.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <chrono>
#include <thread>
#include <cstring>

namespace wingman::platform::win {

/**
 * @brief Windows Win32 window management implementation
 */
class Win32Window : public IWindow {
public:
    Win32Window() = default;
    ~Win32Window() override {
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
        EnumWindowsData data;
        data.title = title;
        EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&data));

        return data.results.empty() ? nullptr : data.results[0];
    }

    std::vector<WindowHandle> findAll(const std::string& title) override {
        EnumWindowsData data;
        data.title = title;
        EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&data));
        return data.results;
    }

    WindowHandle findByClassName(const std::string& className) override {
        HWND hwnd = FindWindowA(className.c_str(), nullptr);
        return hwnd;
    }

    std::vector<WindowHandle> findByProcessId(uint32_t processId) override {
        EnumWindowsData data;
        data.processId = processId;
        EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&data));
        return data.results;
    }

    std::vector<WindowInfo> enumerate() override {
        EnumWindowsData data;
        data.collectInfo = true;
        EnumWindows(enumWindowsCallback, reinterpret_cast<LPARAM>(&data));
        return data.windowInfos;
    }

    WindowHandle getForeground() override {
        return GetForegroundWindow();
    }

    bool activate(WindowHandle hwnd) override {
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

    bool minimize(WindowHandle hwnd) override {
        return ShowWindow(hwnd, SW_MINIMIZE) != 0;
    }

    bool maximize(WindowHandle hwnd) override {
        return ShowWindow(hwnd, SW_MAXIMIZE) != 0;
    }

    bool restore(WindowHandle hwnd) override {
        return ShowWindow(hwnd, SW_RESTORE) != 0;
    }

    bool close(WindowHandle hwnd) override {
        return PostMessage(hwnd, WM_CLOSE, 0, 0) != 0;
    }

    bool forceClose(WindowHandle hwnd) override {
        return PostMessage(hwnd, WM_DESTROY, 0, 0) != 0;
    }

    bool hide(WindowHandle hwnd) override {
        return ShowWindow(hwnd, SW_HIDE) != 0;
    }

    bool show(WindowHandle hwnd) override {
        return ShowWindow(hwnd, SW_SHOW) != 0;
    }

    std::string getTitle(WindowHandle hwnd) override {
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

    Rect getBounds(WindowHandle hwnd) override {
        RECT rect = {};
        GetWindowRect(hwnd, &rect);
        return Rect(rect.left, rect.top,
                    rect.right - rect.left,
                    rect.bottom - rect.top);
    }

    bool setBounds(WindowHandle hwnd, const Rect& bounds) override {
        return SetWindowPos(hwnd, nullptr,
                           bounds.x, bounds.y,
                           bounds.width, bounds.height,
                           SWP_NOZORDER) != 0;
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
        if (!isValid(hwnd)) {
            return false;
        }

        Rect windowBounds = getBounds(hwnd);

        // Get display bounds
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);

        Rect workArea{
            mi.rcWork.left,
            mi.rcWork.top,
            mi.rcWork.right - mi.rcWork.left,
            mi.rcWork.bottom - mi.rcWork.top
        };

        int x = workArea.x + (workArea.width - windowBounds.width) / 2;
        int y = workArea.y + (workArea.height - windowBounds.height) / 2;

        return move(hwnd, x, y);
    }

    bool isValid(WindowHandle hwnd) override {
        return IsWindow(hwnd) != 0;
    }

    bool isVisible(WindowHandle hwnd) override {
        return IsWindowVisible(hwnd) != 0;
    }

    bool isForeground(WindowHandle hwnd) override {
        return GetForegroundWindow() == hwnd;
    }

    bool isMinimized(WindowHandle hwnd) override {
        return IsIconic(hwnd) != 0;
    }

    bool isMaximized(WindowHandle hwnd) override {
        WINDOWPLACEMENT wp = {};
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd, &wp);
        return wp.showCmd == SW_SHOWMAXIMIZED;
    }

    std::optional<uint32_t> getProcessId(WindowHandle hwnd) override {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        return pid;
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
        return "Win32";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Win32",
            "1.0",
            initialized_,
            "Windows Win32 Window API"
        };
    }

private:
    bool initialized_ = false;

    struct EnumWindowsData {
        std::string title;
        uint32_t processId = 0;
        std::vector<WindowHandle> results;
        std::vector<WindowInfo> windowInfos;
        bool collectInfo = false;
    };

    static BOOL CALLBACK enumWindowsCallback(HWND hwnd, LPARAM lParam) {
        auto* data = reinterpret_cast<EnumWindowsData*>(lParam);

        if (!IsWindowVisible(hwnd)) {
            return TRUE;
        }

        // Filter by process ID
        if (data->processId != 0) {
            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != data->processId) {
                return TRUE;
            }
        }

        // Filter by title
        if (!data->title.empty()) {
            wchar_t titleBuf[512];
            int len = GetWindowTextW(hwnd, titleBuf, 512);
            if (len == 0) {
                return TRUE;
            }

            char titleUtf8[1024];
            WideCharToMultiByte(CP_UTF8, 0, titleBuf, -1,
                               titleUtf8, 1024, nullptr, nullptr);
            std::string title(titleUtf8);

            if (title.find(data->title) == std::string::npos) {
                return TRUE;
            }
        }

        data->results.push_back(hwnd);

        if (data->collectInfo) {
            WindowInfo info;
            info.handle = hwnd;
            info.title = data->collectInfo ?
                (std::unique_ptr<Win32Window>(new Win32Window())->getTitle(hwnd)) : "";
            info.bounds = data->collectInfo ?
                (std::unique_ptr<Win32Window>(new Win32Window())->getBounds(hwnd)) : Rect{};
            info.isForeground = (GetForegroundWindow() == hwnd);
            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);
            info.processId = pid;
            data->windowInfos.push_back(info);
        }

        return TRUE;
    }
};

} // namespace wingman::platform::win
#endif // _WIN32
