// Bitmap 图片文件编解码（fromFile/save）平台实现，自 screen.cpp 整体迁入：
// Windows GDI+ / macOS ImageIO / OpenCV（WINGMAN_ENABLE_VISION）/ BMP 兜底
// 四条路径原样保留；平台分支收口在 platform 分区（边界守卫豁免区）。
// Bitmap 的纯核心（构造/拷贝/移动/像素读写）仍在 bitmap.cpp（A2，Android
// NDK 与 lua_tests 轻量链接用）。
#include "wingman/screen.hpp"
#include "platform/screen_image_codec.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <comdef.h>
#include <gdiplus.h>
#include <opencv2/opencv.hpp>
#pragma comment(lib, "gdiplus.lib")
#endif

#ifdef WINGMAN_ENABLE_VISION
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <ImageIO/ImageIO.h>
#include <unistd.h>
#endif

#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <vector>

namespace wingman {

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(WINGMAN_ENABLE_VISION)
namespace {

#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t signature;
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixelDataOffset;
};

struct BmpInfoHeader {
    uint32_t headerSize;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t importantColors;
};
#pragma pack(pop)

constexpr uint16_t kBmpSignature = 0x4D42;
constexpr uint32_t kBmpCompressionRgb = 0;

} // namespace
#endif

#if defined(_WIN32) || defined(WINGMAN_ENABLE_VISION)

// 模板匹配核心（Windows 与 Linux vision 构建共用）：imread 模板 → 截图
// BGRA 数据转 BGR → TM_CCOEFF_NORMED → minMaxLoc 阈值判定，单尺度。
// regionOrigin 为截图区域在屏幕上的原点（结果坐标平移回屏幕坐标系用）。
// 非静态：screen_backend.cpp 的 Screen::findImage 跨 TU 调用（与 header
// 声明保持外部链接，否则 Windows 链接期 LNK2019）。
bool matchTemplateOnBitmap(const std::string& imagePath, uint8_t* bgraData,
                           int width, int height, const Point& regionOrigin,
                           double threshold, Point& result) {
    const cv::Mat templateImg = cv::imread(imagePath, cv::IMREAD_COLOR);
    if (templateImg.empty()) {
        return false;
    }

    const cv::Mat screenMat(height, width, CV_8UC4, bgraData);
    cv::Mat screenBGR;
    cv::cvtColor(screenMat, screenBGR, cv::COLOR_BGRA2BGR);

    // 模板大于搜索区域，必然无匹配
    if (templateImg.rows > screenBGR.rows || templateImg.cols > screenBGR.cols) {
        return false;
    }

    cv::Mat matchResult;
    cv::matchTemplate(screenBGR, templateImg, matchResult, cv::TM_CCOEFF_NORMED);

    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(matchResult, &minVal, &maxVal, &minLoc, &maxLoc);

    if (maxVal >= threshold) {
        result.x = regionOrigin.x + maxLoc.x;
        result.y = regionOrigin.y + maxLoc.y;
        return true;
    }
    return false;
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
#elif defined(WINGMAN_ENABLE_VISION)
    cv::Mat image = cv::imread(filepath, cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        return nullptr;
    }

    cv::Mat bgra;
    switch (image.channels()) {
        case 1:
            cv::cvtColor(image, bgra, cv::COLOR_GRAY2BGRA);
            break;
        case 3:
            cv::cvtColor(image, bgra, cv::COLOR_BGR2BGRA);
            break;
        case 4:
            bgra = image;
            break;
        default:
            return nullptr;
    }

    if (!bgra.isContinuous()) {
        bgra = bgra.clone();
    }

    auto bitmap = std::make_unique<Bitmap>(bgra.cols, bgra.rows);
    std::memcpy(bitmap->getData(), bgra.data, static_cast<size_t>(bgra.total() * bgra.elemSize()));
    return bitmap;
#else
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        return nullptr;
    }

    BmpFileHeader fileHeader{};
    BmpInfoHeader infoHeader{};
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    if (!file) {
        return nullptr;
    }

    if (fileHeader.signature != kBmpSignature ||
        infoHeader.headerSize < sizeof(BmpInfoHeader) ||
        infoHeader.width <= 0 ||
        infoHeader.height == 0 ||
        infoHeader.planes != 1 ||
        infoHeader.compression != kBmpCompressionRgb ||
        (infoHeader.bitCount != 24 && infoHeader.bitCount != 32)) {
        return nullptr;
    }

    const int width = infoHeader.width;
    const int height = std::abs(infoHeader.height);
    const bool isTopDown = infoHeader.height < 0;
    const size_t bytesPerPixel = infoHeader.bitCount / 8;
    const size_t rowStride = ((static_cast<size_t>(width) * bytesPerPixel) + 3U) & ~size_t{3};

    auto bitmap = std::make_unique<Bitmap>(width, height);
    std::vector<uint8_t> row(rowStride);

    file.seekg(static_cast<std::streamoff>(fileHeader.pixelDataOffset), std::ios::beg);
    if (!file) {
        return nullptr;
    }

    for (int fileRow = 0; fileRow < height; ++fileRow) {
        file.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size()));
        if (!file) {
            return nullptr;
        }

        const int targetRow = isTopDown ? fileRow : (height - 1 - fileRow);
        uint8_t* dst = bitmap->getData() + static_cast<size_t>(targetRow) * static_cast<size_t>(width) * 4U;
        for (int x = 0; x < width; ++x) {
            const size_t srcOffset = static_cast<size_t>(x) * bytesPerPixel;
            const size_t dstOffset = static_cast<size_t>(x) * 4U;
            dst[dstOffset + 0] = row[srcOffset + 0];
            dst[dstOffset + 1] = row[srcOffset + 1];
            dst[dstOffset + 2] = row[srcOffset + 2];
            dst[dstOffset + 3] = bytesPerPixel == 4 ? row[srcOffset + 3] : 255;
        }
    }

    return bitmap;
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
#elif defined(__APPLE__)
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
#elif defined(WINGMAN_ENABLE_VISION)
bool Bitmap::save(const std::string& filepath) const {
    if (m_width <= 0 || m_height <= 0 || !m_data) {
        return false;
    }

    cv::Mat bgra(m_height, m_width, CV_8UC4, const_cast<uint8_t*>(m_data.get()));
    // OpenCV 的 BMP/JPEG 编码器不收 4 通道：这三类后缀先 BGRA→BGR；
    // PNG 保留 alpha 直写。imwrite 对编不了的格式是抛异常而非返回 false，
    // 兜底转 false 守住「失败返回 false」契约（batch11 save 分支依赖）
    const std::string lower = [&] {
        std::string out = filepath;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }();
    const bool needs3ch =
        lower.size() >= 4 && (lower.compare(lower.size() - 4, 4, ".bmp") == 0
                              || lower.compare(lower.size() - 4, 4, ".jpg") == 0);
    const bool needs3chJpeg = lower.size() >= 5
                              && lower.compare(lower.size() - 5, 5, ".jpeg") == 0;
    try {
        if (needs3ch || needs3chJpeg) {
            cv::Mat bgr;
            cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);
            return cv::imwrite(filepath, bgr);
        }
        return cv::imwrite(filepath, bgra);
    } catch (const cv::Exception&) {
        return false;
    }
}
#else
bool Bitmap::save(const std::string& filepath) const {
    if (m_width <= 0 || m_height <= 0 || !m_data) {
        return false;
    }

    const size_t rowStride = ((static_cast<size_t>(m_width) * 3U) + 3U) & ~size_t{3};
    const uint32_t imageSize = static_cast<uint32_t>(rowStride * static_cast<size_t>(m_height));

    BmpFileHeader fileHeader{
        kBmpSignature,
        static_cast<uint32_t>(sizeof(BmpFileHeader) + sizeof(BmpInfoHeader)) + imageSize,
        0,
        0,
        static_cast<uint32_t>(sizeof(BmpFileHeader) + sizeof(BmpInfoHeader))
    };
    BmpInfoHeader infoHeader{
        sizeof(BmpInfoHeader),
        m_width,
        m_height,
        1,
        24,
        kBmpCompressionRgb,
        imageSize,
        0,
        0,
        0,
        0
    };

    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    if (!file) {
        return false;
    }

    std::vector<uint8_t> row(rowStride, 0);
    for (int y = m_height - 1; y >= 0; --y) {
        const uint8_t* src = m_data.get() + static_cast<size_t>(y) * static_cast<size_t>(m_width) * 4U;
        for (int x = 0; x < m_width; ++x) {
            const size_t srcOffset = static_cast<size_t>(x) * 4U;
            const size_t dstOffset = static_cast<size_t>(x) * 3U;
            row[dstOffset + 0] = src[srcOffset + 0];
            row[dstOffset + 1] = src[srcOffset + 1];
            row[dstOffset + 2] = src[srcOffset + 2];
        }
        file.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
        if (!file) {
            return false;
        }
    }

    return true;
}
#endif // platform-specific bitmap image codec

} // namespace wingman
