#include "wingman/screen.hpp"

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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

Bitmap::~Bitmap() = default;

Color Bitmap::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return Color();
    }
    const uint8_t* p = m_data.get() + (y * m_width + x) * 4;
    return Color(p[2], p[1], p[0], p[3]);  // BGR -> RGB
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
    bmi.bmiHeader.biHeight = -bm.bmHeight;  // 自下而上
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
    // 使用 GDI+ 保存为 PNG
    static bool gdiplusInitialized = false;
    static ULONG_PTR gdiplusToken;

    if (!gdiplusInitialized) {
        Gdiplus::GdiplusStartupInput startupInput = {};
        Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr);
        gdiplusInitialized = true;
    }

    // 创建 GDI+ Bitmap
    Gdiplus::Bitmap gdiBmp(m_width, m_height, m_width * 4,
                          PixelFormat32bppARGB, m_data.get());

    // 转换文件路径
    std::wstring widePath(filepath.begin(), filepath.end());

    // 获取编码器 CLSID
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

    // 保存
    Gdiplus::Status status = gdiBmp.Save(widePath.c_str(), &pngClsid, nullptr);
    return status == Gdiplus::Ok;
}

// ============================================================================
// Screen Implementation (Windows)
// ============================================================================

std::unique_ptr<Bitmap> Screen::capture() {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
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

    // 复制屏幕内容
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

    // 优化：先扫描再检查容差
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
    // TODO: 实现图像匹配
    // 需要加载模板图像，然后在屏幕截图中搜索
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

#else

// ============================================================================
// Screen Implementation (Linux/X11)
// ============================================================================

static Display* g_display = nullptr;
static bool g_displayOpened = false;

static Display* getDisplay() {
    if (!g_displayOpened) {
        g_display = XOpenDisplay(nullptr);
        g_displayOpened = true;
    }
    return g_display;
}

std::unique_ptr<Bitmap> Screen::capture() {
    Display* display = getDisplay();
    if (!display) {
        return nullptr;
    }

    Window root = DefaultRootWindow(display);
    int width = DisplayWidth(display, DefaultScreen(display));
    int height = DisplayHeight(display, DefaultScreen(display));

    XImage* ximage = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!ximage) {
        return nullptr;
    }

    auto bitmap = std::make_unique<Bitmap>(width, height);
    uint8_t* data = bitmap->getData();

    // 转换 XImage 数据
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned long pixel = XGetPixel(ximage, x, y);
            uint8_t* p = data + (y * width + x) * 4;
            p[0] = (pixel >> 16) & 0xFF;  // R
            p[1] = (pixel >> 8) & 0xFF;   // G
            p[2] = pixel & 0xFF;          // B
            p[3] = 255;                   // A
        }
    }

    XDestroyImage(ximage);
    return bitmap;
}

std::unique_ptr<Bitmap> Screen::capture(const Rect& region) {
    auto full = capture();
    if (!full) {
        return nullptr;
    }

    auto result = std::make_unique<Bitmap>(region.width, region.height);
    const uint8_t* src = full->getData();
    uint8_t* dst = result->getData();

    for (int y = 0; y < region.height; ++y) {
        int srcY = region.y + y;
        if (srcY >= full->getHeight()) break;
        for (int x = 0; x < region.width; ++x) {
            int srcX = region.x + x;
            if (srcX >= full->getWidth()) break;
            std::memcpy(dst + (y * region.width + x) * 4,
                       src + (srcY * full->getWidth() + srcX) * 4, 4);
        }
    }

    return result;
}

Color Screen::getPixel(int x, int y) {
    Display* display = getDisplay();
    if (!display) {
        return Color();
    }

    Window root = DefaultRootWindow(display);
    XImage* ximage = XGetImage(display, root, x, y, 1, 1, AllPlanes, ZPixmap);
    if (!ximage) {
        return Color();
    }

    unsigned long pixel = XGetPixel(ximage, 0, 0);
    Color color(
        (pixel >> 16) & 0xFF,
        (pixel >> 8) & 0xFF,
        pixel & 0xFF
    );

    XDestroyImage(ximage);
    return color;
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
    return false;
}

int Screen::getScreenWidth() {
    Display* display = getDisplay();
    return display ? DisplayWidth(display, DefaultScreen(display)) : 0;
}

int Screen::getScreenHeight() {
    Display* display = getDisplay();
    return display ? DisplayHeight(display, DefaultScreen(display)) : 0;
}

Rect Screen::getScreenBounds() {
    return Rect(0, 0, getScreenWidth(), getScreenHeight());
}

#endif

} // namespace wingman
