// Screen 屏幕采集平台实现，自 screen.cpp 整体迁入：Windows Win32 GDI /
// macOS CoreGraphics（screencapture CLI + CG）/ Linux X11Capture 装配三条
// 路径原样保留；平台分支收口在 platform 分区（边界守卫豁免区）。
// Bitmap 的文件编解码（fromFile/save）在 screen_image_codec.cpp。
#include "wingman/screen.hpp"
#include "platform/screen_image_codec.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "platform/win/win32_screen.hpp"
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#endif

#if defined(__linux__) && !defined(__ANDROID__)
// Linux 截图装配：接线 X11Capture（此前 Screen::capture 恒 nullptr 的装配断链，
// 与 Clipboard 同款，2026-09-14 修复）。截图/取色/找色不依赖 OpenCV。
// __ANDROID__ 亦定义 __linux__，X11 装配在 NDK 下不适用（A2 起 Android 走
// MediaProjection 采集源，不经本文件 Screen 静态）。
#include "wingman/platform/icapture.hpp"
#include "wingman/platform/screen_factory.hpp"

// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间限定需要 undo（同 x11_factory.cpp）
#if defined(linux)
#undef linux
#endif

namespace wingman::platform::linux {
std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config);
}
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

namespace wingman {

#ifdef _WIN32
std::unique_ptr<Bitmap> Screen::capture() {
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    return capture(Rect(0, 0, width, height));
}

std::unique_ptr<Bitmap> Screen::capture(const Rect& region) {
    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) {
        return nullptr;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HBITMAP hbitmap = CreateCompatibleBitmap(hdcScreen, region.width, region.height);
    if (!hbitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HBITMAP hbitmapOld = (HBITMAP)SelectObject(hdcMem, hbitmap);

    BitBlt(hdcMem, 0, 0, region.width, region.height,
           hdcScreen, region.x, region.y, SRCCOPY);

    SelectObject(hdcMem, hbitmapOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    auto bitmap = platform::win::bitmapFromHBITMAP(hbitmap);
    DeleteObject(hbitmap);

    return bitmap;
}

Color Screen::getPixel(int x, int y) {
    HDC hdc = GetDC(nullptr);
    if (!hdc) {
        return Color();
    }

    COLORREF color = GetPixel(hdc, x, y);
    ReleaseDC(nullptr, hdc);

    return Color(
        GetRValue(color),
        GetGValue(color),
        GetBValue(color)
    );
}

bool Screen::findColor(const Color& color, const Rect& region,
                      int tolerance, Point& result) {
    auto bitmap = capture(region);
    if (!bitmap) {
        return false;
    }

    int width = bitmap->getWidth();
    int height = bitmap->getHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color pixel = bitmap->getPixel(x, y);
            if (pixel.matches(color, tolerance)) {
                result.x = region.x + x;
                result.y = region.y + y;
                return true;
            }
        }
    }

    return false;
}

std::vector<Point> Screen::findColors(const Color& color, const Rect& region,
                                      int tolerance, int maxCount) {
    std::vector<Point> results;
    auto bitmap = capture(region);
    if (!bitmap) {
        return results;
    }

    int width = bitmap->getWidth();
    int height = bitmap->getHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color pixel = bitmap->getPixel(x, y);
            if (pixel.matches(color, tolerance)) {
                results.emplace_back(region.x + x, region.y + y);
                if (maxCount > 0 && results.size() >= maxCount) {
                    return results;
                }
            }
        }
    }

    return results;
}

bool Screen::findImage(const std::string& imagePath, const Rect& region,
                       double threshold, Point& result) {
    auto screenBitmap = capture(region);
    if (!screenBitmap) {
        return false;
    }
    return matchTemplateOnBitmap(imagePath, screenBitmap->getData(),
                                 screenBitmap->getWidth(), screenBitmap->getHeight(),
                                 Point(region.x, region.y), threshold, result);
}

int Screen::getScreenWidth() {
    return GetSystemMetrics(SM_CXSCREEN);
}

int Screen::getScreenHeight() {
    return GetSystemMetrics(SM_CYSCREEN);
}

Rect Screen::getScreenBounds() {
    return Rect(0, 0, getScreenWidth(), getScreenHeight());
}
#elif defined(__APPLE__)
namespace {

std::unique_ptr<Bitmap> bitmapFromCGImage(CGImageRef image) {
    if (!image) {
        return nullptr;
    }

    const int width = static_cast<int>(CGImageGetWidth(image));
    const int height = static_cast<int>(CGImageGetHeight(image));
    if (width <= 0 || height <= 0) {
        return nullptr;
    }

    auto bitmap = std::make_unique<Bitmap>(width, height);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (!colorSpace) {
        return nullptr;
    }

    CGContextRef context = CGBitmapContextCreate(
        bitmap->getData(),
        static_cast<size_t>(width),
        static_cast<size_t>(height),
        8,
        static_cast<size_t>(width * 4),
        colorSpace,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | kCGBitmapByteOrder32Little
    );
    CGColorSpaceRelease(colorSpace);

    if (!context) {
        return nullptr;
    }

    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    CGContextRelease(context);
    return bitmap;
}

CGImageRef captureImage(const Rect& region) {
    char pathTemplate[] = "/tmp/wingman-capture-XXXXXX.png";
    const int fd = mkstemps(pathTemplate, 4);
    if (fd == -1) {
        return nullptr;
    }
    close(fd);

    const std::filesystem::path imagePath(pathTemplate);
    const std::string command = "/usr/sbin/screencapture -x -R" +
        std::to_string(region.x) + "," +
        std::to_string(region.y) + "," +
        std::to_string(region.width) + "," +
        std::to_string(region.height) + " \"" +
        imagePath.string() + "\" >/dev/null 2>&1";

    if (std::system(command.c_str()) != 0) {
        std::error_code ec;
        std::filesystem::remove(imagePath, ec);
        return nullptr;
    }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(imagePath.string().c_str()),
        static_cast<CFIndex>(imagePath.string().size()),
        false
    );
    if (!url) {
        std::error_code ec;
        std::filesystem::remove(imagePath, ec);
        return nullptr;
    }

    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!source) {
        std::error_code ec;
        std::filesystem::remove(imagePath, ec);
        return nullptr;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);

    std::error_code ec;
    std::filesystem::remove(imagePath, ec);
    return image;
}

} // namespace

std::unique_ptr<Bitmap> Screen::capture() {
    return capture(getScreenBounds());
}

std::unique_ptr<Bitmap> Screen::capture(const Rect& region) {
    if (region.isEmpty()) {
        return nullptr;
    }

    CGImageRef image = captureImage(region);
    if (!image) {
        return nullptr;
    }

    auto bitmap = bitmapFromCGImage(image);
    CGImageRelease(image);
    return bitmap;
}

Color Screen::getPixel(int x, int y) {
    auto bitmap = capture(Rect(x, y, 1, 1));
    if (!bitmap) {
        return Color();
    }
    return bitmap->getPixel(0, 0);
}

bool Screen::findColor(const Color& color, const Rect& region,
                      int tolerance, Point& result) {
    auto bitmap = capture(region);
    if (!bitmap) {
        return false;
    }

    for (int y = 0; y < bitmap->getHeight(); ++y) {
        for (int x = 0; x < bitmap->getWidth(); ++x) {
            const Color pixel = bitmap->getPixel(x, y);
            if (pixel.matches(color, tolerance)) {
                result = Point(region.x + x, region.y + y);
                return true;
            }
        }
    }

    return false;
}

std::vector<Point> Screen::findColors(const Color& color, const Rect& region,
                                      int tolerance, int maxCount) {
    std::vector<Point> results;
    auto bitmap = capture(region);
    if (!bitmap) {
        return results;
    }

    for (int y = 0; y < bitmap->getHeight(); ++y) {
        for (int x = 0; x < bitmap->getWidth(); ++x) {
            const Color pixel = bitmap->getPixel(x, y);
            if (pixel.matches(color, tolerance)) {
                results.emplace_back(region.x + x, region.y + y);
                if (maxCount > 0 && static_cast<int>(results.size()) >= maxCount) {
                    return results;
                }
            }
        }
    }

    return results;
}

bool Screen::findImage(const std::string& /*imagePath*/, const Rect& /*region*/,
                       double /*threshold*/, Point& /*result*/) {
    return false;
}

int Screen::getScreenWidth() {
    return static_cast<int>(CGDisplayPixelsWide(CGMainDisplayID()));
}

int Screen::getScreenHeight() {
    return static_cast<int>(CGDisplayPixelsHigh(CGMainDisplayID()));
}

Rect Screen::getScreenBounds() {
    const CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    return Rect(
        static_cast<int>(bounds.origin.x),
        static_cast<int>(bounds.origin.y),
        static_cast<int>(bounds.size.width),
        static_cast<int>(bounds.size.height)
    );
}
#endif // _WIN32 / __APPLE__

// ============================================================================
// Screen Implementation (Linux) —— 接线 X11Capture
// ============================================================================
//
// 此前本文件在 Linux 两个分支（有/无 vision）里都是恒 nullptr 的 stub，
// X11Capture 有完整实现却无产品消费者（装配断链，与 Clipboard 同款，
// 2026-09-14 接线）。截图/取色/找色不依赖 OpenCV，vision 与否共用本实现；
// findImage（模板匹配）在有 OpenCV 的构建里走共享 matchTemplateOnBitmap
// （vision feature，2026-09-15 接线），无 vision 构建保持 stub。
// 每次调用经工厂独立创建 ICapture（自带 X 连接），规避跨线程共享 Display
// 的线程安全问题；XOpenDisplay 走本地 socket，开销亚毫秒。

#if defined(__linux__) && !defined(__ANDROID__)
namespace {

std::unique_ptr<platform::ICapture> linuxCapture() {
    auto capture = platform::linux::createX11Capture(platform::CaptureConfig{});
    if (!capture || !capture->isAvailable()) {
        return nullptr;
    }
    return capture;
}

std::unique_ptr<platform::IScreen> linuxScreen() {
    return platform::createPlatformScreen();
}

} // namespace

std::unique_ptr<Bitmap> Screen::capture() {
    auto capture = linuxCapture();
    if (!capture) {
        return nullptr;
    }
    return capture->captureScreen(0);
}

std::unique_ptr<Bitmap> Screen::capture(const Rect& region) {
    if (region.isEmpty()) {
        return nullptr;
    }
    auto capture = linuxCapture();
    if (!capture) {
        return nullptr;
    }
    return capture->captureRegion(
        platform::Rect{region.x, region.y, region.width, region.height});
}

Color Screen::getPixel(int x, int y) {
    auto bitmap = capture(Rect(x, y, 1, 1));
    if (!bitmap) {
        return Color();
    }
    return bitmap->getPixel(0, 0);
}

bool Screen::findColor(const Color& color, const Rect& region,
                      int tolerance, Point& result) {
    auto bitmap = capture(region);
    if (!bitmap) {
        return false;
    }

    for (int y = 0; y < bitmap->getHeight(); ++y) {
        for (int x = 0; x < bitmap->getWidth(); ++x) {
            if (bitmap->getPixel(x, y).matches(color, tolerance)) {
                result.x = region.x + x;
                result.y = region.y + y;
                return true;
            }
        }
    }

    return false;
}

std::vector<Point> Screen::findColors(const Color& color, const Rect& region,
                                      int tolerance, int maxCount) {
    std::vector<Point> results;
    auto bitmap = capture(region);
    if (!bitmap) {
        return results;
    }

    for (int y = 0; y < bitmap->getHeight(); ++y) {
        for (int x = 0; x < bitmap->getWidth(); ++x) {
            if (bitmap->getPixel(x, y).matches(color, tolerance)) {
                results.emplace_back(region.x + x, region.y + y);
                if (maxCount > 0 && results.size() >= static_cast<size_t>(maxCount)) {
                    return results;
                }
            }
        }
    }

    return results;
}

bool Screen::findImage(const std::string& imagePath, const Rect& region,
                       double threshold, Point& result) {
#ifdef WINGMAN_ENABLE_VISION
    // 模板匹配走 OpenCV（与 Windows 共用 matchTemplateOnBitmap）
    auto screenBitmap = capture(region);
    if (!screenBitmap) {
        return false;
    }
    return matchTemplateOnBitmap(imagePath, screenBitmap->getData(),
                                 screenBitmap->getWidth(), screenBitmap->getHeight(),
                                 Point(region.x, region.y), threshold, result);
#else
    (void)imagePath; (void)region; (void)threshold; (void)result;
    // 模板匹配需 OpenCV：启用 vcpkg vision feature（-DVCPKG_MANIFEST_FEATURES=...;vision）
    return false;
#endif
}

int Screen::getScreenWidth() {
    auto screen = linuxScreen();
    if (!screen) {
        return 0;
    }
    return screen->getPrimaryMonitorBounds().width;
}

int Screen::getScreenHeight() {
    auto screen = linuxScreen();
    if (!screen) {
        return 0;
    }
    return screen->getPrimaryMonitorBounds().height;
}

Rect Screen::getScreenBounds() {
    return Rect(0, 0, getScreenWidth(), getScreenHeight());
}

#endif // __linux__

} // namespace wingman
