#include "wingman/platform/iscreen.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

namespace wingman::platform::win {

/**
 * @brief Windows Win32 screen management implementation
 */
class Win32Screen : public IScreen {
public:
    Win32Screen() = default;
    ~Win32Screen() override {
        shutdown();
    }

    bool initialize() override {
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    int getMonitorCount() override {
        return GetSystemMetrics(SM_CMONITORS);
    }

    int getPrimaryMonitorIndex() override {
        return 0;
    }

    Rect getPrimaryMonitorBounds() override {
        return Rect{
            0,
            0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN)
        };
    }

    Rect getMonitorBounds(int monitorIndex) override {
        struct MonitorData {
            int targetIndex;
            int currentIndex;
            Rect result;
        } data = {monitorIndex, 0, Rect{}};

        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* d = reinterpret_cast<MonitorData*>(lParam);
                if (d->currentIndex == d->targetIndex) {
                    MONITORINFO mi = {};
                    mi.cbSize = sizeof(mi);
                    GetMonitorInfoA(hMonitor, &mi);
                    d->result = Rect{
                        mi.rcMonitor.left,
                        mi.rcMonitor.top,
                        mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top
                    };
                    return FALSE;
                }
                d->currentIndex++;
                return TRUE;
            }, reinterpret_cast<LPARAM>(&data));

        return data.result;
    }

    Rect getMonitorWorkArea(int monitorIndex) override {
        struct MonitorData {
            int targetIndex;
            int currentIndex;
            Rect result;
        } data = {monitorIndex, 0, Rect{}};

        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* d = reinterpret_cast<MonitorData*>(lParam);
                if (d->currentIndex == d->targetIndex) {
                    MONITORINFO mi = {};
                    mi.cbSize = sizeof(mi);
                    GetMonitorInfoA(hMonitor, &mi);
                    d->result = Rect{
                        mi.rcWork.left,
                        mi.rcWork.top,
                        mi.rcWork.right - mi.rcWork.left,
                        mi.rcWork.bottom - mi.rcWork.top
                    };
                    return FALSE;
                }
                d->currentIndex++;
                return TRUE;
            }, reinterpret_cast<LPARAM>(&data));

        return data.result;
    }

    std::string getMonitorName(int monitorIndex) override {
        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return std::string(dd.DeviceName);
        }
        return "Monitor " + std::to_string(monitorIndex);
    }

    bool isPrimaryMonitor(int monitorIndex) override {
        return monitorIndex == 0;
    }

    double getDpiScale(int monitorIndex) override {
        HMONITOR hMonitor = getMonitorHandle(monitorIndex);
        if (!hMonitor) {
            return 1.0;
        }

        UINT dpiX, dpiY;
        if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            return static_cast<double>(dpiX) / 96.0;
        }

        // Fallback: use DC
        HDC hdc = GetDC(nullptr);
        if (hdc) {
            double dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(nullptr, hdc);
            return dpi / 96.0;
        }

        return 1.0;
    }

    int getDpi(int monitorIndex) override {
        HMONITOR hMonitor = getMonitorHandle(monitorIndex);
        if (!hMonitor) {
            return 96;
        }

        UINT dpiX, dpiY;
        if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            return static_cast<int>(dpiX);
        }

        return 96;
    }

    Point logicalToPhysical(const Point& point, int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        return Point{
            static_cast<int>(point.x * scale),
            static_cast<int>(point.y * scale)
        };
    }

    Point physicalToLogical(const Point& point, int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        return Point{
            static_cast<int>(point.x / scale),
            static_cast<int>(point.y / scale)
        };
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

    bool setDisplayMode(int monitorIndex, const DisplayMode& mode) override {
        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return false;
        }

        DEVMODE dm = {};
        dm.dmSize = sizeof(dm);
        dm.dmPelsWidth = mode.width;
        dm.dmPelsHeight = mode.height;
        dm.dmDisplayFrequency = mode.refreshRate;
        dm.dmBitsPerPel = mode.bitDepth;
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_BITSPERPEL;

        LONG result = ChangeDisplaySettingsExA(dd.DeviceName, &dm, nullptr,
                                              CDS_UPDATEREGISTRY, nullptr);
        return result == DISP_CHANGE_SUCCESSFUL;
    }

    bool resetDisplayMode(int monitorIndex) override {
        DISPLAY_DEVICE dd = {};
        dd.cb = sizeof(dd);
        if (!EnumDisplayDevicesA(nullptr, monitorIndex, &dd, 0)) {
            return false;
        }

        LONG result = ChangeDisplaySettingsExA(dd.DeviceName, nullptr, nullptr,
                                              0, nullptr);
        return result == DISP_CHANGE_SUCCESSFUL;
    }

    bool isScreenSaverRunning() override {
        BOOL isRunning;
        if (SystemParametersInfoA(SPI_GETSCREENSAVERRUNNING, 0, &isRunning, 0)) {
            return isRunning != 0;
        }
        return false;
    }

    void startScreenSaver() override {
        SendMessageA(HWND_BROADCAST, WM_SYSCOMMAND, SC_SCREENSAVE, 0);
    }

    bool isMonitorOff() override {
        // Windows does not directly support detecting display off state
        return false;
    }

    void wakeUpMonitor() override {
        // Move mouse to wake display
        POINT pt;
        GetCursorPos(&pt);
        SetCursorPos(pt.x, pt.y);
    }

    Rect getVirtualScreenBounds() override {
        return Rect{
            GetSystemMetrics(SM_XVIRTUALSCREEN),
            GetSystemMetrics(SM_YVIRTUALSCREEN),
            GetSystemMetrics(SM_CXVIRTUALSCREEN),
            GetSystemMetrics(SM_CYVIRTUALSCREEN)
        };
    }

    int getMonitorFromPoint(const Point& point) override {
        struct PointData {
            Point point;
            int result;
        } data = {point, -1};

        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* d = reinterpret_cast<PointData*>(lParam);
                MONITORINFO mi = {};
                mi.cbSize = sizeof(mi);
                GetMonitorInfoA(hMonitor, &mi);

                if (d->point.x >= mi.rcMonitor.left &&
                    d->point.x < mi.rcMonitor.right &&
                    d->point.y >= mi.rcMonitor.top &&
                    d->point.y < mi.rcMonitor.bottom) {
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&data));

        return data.result;
    }

    int getMonitorFromWindow(WindowHandle hwnd) override {
        HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

        struct MonitorData {
            HMONITOR target;
            int result;
        } data = {hMonitor, -1};

        EnumDisplayMonitors(nullptr, nullptr,
            [](HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) -> BOOL {
                auto* d = reinterpret_cast<MonitorData*>(lParam);
                if (hMonitor == d->target) {
                    return FALSE;
                }
                d->result++;
                return TRUE;
            }, reinterpret_cast<LPARAM>(&data));

        return data.result;
    }

    std::string getBackendName() const override {
        return "Win32";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "Win32",
            "1.0",
            initialized_,
            "Windows Win32 Screen API"
        };
    }

private:
    bool initialized_ = false;

    static HMONITOR getMonitorHandle(int index) {
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

namespace wingman::platform {

std::unique_ptr<IScreen> createPlatformScreen() {
    auto screen = std::unique_ptr<IScreen>(new win::Win32Screen());
    screen->initialize();
    return screen;
}

} // namespace wingman::platform
#endif // _WIN32
