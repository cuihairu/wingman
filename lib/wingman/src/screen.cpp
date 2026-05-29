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

#include <cstring>
#include <algorithm>

namespace wingman {

// ============================================================================
// Bitmap Implementation
// ============================================================================

Bitmap::Bitmap(int width, int height)
    : m_width(width), m_height(height),
      m_data(new uint8_t[width * height * 4]) {}

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

// ============================================================================
// Screen Implementation
// ============================================================================

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

#ifdef _WIN32
bool Screen::findImage(const std::string& imagePath, const Rect& region,
                       double threshold, Point& result) {
    // 加载模板图像
    cv::Mat templateImg = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (templateImg.empty()) {
        return false;
    }

    // 截取屏幕区域
    auto screenBitmap = capture(region);
    if (!screenBitmap) {
        return false;
    }

    // 转换 Bitmap 到 OpenCV Mat
    int width = screenBitmap->getWidth();
    int height = screenBitmap->getHeight();
    cv::Mat screenMat(height, width, CV_8UC4, screenBitmap->getData());

    // 转换 BGRA 到 BGR
    cv::Mat screenBGR;
    cv::cvtColor(screenMat, screenBGR, cv::COLOR_BGRA2BGR);

    // 模板图像太大则跳过
    if (templateImg.rows > screenBGR.rows || templateImg.cols > screenBGR.cols) {
        return false;
    }

    // 执行模板匹配
    cv::Mat matchResult;
    cv::matchTemplate(screenBGR, templateImg, matchResult, cv::TM_CCOEFF_NORMED);

    // 查找最佳匹配位置
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

    // 检查阈值
    if (maxVal >= threshold) {
        result.x = region.x + maxLoc.x;
        result.y = region.y + maxLoc.y;
        return true;
    }

    return false;
}
#endif // _WIN32

int Screen::getScreenWidth() {
    return GetSystemMetrics(SM_CXSCREEN);
}

int Screen::getScreenHeight() {
    return GetSystemMetrics(SM_CYSCREEN);
}

Rect Screen::getScreenBounds() {
    return Rect(0, 0, getScreenWidth(), getScreenHeight());
}

#endif // _WIN32

} // namespace wingman
