#include "wingman/platform/iscreen.hpp"
#include <spdlog/spdlog.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <vector>
#include <algorithm>
#include <climits>

// 取消 linux 宏定义
#undef linux

namespace wingman::platform::linux {

/**
 * @brief Linux X11 屏幕管理实现
 *
 * 使用 XRandR 扩展实现显示器管理和 DPI 查询。
 */
class X11Screen : public IScreen {
public:
    X11Screen() : display_(nullptr), initialized_(false) {}

    ~X11Screen() override {
        shutdown();
    }

    bool initialize() override {
        if (initialized_) return true;

        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("[X11Screen] Failed to open X display");
            return false;
        }

        // 检查 XRandR 扩展
        int majorVersion, minorVersion;
        if (!XRRQueryVersion(display_, &majorVersion, &minorVersion)) {
            spdlog::error("[X11Screen] XRandR extension not available");
            XCloseDisplay(display_);
            display_ = nullptr;
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

    int getMonitorCount() override {
        if (!initialized_) return 0;

        Window root = DefaultRootWindow(display_);
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &monitorCount_);

        if (monitors) {
            XRRFreeMonitors(monitors);
        }

        return monitorCount_;
    }

    int getPrimaryMonitorIndex() override {
        if (!initialized_) return 0;

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        int primaryIndex = 0;
        if (monitors) {
            for (int i = 0; i < count; ++i) {
                if (monitors[i].primary) {
                    primaryIndex = i;
                    break;
                }
            }
            XRRFreeMonitors(monitors);
        }

        return primaryIndex;
    }

    Rect getPrimaryMonitorBounds() override {
        return getMonitorBounds(getPrimaryMonitorIndex());
    }

    Rect getMonitorBounds(int monitorIndex) override {
        if (!initialized_) return Rect{};

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        Rect bounds{};
        if (monitors && monitorIndex >= 0 && monitorIndex < count) {
            bounds = Rect{
                monitors[monitorIndex].x,
                monitors[monitorIndex].y,
                static_cast<int>(monitors[monitorIndex].width),
                static_cast<int>(monitors[monitorIndex].height)
            };
        }

        if (monitors) {
            XRRFreeMonitors(monitors);
        }

        return bounds;
    }

    Rect getMonitorWorkArea(int monitorIndex) override {
        // X11 没有直接的工作区 API，返回整个显示器边界
        // 可以通过 _NET_WORKAREA 属性获取，但这里简化处理
        return getMonitorBounds(monitorIndex);
    }

    std::string getMonitorName(int monitorIndex) override {
        if (!initialized_) return "";

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        std::string name = "Display " + std::to_string(monitorIndex);

        if (monitors && monitorIndex >= 0 && monitorIndex < count) {
            XRRMonitorInfo* monitor = &monitors[monitorIndex];
            for (int i = 0; i < monitor->noutput; ++i) {
                XRROutputInfo* outputInfo = XRRGetOutputInfo(display_, monitor->outputs[i]);
                if (outputInfo && outputInfo->name) {
                    name = outputInfo->name;
                    XRRFreeOutputInfo(outputInfo);
                    break;
                }
            }
        }

        if (monitors) {
            XRRFreeMonitors(monitors);
        }

        return name;
    }

    bool isPrimaryMonitor(int monitorIndex) override {
        return monitorIndex == getPrimaryMonitorIndex();
    }

    double getDpiScale(int monitorIndex) override {
        int dpi = getDpi(monitorIndex);
        return static_cast<double>(dpi) / 96.0;
    }

    int getDpi(int monitorIndex) override {
        if (!initialized_) return 96;

        Window root = DefaultRootWindow(display_);
        int screenWidth = DisplayWidth(display_, 0);
        int screenHeight = DisplayHeight(display_, 0);
        int screenWidthMM = DisplayWidthMM(display_, 0);
        int screenHeightMM = DisplayHeightMM(display_, 0);

        if (screenWidthMM > 0 && screenHeightMM > 0) {
            double dpiX = (screenWidth * 25.4) / screenWidthMM;
            double dpiY = (screenHeight * 25.4) / screenHeightMM;
            return static_cast<int>((dpiX + dpiY) / 2.0);
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

        if (!initialized_) return modes;

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        if (monitors && monitorIndex >= 0 && monitorIndex < count) {
            XRRMonitorInfo* monitor = &monitors[monitorIndex];

            for (int i = 0; i < monitor->noutput; ++i) {
                XRROutputInfo* outputInfo = XRRGetOutputInfo(display_, monitor->outputs[i]);
                if (!outputInfo) continue;

                XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(display_, outputInfo->crtc);
                if (!crtcInfo) {
                    XRRFreeOutputInfo(outputInfo);
                    continue;
                }

                // 获取支持的模式
                for (int j = 0; j < outputInfo->nmode; ++j) {
                    XRRModeInfo* modeInfo = findModeInfo(outputInfo->modes[j]);
                    if (modeInfo) {
                        DisplayMode mode{
                            static_cast<int>(modeInfo->width),
                            static_cast<int>(modeInfo->height),
                            static_cast<int>(modeInfo->dotCount / (modeInfo->hTotal * modeInfo->vTotal)),
                            32
                        };

                        // 避免重复
                        bool exists = false;
                        for (const auto& m : modes) {
                            if (m.width == mode.width && m.height == mode.height &&
                                m.refreshRate == mode.refreshRate) {
                                exists = true;
                                break;
                            }
                        }

                        if (!exists) {
                            modes.push_back(mode);
                        }
                    }
                }

                XRRFreeCrtcInfo(crtcInfo);
                XRRFreeOutputInfo(outputInfo);
            }
        }

        if (monitors) {
            XRRFreeMonitors(monitors);
        }

        return modes;
    }

    std::optional<DisplayMode> getCurrentDisplayMode(int monitorIndex) override {
        if (!initialized_) return std::nullopt;

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        std::optional<DisplayMode> result;

        if (monitors && monitorIndex >= 0 && monitorIndex < count) {
            XRRMonitorInfo* monitor = &monitors[monitorIndex];

            for (int i = 0; i < monitor->noutput; ++i) {
                XRROutputInfo* outputInfo = XRRGetOutputInfo(display_, monitor->outputs[i]);
                if (!outputInfo || !outputInfo->crtc) {
                    if (outputInfo) XRRFreeOutputInfo(outputInfo);
                    continue;
                }

                XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(display_, outputInfo->crtc);
                if (!crtcInfo) {
                    XRRFreeOutputInfo(outputInfo);
                    continue;
                }

                XRRModeInfo* modeInfo = findModeInfo(crtcInfo->mode);
                if (modeInfo) {
                    double refreshRate = modeInfo->dotCount / (modeInfo->hTotal * modeInfo->vTotal);
                    result = DisplayMode{
                        static_cast<int>(modeInfo->width),
                        static_cast<int>(modeInfo->height),
                        static_cast<int>(refreshRate),
                        32
                    };
                }

                XRRFreeCrtcInfo(crtcInfo);
                XRRFreeOutputInfo(outputInfo);

                if (result) break;
            }
        }

        if (monitors) {
            XRRFreeMonitors(monitors);
        }

        return result;
    }

    bool setDisplayMode(int monitorIndex, const DisplayMode& mode) override {
        if (!initialized_) return false;

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        bool success = false;

        if (monitors && monitorIndex >= 0 && monitorIndex < count) {
            XRRMonitorInfo* monitor = &monitors[monitorIndex];

            for (int i = 0; i < monitor->noutput; ++i) {
                XRROutputInfo* outputInfo = XRRGetOutputInfo(display_, monitor->outputs[i]);
                if (!outputInfo) continue;

                // 查找匹配的模式
                RRMode targetMode = None;
                for (int j = 0; j < outputInfo->nmode; ++j) {
                    XRRModeInfo* modeInfo = findModeInfo(outputInfo->modes[j]);
                    if (modeInfo) {
                        double refreshRate = modeInfo->dotCount / (modeInfo->hTotal * modeInfo->vTotal);
                        if (modeInfo->width == mode.width &&
                            modeInfo->height == mode.height &&
                            static_cast<int>(refreshRate) == mode.refreshRate) {
                            targetMode = outputInfo->modes[j];
                            break;
                        }
                    }
                }

                if (targetMode != None) {
                    XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(display_, outputInfo->crtc);
                    if (crtcInfo) {
                        Status status = XRRSetCrtcConfig(
                            display_, outputInfo->crtc, CurrentTime,
                            crtcInfo->x, crtcInfo->y, targetMode,
                            crtcInfo->rotation, crtcInfo->outputs,
                            crtcInfo->noutput
                        );

                        success = (status == RRSetConfigSuccess);
                        XRRFreeCrtcInfo(crtcInfo);
                    }
                }

                XRRFreeOutputInfo(outputInfo);
                if (success) break;
            }
        }

        if (monitors) {
            XRRFreeMonitors(monitors);
        }

        return success;
    }

    bool resetDisplayMode(int monitorIndex) override {
        // 重置为自动模式
        return setDisplayMode(monitorIndex, DisplayMode{0, 0, 0, 32});
    }

    bool isScreenSaverRunning() override {
        if (!initialized_) return false;

        int state;
        if (XScreenSaverQueryInfo(display_, DefaultRootWindow(display_))) {
            // XScreenSaver 扩展存在
            return false;
        }

        // 检查 xscreensaver 或 gnome-screensaver 进程
        return false;
    }

    void startScreenSaver() override {
        if (!initialized_) return;

        // 发动屏幕保护程序
        XScreenSaverActivate(display_, CurrentTime);
        XFlush(display_);
    }

    bool isMonitorOff() override {
        if (!initialized_) return false;

        // 检查 DPMS 状态
        BOOL dpmsEnabled;
        CARD16 powerLevel;
        if (DPMSInfo(display_, &powerLevel, &dpmsEnabled)) {
            return dpmsEnabled && powerLevel != DPMSModeOn;
        }

        return false;
    }

    void wakeUpMonitor() override {
        if (!initialized_) return;

        // 唤醒显示器
        if (DPMSCapable(display_)) {
            DPMSForceLevel(display_, DPMSModeOn);
            XFlush(display_);
        }
    }

    Rect getVirtualScreenBounds() override {
        if (!initialized_) return Rect{};

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        int minX = INT_MAX, minY = INT_MAX;
        int maxX = INT_MIN, maxY = INT_MIN;

        if (monitors) {
            for (int i = 0; i < count; ++i) {
                minX = std::min(minX, monitors[i].x);
                minY = std::min(minY, monitors[i].y);
                maxX = std::max(maxX, monitors[i].x + static_cast<int>(monitors[i].width));
                maxY = std::max(maxY, monitors[i].y + static_cast<int>(monitors[i].height));
            }
            XRRFreeMonitors(monitors);
        }

        if (minX == INT_MAX) {
            return Rect{0, 0,
                       DisplayWidth(display_, 0),
                       DisplayHeight(display_, 0)};
        }

        return Rect{minX, minY, maxX - minX, maxY - minY};
    }

    int getMonitorFromPoint(const Point& point) override {
        if (!initialized_) return -1;

        Window root = DefaultRootWindow(display_);
        int count;
        XRRMonitorInfo* monitors = XRRGetMonitors(display_, root, True, &count);

        int result = -1;

        if (monitors) {
            for (int i = 0; i < count; ++i) {
                Rect bounds{
                    monitors[i].x,
                    monitors[i].y,
                    static_cast<int>(monitors[i].width),
                    static_cast<int>(monitors[i].height)
                };

                if (bounds.contains(point)) {
                    result = i;
                    break;
                }
            }
            XRRFreeMonitors(monitors);
        }

        return result;
    }

    int getMonitorFromWindow(WindowHandle hwnd) override {
        if (!initialized_) return -1;

        Window window = static_cast<Window>(hwnd);
        if (window == None) return -1;

        // 获取窗口位置
        Window root;
        int x, y;
        unsigned int width, height, border_width, depth;
        if (!XGetGeometry(display_, window, &root, &x, &y, &width, &height, &border_width, &depth)) {
            return -1;
        }

        // 获取绝对坐标
        int destX, destY;
        Window child;
        XTranslateCoordinates(display_, window, root, 0, 0, &destX, &destY, &child);

        return getMonitorFromPoint(Point{destX, destY});
    }

    std::string getBackendName() const override {
        return "X11";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "X11",
            "1.0",
            initialized_,
            "Linux X11/XRandR Screen API"
        };
    }

private:
    Display* display_;
    bool initialized_;
    int monitorCount_ = 0;

    XRRModeInfo* findModeInfo(RRMode mode) {
        Window root = DefaultRootWindow(display_);
        int numModes;
        XRRModeInfo* modes = XRRListModes(display_, root, &numModes);

        for (int i = 0; i < numModes; ++i) {
            if (modes[i].id == mode) {
                return &modes[i];
            }
        }

        return nullptr;
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
