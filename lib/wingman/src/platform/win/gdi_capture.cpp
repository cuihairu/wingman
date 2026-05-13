#include "wingman/platform/icapture.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

namespace wingman::platform::win {

/**
 * @brief Windows GDI+ 捕获实现
 *
 * 使用 GDI+ API 进行屏幕和窗口捕获。
 * 优点：兼容性好，支持所有 Windows 版本
 * 缺点：性能较低，不支持光标捕获
 */
class GDICapture : public ICapture {
public:
    GDICapture() = default;
    ~GDICapture() override {
        shutdown();
    }

    bool initialize(const CaptureConfig& config) override {
        config_ = config;
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    std::unique_ptr<Bitmap> captureScreen(int monitorIndex) override {
        if (!initialized_) {
            return nullptr;
        }

        Rect bounds = getMonitorBounds(monitorIndex);
        return captureRegion(bounds);
    }

    std::unique_ptr<Bitmap> captureRegion(const Rect& region) override {
        return captureRegion(0, region);
    }

    std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) override {
        if (!initialized_) {
            return nullptr;
        }

        Rect monitorBounds = getMonitorBounds(monitorIndex);
        Rect captureRegion = region.isEmpty() ? monitorBounds : region;

        HDC hdcScreen = GetDC(nullptr);
        if (!hdcScreen) {
            spdlog::error("[GDICapture] Failed to get screen DC");
            return nullptr;
        }

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        if (!hdcMem) {
            ReleaseDC(nullptr, hdcScreen);
            return nullptr;
        }

        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen,
            captureRegion.width, captureRegion.height);
        if (!hBitmap) {
            DeleteDC(hdcMem);
            ReleaseDC(nullptr, hdcScreen);
            return nullptr;
        }

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        BitBlt(hdcMem, 0, 0, captureRegion.width, captureRegion.height,
               hdcScreen, captureRegion.x, captureRegion.y, SRCCOPY);
        SelectObject(hdcMem, hOldBitmap);

        auto bitmap = std::make_unique<Bitmap>(captureRegion.width, captureRegion.height);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = captureRegion.width;
        bmi.bmiHeader.biHeight = -captureRegion.height;  // 自顶向下
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        GetDIBits(hdcScreen, hBitmap, 0, captureRegion.height,
                  bitmap->getData(), &bmi, DIB_RGB_COLORS);

        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);

        return bitmap;
    }

    std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) override {
        if (!initialized_ || !IsWindow(hwnd)) {
            return nullptr;
        }

        RECT rect;
        GetWindowRect(hwnd, &rect);

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        HDC hdcWindow = GetDC(hwnd);
        if (!hdcWindow) {
            return nullptr;
        }

        HDC hdcMem = CreateCompatibleDC(hdcWindow);
        if (!hdcMem) {
            ReleaseDC(hwnd, hdcWindow);
            return nullptr;
        }

        HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
        if (!hBitmap) {
            DeleteDC(hdcMem);
            ReleaseDC(hwnd, hdcWindow);
            return nullptr;
        }

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

        // 使用 PrintWindow 以捕获窗口内容（包括非客户区）
        PrintWindow(hwnd, hdcMem, 0);

        SelectObject(hdcMem, hOldBitmap);

        auto bitmap = std::make_unique<Bitmap>(width, height);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        GetDIBits(hdcWindow, hBitmap, 0, height,
                  bitmap->getData(), &bmi, DIB_RGB_COLORS);

        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWindow);

        return bitmap;
    }

    std::unique_ptr<Bitmap> captureWindowByTitle(const std::string& title) override {
        // 需要依赖 IWindow 接口来查找窗口
        // 这里暂时返回 nullptr，由工厂层协调
        return nullptr;
    }

    int getMonitorCount() override {
        return GetSystemMetrics(SM_CMONITORS);
    }

    Rect getMonitorBounds(int monitorIndex) override {
        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return Rect{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        }

        DEVMODE dm = {};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
            return Rect{0, 0, 0, 0};
        }

        // 获取监视器位置
        HMONITOR hMonitor = getMonitorFromIndex(monitorIndex);
        if (!hMonitor) {
            return Rect{0, 0, static_cast<int>(dm.dmPelsWidth), static_cast<int>(dm.dmPelsHeight)};
        }

        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoA(hMonitor, &mi);

        return Rect{
            mi.rcMonitor.left,
            mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top
        };
    }

    std::string getMonitorName(int monitorIndex) override {
        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return std::string(dd.DeviceName);
        }
        return "Monitor " + std::to_string(monitorIndex);
    }

    int getPrimaryMonitorIndex() override {
        return 0;
    }

    double getDpiScale(int monitorIndex) override {
        HMONITOR hMonitor = getMonitorFromIndex(monitorIndex);
        if (!hMonitor) {
            return 1.0;
        }

        UINT dpiX, dpiY;
        if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            return static_cast<double>(dpiX) / 96.0;
        }

        return 1.0;
    }

    std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) override {
        std::vector<DisplayMode> modes;

        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return modes;
        }

        DWORD modeIndex = 0;
        DEVMODE dm = {};
        dm.dmSize = sizeof(dm);

        while (EnumDisplaySettingsA(dd.DeviceName, modeIndex++, &dm)) {
            modes.push_back(DisplayMode{
                static_cast<int>(dm.dmPelsWidth),
                static_cast<int>(dm.dmPelsHeight),
                static_cast<int>(dm.dmDisplayFrequency),
                static_cast<int>(dm.dmBitsPerPel)
            });
        }

        return modes;
    }

    std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) override {
        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return std::nullopt;
        }

        DEVMODE dm = {};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsA(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
            return std::nullopt;
        }

        return DisplayMode{
            static_cast<int>(dm.dmPelsWidth),
            static_cast<int>(dm.dmPelsHeight),
            static_cast<int>(dm.dmDisplayFrequency),
            static_cast<int>(dm.dmBitsPerPel)
        };
    }

    bool isAvailable() const override {
        return initialized_;
    }

    std::string getBackendName() const override {
        return "GDI";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "GDI+",
            "1.0",
            initialized_,
            "Windows GDI+ Screen Capture (Compatible)"
        };
    }

    bool supportsWindowCapture() const override {
        return true;
    }

    bool supportsCursorCapture() const override {
        return false;
    }

    CaptureConfig getConfig() const override {
        return config_;
    }

    bool updateConfig(const CaptureConfig& config) override {
        config_ = config;
        return true;
    }

private:
    CaptureConfig config_;
    bool initialized_ = false;

    static HMONITOR getMonitorFromIndex(int index) {
        struct MonitorData {
            int targetIndex;
            int currentIndex;
            HMONITOR result;
        } data = {index, 0, nullptr};

        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* d = reinterpret_cast<MonitorData*>(lParam);
                if (d->currentIndex == d->targetIndex) {
                    d->result = hMonitor;
                    return FALSE;
                }
                d->currentIndex++;
                return TRUE;
            }, reinterpret_cast<LPARAM>(&data));

        return data.result;
    }
};

} // namespace wingman::platform::win
#endif // _WIN32
