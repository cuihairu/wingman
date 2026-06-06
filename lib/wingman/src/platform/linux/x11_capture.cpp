#ifdef __linux__

#include "wingman/platform/icapture.hpp"
#include "wingman/screen.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <spdlog/spdlog.h>

namespace wingman::platform::linux {

class X11Capture : public ICapture {
public:
    X11Capture() = default;
    ~X11Capture() override { shutdown(); }

    bool initialize(const CaptureConfig& config) override {
        config_ = config;
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("X11Capture: failed to open X display");
            return false;
        }
        root_ = DefaultRootWindow(display_);
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (display_) { XCloseDisplay(display_); display_ = nullptr; }
        initialized_ = false;
    }

    std::unique_ptr<Bitmap> captureScreen(int monitorIndex) override {
        Rect bounds = getMonitorBounds(monitorIndex);
        return captureRegion(monitorIndex, bounds);
    }

    std::unique_ptr<Bitmap> captureRegion(const Rect& region) override {
        return captureRegion(0, region);
    }

    std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) override {
        if (!initialized_ || !display_) return nullptr;

        Rect bounds = region.isEmpty() ? getMonitorBounds(monitorIndex) : region;
        XImage* image = XGetImage(display_, root_,
                                  bounds.x, bounds.y,
                                  bounds.width, bounds.height,
                                  AllPlanes, ZPixmap);
        if (!image) {
            spdlog::error("X11Capture: XGetImage failed");
            return nullptr;
        }

        auto bitmap = std::make_unique<Bitmap>(bounds.width, bounds.height);
        uint8_t* dst = bitmap->getData();

        // Convert XImage (typically BGRA on little-endian) to Bitmap format
        for (int y = 0; y < bounds.height; y++) {
            for (int x = 0; x < bounds.width; x++) {
                unsigned long pixel = XGetPixel(image, x, y);
                uint8_t b = pixel & 0xFF;
                uint8_t g = (pixel >> 8) & 0xFF;
                uint8_t r = (pixel >> 16) & 0xFF;
                uint8_t a = (pixel >> 24) & 0xFF;

                int offset = (y * bounds.width + x) * 4;
                dst[offset + 0] = b;
                dst[offset + 1] = g;
                dst[offset + 2] = r;
                dst[offset + 3] = a;
            }
        }

        XDestroyImage(image);
        return bitmap;
    }

    std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) override {
        if (!initialized_ || hwnd == 0) return nullptr;

        XWindowAttributes attrs;
        if (XGetWindowAttributes(display_, static_cast<Window>(hwnd), &attrs) == 0)
            return nullptr;

        // Translate to root coordinates
        int x, y;
        Window child;
        XTranslateCoordinates(display_, static_cast<Window>(hwnd), root_,
                              0, 0, &x, &y, &child);

        XImage* image = XGetImage(display_, static_cast<Window>(hwnd),
                                  0, 0, attrs.width, attrs.height,
                                  AllPlanes, ZPixmap);
        if (!image) return nullptr;

        auto bitmap = std::make_unique<Bitmap>(attrs.width, attrs.height);
        uint8_t* dst = bitmap->getData();

        for (int py = 0; py < attrs.height; py++) {
            for (int px = 0; px < attrs.width; px++) {
                unsigned long pixel = XGetPixel(image, px, py);
                int offset = (py * attrs.width + px) * 4;
                dst[offset + 0] = pixel & 0xFF;
                dst[offset + 1] = (pixel >> 8) & 0xFF;
                dst[offset + 2] = (pixel >> 16) & 0xFF;
                dst[offset + 3] = (pixel >> 24) & 0xFF;
            }
        }

        XDestroyImage(image);
        return bitmap;
    }

    std::unique_ptr<Bitmap> captureWindowRegion(WindowHandle hwnd, const Rect& region) override {
        if (!initialized_ || hwnd == 0) return nullptr;

        XImage* image = XGetImage(display_, static_cast<Window>(hwnd),
                                  region.x, region.y,
                                  region.width, region.height,
                                  AllPlanes, ZPixmap);
        if (!image) return nullptr;

        auto bitmap = std::make_unique<Bitmap>(region.width, region.height);
        uint8_t* dst = bitmap->getData();

        for (int py = 0; py < region.height; py++) {
            for (int px = 0; px < region.width; px++) {
                unsigned long pixel = XGetPixel(image, px, py);
                int offset = (py * region.width + px) * 4;
                dst[offset + 0] = pixel & 0xFF;
                dst[offset + 1] = (pixel >> 8) & 0xFF;
                dst[offset + 2] = (pixel >> 16) & 0xFF;
                dst[offset + 3] = (pixel >> 24) & 0xFF;
            }
        }

        XDestroyImage(image);
        return bitmap;
    }

    int getMonitorCount() override {
        if (!initialized_) return 1;
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (monitors) XRRFreeMonitors(monitors);
        return n > 0 ? n : 1;
    }

    Rect getMonitorBounds(int monitorIndex) override {
        if (!initialized_) return {0, 0, 1920, 1080};
        int n = 0;
        auto* monitors = XRRGetMonitors(display_, root_, True, &n);
        if (!monitors || monitorIndex >= n) {
            if (monitors) XRRFreeMonitors(monitors);
            Screen* s = DefaultScreenOfDisplay(display_);
            return {0, 0, s->width, s->height};
        }
        auto& m = monitors[monitorIndex];
        Rect r{m.x, m.y, static_cast<int>(m.width), static_cast<int>(m.height)};
        XRRFreeMonitors(monitors);
        return r;
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

    bool isAvailable() const override { return initialized_; }
    std::string getBackendName() const override { return "X11"; }
    BackendInfo getBackendInfo() const override {
        return {"X11", "1.0", initialized_, "X11 XGetImage Screen Capture"};
    }

private:
    Display* display_ = nullptr;
    Window root_ = 0;
    CaptureConfig config_;
    bool initialized_ = false;
};

} // namespace wingman::platform::linux

#endif // __linux__
