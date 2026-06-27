#include "wingman/screen.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <comdef.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#endif

#ifdef _WIN32
#include <opencv2/opencv.hpp>
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <ImageIO/ImageIO.h>
#include <unistd.h>
#endif

#include <cstring>
#include <algorithm>
#include <filesystem>

namespace wingman {

// ============================================================================
// Bitmap Implementation
// ============================================================================

Bitmap::Bitmap(int width, int height)
    : m_width(width), m_height(height),
      m_data(new uint8_t[width * height * 4]()) {
    // Value-initialize array to zeros (prevents garbage data)
}

Bitmap::Bitmap(const Bitmap& other)
    : m_width(other.m_width), m_height(other.m_height),
      m_data(new uint8_t[m_width * m_height * 4]) {
    std::memcpy(m_data.get(), other.m_data.get(), m_width * m_height * 4);
}

Bitmap::Bitmap(Bitmap&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height),
      m_data(std::move(other.m_data)) {
    other.m_width = 0;
    other.m_height = 0;
}

Bitmap::~Bitmap() = default;

Bitmap& Bitmap::operator=(const Bitmap& other) {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_data.reset(new uint8_t[m_width * m_height * 4]);
        std::memcpy(m_data.get(), other.m_data.get(), m_width * m_height * 4);
    }
    return *this;
}

Bitmap& Bitmap::operator=(Bitmap&& other) noexcept {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_data = std::move(other.m_data);
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

Color Bitmap::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return Color();
    }
    const uint8_t* p = m_data.get() + (y * m_width + x) * 4;
    return Color(p[2], p[1], p[0], p[3]);
}

void Bitmap::setPixel(int x, int y, const Color& color) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return;
    }
    uint8_t* p = m_data.get() + (y * m_width + x) * 4;
    p[0] = color.b;
    p[1] = color.g;
    p[2] = color.r;
    p[3] = color.a;
}

#ifdef _WIN32
std::unique_ptr<Bitmap> Bitmap::fromHBITMAP(HBITMAP hbitmap) {
    BITMAP bm = {};
    if (!GetObject(hbitmap, sizeof(bm), &bm)) {
        return nullptr;
    }

    auto bitmap = std::make_unique<Bitmap>(bm.bmWidth, bm.bmHeight);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    if (!hdc) {
        return nullptr;
    }

    int result = GetDIBits(hdc, hbitmap, 0, bm.bmHeight,
                           bitmap->getData(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    if (!result) {
        return nullptr;
    }

    return bitmap;
}
#endif

std::unique_ptr<Bitmap> Bitmap::fromFile(const std::string& filepath) {
    if (!std::filesystem::exists(filepath)) {
        return nullptr;
    }

#ifdef _WIN32
    static bool gdiplusInitialized = false;
    static ULONG_PTR gdiplusToken;

    if (!gdiplusInitialized) {
        Gdiplus::GdiplusStartupInput startupInput = {};
        Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);
        gdiplusInitialized = true;
    }

    std::wstring widePath(filepath.begin(), filepath.end());
    auto* gdiBmp = Gdiplus::Bitmap::FromFile(widePath.c_str());
    if (!gdiBmp || gdiBmp->GetLastStatus() != Gdiplus::Ok) {
        delete gdiBmp;
        return nullptr;
    }

    UINT w = gdiBmp->GetWidth();
    UINT h = gdiBmp->GetHeight();
    auto bitmap = std::make_unique<Bitmap>(static_cast<int>(w), static_cast<int>(h));

    Gdiplus::BitmapData bmpData = {};
    Gdiplus::Rect rect(0, 0, static_cast<INT>(w), static_cast<INT>(h));
    if (gdiBmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData) != Gdiplus::Ok) {
        delete gdiBmp;
        return nullptr;
    }

    for (UINT y = 0; y < h; ++y) {
        const uint8_t* srcRow = static_cast<const uint8_t*>(bmpData.Scan0) + y * bmpData.Stride;
        uint8_t* dstRow = bitmap->getData() + y * w * 4;
        // GDI+ ARGB → internal BGRA
        for (UINT x = 0; x < w; ++x) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 0]; // B
            dstRow[x * 4 + 1] = srcRow[x * 4 + 1]; // G
            dstRow[x * 4 + 2] = srcRow[x * 4 + 2]; // R
            dstRow[x * 4 + 3] = srcRow[x * 4 + 3]; // A
        }
    }

    gdiBmp->UnlockBits(&bmpData);
    delete gdiBmp;
    return bitmap;
#elif defined(__APPLE__)
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(filepath.c_str()),
        static_cast<CFIndex>(filepath.size()),
        false
    );
    if (!url) {
        return nullptr;
    }

    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!source) {
        return nullptr;
    }

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (!image) {
        return nullptr;
    }

    const int width = static_cast<int>(CGImageGetWidth(image));
    const int height = static_cast<int>(CGImageGetHeight(image));
    if (width <= 0 || height <= 0) {
        CGImageRelease(image);
        return nullptr;
    }

    auto bitmap = std::make_unique<Bitmap>(width, height);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (!colorSpace) {
        CGImageRelease(image);
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
        CGImageRelease(image);
        return nullptr;
    }

    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    CGContextRelease(context);
    CGImageRelease(image);
    return bitmap;
#else
    return nullptr;
#endif
}

#ifdef _WIN32
bool Bitmap::save(const std::string& filepath) const {
    static bool gdiplusInitialized = false;
    static ULONG_PTR gdiplusToken;

    if (!gdiplusInitialized) {
        Gdiplus::GdiplusStartupInput startupInput = {};
        Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);
        gdiplusInitialized = true;
    }

    Gdiplus::Bitmap gdiBmp(m_width, m_height, m_width * 4,
                          PixelFormat32bppARGB, m_data.get());

    std::wstring widePath(filepath.begin(), filepath.end());

    CLSID pngClsid;
    GUID format = Gdiplus::ImageFormatPNG;
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return false;

    std::vector<Gdiplus::ImageCodecInfo> codecs(size / sizeof(Gdiplus::ImageCodecInfo));
    Gdiplus::GetImageEncoders(num, size, codecs.data());

    for (UINT i = 0; i < num; ++i) {
        if (codecs[i].FormatID == format) {
            pngClsid = codecs[i].Clsid;
            break;
        }
    }

    Gdiplus::Status status = gdiBmp.Save(widePath.c_str(), &pngClsid, nullptr);
    return status == Gdiplus::Ok;
}

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

    auto bitmap = Bitmap::fromHBITMAP(hbitmap);
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
    // Load template image
    cv::Mat templateImg = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (templateImg.empty()) {
        return false;
    }

    // Capture screen region
    auto screenBitmap = capture(region);
    if (!screenBitmap) {
        return false;
    }

    // Convert Bitmap to OpenCV Mat
    int width = screenBitmap->getWidth();
    int height = screenBitmap->getHeight();
    cv::Mat screenMat(height, width, CV_8UC4, screenBitmap->getData());

    // Convert BGRA to BGR
    cv::Mat screenBGR;
    cv::cvtColor(screenMat, screenBGR, cv::COLOR_BGRA2BGR);

    // Skip if template image is too large
    if (templateImg.rows > screenBGR.rows || templateImg.cols > screenBGR.cols) {
        return false;
    }

    // Execute template matching
    cv::Mat matchResult;
    cv::matchTemplate(screenBGR, templateImg, matchResult, cv::TM_CCOEFF_NORMED);

    // Find best match position
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

    // Check threshold
    if (maxVal >= threshold) {
        result.x = region.x + maxLoc.x;
        result.y = region.y + maxLoc.y;
        return true;
    }

    return false;
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

bool Bitmap::save(const std::string& filepath) const {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(filepath.c_str()),
        static_cast<CFIndex>(filepath.size()),
        false
    );
    if (!url) {
        return false;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (!colorSpace) {
        CFRelease(url);
        return false;
    }

    CGContextRef context = CGBitmapContextCreate(
        const_cast<uint8_t*>(m_data.get()),
        static_cast<size_t>(m_width),
        static_cast<size_t>(m_height),
        8,
        static_cast<size_t>(m_width * 4),
        colorSpace,
        static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | kCGBitmapByteOrder32Little
    );
    CGColorSpaceRelease(colorSpace);

    if (!context) {
        CFRelease(url);
        return false;
    }

    CGImageRef image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    if (!image) {
        CFRelease(url);
        return false;
    }

    CGImageDestinationRef destination = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
    CFRelease(url);
    if (!destination) {
        CGImageRelease(image);
        return false;
    }

    CGImageDestinationAddImage(destination, image, nullptr);
    const bool success = CGImageDestinationFinalize(destination);
    CFRelease(destination);
    CGImageRelease(image);
    return success;
}

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
#else
bool Bitmap::save(const std::string& /*filepath*/) const {
    return false;
}

// ============================================================================
// Screen Implementation
// ============================================================================

std::unique_ptr<Bitmap> Screen::capture() {
    return nullptr;
}

std::unique_ptr<Bitmap> Screen::capture(const Rect& /*region*/) {
    return nullptr;
}

Color Screen::getPixel(int /*x*/, int /*y*/) {
    return Color();
}

bool Screen::findColor(const Color& /*color*/, const Rect& /*region*/,
                      int /*tolerance*/, Point& /*result*/) {
    return false;
}

std::vector<Point> Screen::findColors(const Color& /*color*/, const Rect& /*region*/,
                                      int /*tolerance*/, int /*maxCount*/) {
    return {};
}

bool Screen::findImage(const std::string& /*imagePath*/, const Rect& /*region*/,
                       double /*threshold*/, Point& /*result*/) {
    return false;
}

int Screen::getScreenWidth() {
    return 0;
}

int Screen::getScreenHeight() {
    return 0;
}

Rect Screen::getScreenBounds() {
    return Rect();
}

#endif // _WIN32

} // namespace wingman
