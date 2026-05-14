#include "wingman/platform/icapture.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>

#include <vector>
#include <cstring>
#include <algorithm>

namespace wingman::platform::linux {

/**
 * @brief Linux X11 屏幕捕获实现
 *
 * 使用 XGetImage 和 XShm 扩展进行屏幕捕获。
 */
class X11Capture : public ICapture {
public:
    X11Capture() : display_(nullptr), initialized_(false), useShm_(false) {}

    ~X11Capture() override {
        shutdown();
    }

    bool initialize(const CaptureConfig& config) override {
        if (initialized_) return true;

        config_ = config;
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("[X11Capture] Failed to open X display");
            return false;
        }

        // 检查 XShm 扩展
        int majorOpcode, firstEvent, firstError;
        if (XQueryExtension(display_, "MIT-SHM", &majorOpcode, &firstEvent, &firstError)) {
            // 尝试使用 XShm
            useShm_ = testShm();
            if (useShm_) {
                spdlog::info("[X11Capture] Using XShm extension for faster capture");
            } else {
                spdlog::warn("[X11Capture] XShm available but not working, using XGetImage");
            }
        } else {
            spdlog::info("[X11Capture] XShm not available, using XGetImage");
        }

        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (shmInfo_.shmaddr) {
            XShmDetach(display_, &shmInfo_);
            shmdt(shmInfo_.shmaddr);
            shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
            shmInfo_.shmaddr = nullptr;
        }

        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }
        initialized_ = false;
    }

    std::unique_ptr<Bitmap> captureScreen(int monitorIndex) override {
        if (!initialized_) return nullptr;

        Rect bounds = getMonitorBounds(monitorIndex);
        if (bounds.isEmpty()) return nullptr;

        return captureRegion(bounds);
    }

    std::unique_ptr<Bitmap> captureRegion(const Rect& region) override {
        if (!initialized_) return nullptr;

        if (useShm_) {
            return captureRegionShm(region, DefaultRootWindow(display_));
        } else {
            return captureRegionXImage(region, DefaultRootWindow(display_));
        }
    }

    std::unique_ptr<Bitmap> captureRegion(int monitorIndex, const Rect& region) override {
        if (!initialized_) return nullptr;

        Window root = DefaultRootWindow(display_);
        Window target = getMonitorWindow(monitorIndex);
        if (target == None) target = root;

        return captureRegion(region);
    }

    std::unique_ptr<Bitmap> captureWindow(WindowHandle hwnd) override {
        if (!initialized_) return nullptr;

        Window window = static_cast<Window>(hwnd);
        if (window == None) return nullptr;

        // 获取窗口边界
        Window root;
        int x, y;
        unsigned int width, height, border_width, depth;
        if (!XGetGeometry(display_, window, &root, &x, &y, &width, &height, &border_width, &depth)) {
            return nullptr;
        }

        Rect bounds{0, 0, static_cast<int>(width), static_cast<int>(height)};

        if (useShm_) {
            return captureRegionShm(bounds, window);
        } else {
            return captureRegionXImage(bounds, window);
        }
    }

    std::unique_ptr<Bitmap> captureWindowRegion(WindowHandle hwnd, const Rect& region) override {
        if (!initialized_) return nullptr;

        Window window = static_cast<Window>(hwnd);
        if (window == None) return nullptr;

        if (useShm_) {
            return captureRegionShm(region, window);
        } else {
            return captureRegionXImage(region, window);
        }
    }

    int getMonitorCount() override {
        if (!initialized_) return 0;

        return ScreenCount(display_);
    }

    Rect getMonitorBounds(int monitorIndex) override {
        if (!initialized_) return Rect{};

        if (monitorIndex < 0 || monitorIndex >= ScreenCount(display_)) {
            return Rect{};
        }

        return Rect{
            0,
            0,
            DisplayWidth(display_, monitorIndex),
            DisplayHeight(display_, monitorIndex)
        };
    }

    std::string getMonitorName(int monitorIndex) override {
        if (!initialized_) return "";

        if (monitorIndex < 0 || monitorIndex >= ScreenCount(display_)) {
            return "";
        }

        return "Screen " + std::to_string(monitorIndex);
    }

    bool isAvailable() const override {
        return initialized_;
    }

    std::string getBackendName() const override {
        return useShm_ ? "X11-Shm" : "X11";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            useShm_ ? "X11-Shm" : "X11",
            "1.0",
            initialized_,
            useShm_ ? "Linux X11 with MIT-SHM extension" : "Linux X11 XGetImage"
        };
    }

private:
    Display* display_;
    bool initialized_;
    bool useShm_;
    CaptureConfig config_;
    XShmSegmentInfo shmInfo_;

    bool testShm() {
        // 创建一个小的测试图像
        XImage* testImage = XShmCreateImage(
            display_,
            DefaultVisual(display_, 0),
            DefaultDepth(display_, 0),
            ZPixmap,
            nullptr,
            &shmInfo_,
            1, 1
        );

        if (!testImage) return false;

        shmInfo_.shmid = shmget(IPC_PRIVATE, testImage->bytes_per_line, IPC_CREAT | 0777);
        if (shmInfo_.shmid == -1) {
            XDestroyImage(testImage);
            return false;
        }

        shmInfo_.shmaddr = testImage->data = reinterpret_cast<char*>(shmat(shmInfo_.shmid, nullptr, 0));
        if (shmInfo_.shmaddr == reinterpret_cast<char*>(-1)) {
            shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
            XDestroyImage(testImage);
            return false;
        }

        shmInfo_.readOnly = False;

        if (!XShmAttach(display_, &shmInfo_)) {
            shmdt(shmInfo_.shmaddr);
            shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
            XDestroyImage(testImage);
            return false;
        }

        // 测试捕获
        bool success = (XShmGetImage(display_, DefaultRootWindow(display_), testImage, 0, 0, AllPlanes) != 0);

        // 清理
        XShmDetach(display_, &shmInfo_);
        shmdt(shmInfo_.shmaddr);
        shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
        XDestroyImage(testImage);

        memset(&shmInfo_, 0, sizeof(shmInfo_));

        return success;
    }

    std::unique_ptr<Bitmap> captureRegionShm(const Rect& region, Window window) {
        // 获取窗口边界
        Window root;
        int x, y;
        unsigned int width, height, border_width, depth;
        if (!XGetGeometry(display_, window, &root, &x, &y, &width, &height, &border_width, &depth)) {
            return nullptr;
        }

        // 限制捕获区域
        Rect captureRegion = region;
        captureRegion.x = std::max(0, std::min(captureRegion.x, static_cast<int>(width) - 1));
        captureRegion.y = std::max(0, std::min(captureRegion.y, static_cast<int>(height) - 1));
        captureRegion.width = std::min(captureRegion.width, static_cast<int>(width) - captureRegion.x);
        captureRegion.height = std::min(captureRegion.height, static_cast<int>(height) - captureRegion.y);

        if (captureRegion.width <= 0 || captureRegion.height <= 0) {
            return nullptr;
        }

        // 创建 XImage
        XImage* xImage = XShmCreateImage(
            display_,
            DefaultVisual(display_, 0),
            DefaultDepth(display_, 0),
            ZPixmap,
            nullptr,
            &shmInfo_,
            captureRegion.width,
            captureRegion.height
        );

        if (!xImage) {
            return captureRegionXImage(region, window);
        }

        // 分配共享内存
        shmInfo_.shmid = shmget(IPC_PRIVATE, xImage->bytes_per_line * captureRegion.height, IPC_CREAT | 0777);
        if (shmInfo_.shmid == -1) {
            XDestroyImage(xImage);
            return captureRegionXImage(region, window);
        }

        shmInfo_.shmaddr = xImage->data = reinterpret_cast<char*>(shmat(shmInfo_.shmid, nullptr, 0));
        if (shmInfo_.shmaddr == reinterpret_cast<char*>(-1)) {
            shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
            XDestroyImage(xImage);
            return captureRegionXImage(region, window);
        }

        shmInfo_.readOnly = False;

        if (!XShmAttach(display_, &shmInfo_)) {
            shmdt(shmInfo_.shmaddr);
            shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
            XDestroyImage(xImage);
            return captureRegionXImage(region, window);
        }

        // 捕获图像
        bool success = XShmGetImage(
            display_,
            window,
            xImage,
            captureRegion.x,
            captureRegion.y,
            AllPlanes
        ) != 0;

        std::unique_ptr<Bitmap> result;

        if (success) {
            result = xImageToBitmap(xImage, captureRegion.width, captureRegion.height);
        }

        // 清理
        XShmDetach(display_, &shmInfo_);
        XDestroyImage(xImage);
        shmdt(shmInfo_.shmaddr);
        shmctl(shmInfo_.shmid, IPC_RMID, nullptr);
        memset(&shmInfo_, 0, sizeof(shmInfo_));

        return result;
    }

    std::unique_ptr<Bitmap> captureRegionXImage(const Rect& region, Window window) {
        // 获取窗口边界
        Window root;
        int x, y;
        unsigned int width, height, border_width, depth;
        if (!XGetGeometry(display_, window, &root, &x, &y, &width, &height, &border_width, &depth)) {
            return nullptr;
        }

        // 限制捕获区域
        Rect captureRegion = region;
        captureRegion.x = std::max(0, std::min(captureRegion.x, static_cast<int>(width) - 1));
        captureRegion.y = std::max(0, std::min(captureRegion.y, static_cast<int>(height) - 1));
        captureRegion.width = std::min(captureRegion.width, static_cast<int>(width) - captureRegion.x);
        captureRegion.height = std::min(captureRegion.height, static_cast<int>(height) - captureRegion.y);

        if (captureRegion.width <= 0 || captureRegion.height <= 0) {
            return nullptr;
        }

        // 捕获图像
        XImage* xImage = XGetImage(
            display_,
            window,
            captureRegion.x,
            captureRegion.y,
            captureRegion.width,
            captureRegion.height,
            AllPlanes,
            ZPixmap
        );

        if (!xImage) {
            return nullptr;
        }

        auto result = xImageToBitmap(xImage, captureRegion.width, captureRegion.height);
        XDestroyImage(xImage);

        return result;
    }

    std::unique_ptr<Bitmap> xImageToBitmap(XImage* xImage, int width, int height) {
        if (!xImage) return nullptr;

        auto bitmap = std::make_unique<Bitmap>(width, height);
        uint8_t* dest = bitmap->getData();

        // 转换像素格式
        if (xImage->bits_per_pixel == 32) {
            // 32位：BGRA 或 RGBA
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    unsigned long pixel = XGetPixel(xImage, x, y);

                    int destIndex = (y * width + x) * 4;

                    // 检测字节序
                    if (xImage->byte_order == LSBFirst) {
                        // BGRA
                        dest[destIndex + 0] = (pixel >> 0) & 0xFF;  // B
                        dest[destIndex + 1] = (pixel >> 8) & 0xFF;  // G
                        dest[destIndex + 2] = (pixel >> 16) & 0xFF; // R
                        dest[destIndex + 3] = (pixel >> 24) & 0xFF; // A
                    } else {
                        // RGBA
                        dest[destIndex + 0] = (pixel >> 24) & 0xFF; // R
                        dest[destIndex + 1] = (pixel >> 16) & 0xFF; // G
                        dest[destIndex + 2] = (pixel >> 8) & 0xFF;  // B
                        dest[destIndex + 3] = (pixel >> 0) & 0xFF;  // A
                    }
                }
            }
        } else if (xImage->bits_per_pixel == 24) {
            // 24位：BGR 或 RGB
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    unsigned long pixel = XGetPixel(xImage, x, y);

                    int destIndex = (y * width + x) * 4;

                    if (xImage->byte_order == LSBFirst) {
                        dest[destIndex + 0] = (pixel >> 0) & 0xFF;  // B
                        dest[destIndex + 1] = (pixel >> 8) & 0xFF;  // G
                        dest[destIndex + 2] = (pixel >> 16) & 0xFF; // R
                        dest[destIndex + 3] = 255;                  // A
                    } else {
                        dest[destIndex + 0] = (pixel >> 16) & 0xFF; // R
                        dest[destIndex + 1] = (pixel >> 8) & 0xFF;  // G
                        dest[destIndex + 2] = (pixel >> 0) & 0xFF;  // B
                        dest[destIndex + 3] = 255;                  // A
                    }
                }
            }
        } else if (xImage->bits_per_pixel == 16) {
            // 16位：RGB565
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    unsigned long pixel = XGetPixel(xImage, x, y);

                    int destIndex = (y * width + x) * 4;

                    uint8_t r = ((pixel >> 11) & 0x1F) << 3;
                    uint8_t g = ((pixel >> 5) & 0x3F) << 2;
                    uint8_t b = (pixel & 0x1F) << 3;

                    dest[destIndex + 0] = b;
                    dest[destIndex + 1] = g;
                    dest[destIndex + 2] = r;
                    dest[destIndex + 3] = 255;
                }
            }
        } else {
            // 其他格式，直接复制
            spdlog::warn("[X11Capture] Unsupported pixel format: {} bits", xImage->bits_per_pixel);
            return nullptr;
        }

        return bitmap;
    }

    Window getMonitorWindow(int monitorIndex) {
        if (monitorIndex < 0 || monitorIndex >= ScreenCount(display_)) {
            return None;
        }
        return RootWindow(display_, monitorIndex);
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
