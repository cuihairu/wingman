#ifdef __linux__

#include "wingman/platform/iscreen.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace wingman::platform::linux {

class X11Screen : public IScreen {
public:
    X11Screen() = default;
    ~X11Screen() override { shutdown(); }

    bool initialize() override {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("X11Screen: failed to open X display");
            return false;
        }
        root_ = DefaultRootWindow(display_);
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

    int getMonitorCount() override {
        if (!initialized_) return 1;
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (monitors) XRRFreeMonitors(monitors);
        return n > 0 ? n : 1;
    }

    int getPrimaryMonitorIndex() override {
        if (!initialized_) return 0;
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (!monitors) return 0;
        for (int i = 0; i < n; i++) {
            if (monitors[i].primary) {
                XRRFreeMonitors(monitors);
                return i;
            }
        }
        XRRFreeMonitors(monitors);
        return 0;
    }

    Rect getPrimaryMonitorBounds() override {
        return getMonitorBounds(getPrimaryMonitorIndex());
    }

    Rect getMonitorBounds(int monitorIndex) override {
        if (!initialized_) return {0, 0, 1920, 1080};
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (!monitors || monitorIndex >= n) {
            if (monitors) XRRFreeMonitors(monitors);
            Screen* screen = DefaultScreenOfDisplay(display_);
            return {0, 0, screen->width, screen->height};
        }
        auto& m = monitors[monitorIndex];
        Rect r{m.x, m.y, m.width, m.height};
        XRRFreeMonitors(monitors);
        return r;
    }

    Rect getMonitorWorkArea(int monitorIndex) override {
        // XRandR doesn't directly provide work area; return full bounds
        return getMonitorBounds(monitorIndex);
    }

    std::string getMonitorName(int monitorIndex) override {
        if (!initialized_) return "Monitor " + std::to_string(monitorIndex);
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (!monitors || monitorIndex >= n) {
            if (monitors) XRRFreeMonitors(monitors);
            return "Monitor " + std::to_string(monitorIndex);
        }
        char* name = XGetAtomName(display_, monitors[monitorIndex].name);
        std::string result = name ? name : "Monitor " + std::to_string(monitorIndex);
        if (name) XFree(name);
        XRRFreeMonitors(monitors);
        return result;
    }

    bool isPrimaryMonitor(int monitorIndex) override {
        return monitorIndex == getPrimaryMonitorIndex();
    }

    double getDpiScale(int /*monitorIndex*/) override {
        // Default to 1.0; X11 DPI varies
        if (!initialized_) return 1.0;
        int dpi = getDpi(0);
        return dpi / 96.0;
    }

    int getDpi(int /*monitorIndex*/) override {
        if (!initialized_) return 96;
        // Try Xft.dpi resource
        char* resource = XResourceManagerString(display_);
        if (resource) {
            XrmDatabase db = XrmGetStringDatabase(resource);
            if (db) {
                XrmValue value;
                char* type = nullptr;
                if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
                    int dpi = atoi(value.addr);
                    XrmDestroyDatabase(db);
                    return dpi > 0 ? dpi : 96;
                }
                XrmDestroyDatabase(db);
            }
        }
        // Fallback: calculate from screen size
        Screen* s = DefaultScreenOfDisplay(display_);
        double mmWidth = DisplayWidthMM(display_, DefaultScreen(display_));
        if (mmWidth > 0) {
            return static_cast<int>(s->width * 25.4 / mmWidth);
        }
        return 96;
    }

    Point logicalToPhysical(const Point& point, int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        return {static_cast<int>(point.x * scale), static_cast<int>(point.y * scale)};
    }

    Point physicalToLogical(const Point& point, int monitorIndex) override {
        double scale = getDpiScale(monitorIndex);
        if (scale == 0) scale = 1.0;
        return {static_cast<int>(point.x / scale), static_cast<int>(point.y / scale)};
    }

    std::vector<DisplayMode> getSupportedDisplayModes(int monitorIndex) override {
        std::vector<DisplayMode> modes;
        if (!initialized_) return modes;

        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (!monitors || monitorIndex >= n) {
            if (monitors) XRRFreeMonitors(monitors);
            return modes;
        }

        RROutput primaryOutput = monitors[monitorIndex].outputs[0];
        XRRFreeMonitors(monitors);

        XRROutputInfo* outputInfo = XRRGetOutputInfo(display_, primaryOutput);
        if (!outputInfo) return modes;

        for (int i = 0; i < outputInfo->nmode; i++) {
            RRMode modeId = outputInfo->modes[i];
            XRRScreenResources* sr = XRRGetScreenResources(display_, root_);
            if (!sr) continue;
            for (int j = 0; j < sr->nmode; j++) {
                if (sr->modes[j].id == modeId) {
                    auto& m = sr->modes[j];
                    double rate = m.dotClock /
                        static_cast<double>(m.hTotal * m.vTotal);
                    modes.push_back(DisplayMode{
                        static_cast<int>(m.width),
                        static_cast<int>(m.height),
                        static_cast<int>(rate + 0.5),
                        32
                    });
                    break;
                }
            }
            XRRFreeScreenResources(sr);
        }
        XRRFreeOutputInfo(outputInfo);
        return modes;
    }

    std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) override {
        auto bounds = getMonitorBounds(monitorIndex);
        return DisplayMode{bounds.width, bounds.height, 60, 32};
    }

    bool setDisplayMode(int /*monitorIndex*/, const DisplayMode& /*mode*/) override {
        // xrandr mode setting requires root or xrandr commands
        return false;
    }

    bool resetDisplayMode(int /*monitorIndex*/) override {
        return false;
    }

    bool isScreenSaverRunning() override { return false; }
    void startScreenSaver() override {}
    bool isMonitorOff() override { return false; }
    void wakeUpMonitor() override {}

    Rect getVirtualScreenBounds() override {
        if (!initialized_) return {0, 0, 1920, 1080};
        Screen* s = DefaultScreenOfDisplay(display_);
        return {0, 0, s->width, s->height};
    }

    int getMonitorFromPoint(const Point& point) override {
        if (!initialized_) return 0;
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (!monitors) return 0;
        for (int i = 0; i < n; i++) {
            auto& m = monitors[i];
            if (point.x >= m.x && point.x < m.x + static_cast<int>(m.width) &&
                point.y >= m.y && point.y < m.y + static_cast<int>(m.height)) {
                XRRFreeMonitors(monitors);
                return i;
            }
        }
        XRRFreeMonitors(monitors);
        return 0;
    }

    int getMonitorFromWindow(WindowHandle hwnd) override {
        if (!initialized_ || hwnd == 0) return 0;
        // Query window position and find containing monitor
        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, static_cast<Window>(hwnd), &attrs) == 0)
            return 0;
        int x, y;
        Window child;
        XTranslateCoordinates(display_, static_cast<Window>(hwnd), root_,
                              0, 0, &x, &y, &child);
        return getMonitorFromPoint({x, y});
    }

    std::string getBackendName() const override { return "X11"; }
    BackendInfo getBackendInfo() const override {
        return {"X11", "1.0", initialized_, "X11/XRandR Screen API"};
    }

private:
    Display* display_ = nullptr;
    Window root_ = 0;
    bool initialized_ = false;
};

} // namespace wingman::platform::linux

namespace wingman::platform {

std::unique_ptr<IScreen> createPlatformScreen() {
    auto screen = std::unique_ptr<IScreen>(new linux::X11Screen());
    screen->initialize();
    return screen;
}

} // namespace wingman::platform

#endif // __linux__
