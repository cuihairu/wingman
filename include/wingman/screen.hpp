#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace wingman {

struct Point {
    int x;
    int y;

    Point(int x = 0, int y = 0) : x(x), y(y) {}

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct Rect {
    int x;
    int y;
    int width;
    int height;

    Rect(int x = 0, int y = 0, int w = 0, int h = 0)
        : x(x), y(y), width(w), height(h) {}

    bool contains(const Point& p) const {
        return p.x >= x && p.x < x + width &&
               p.y >= y && p.y < y + height;
    }
};

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a = 255;

    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    // 从 0xRRGGBB 格式创建
    static Color fromRGB(uint32_t rgb) {
        return Color(
            static_cast<uint8_t>((rgb >> 16) & 0xFF),
            static_cast<uint8_t>((rgb >> 8) & 0xFF),
            static_cast<uint8_t>(rgb & 0xFF)
        );
    }

    // 转换为 0xRRGGBB 格式
    uint32_t toRGB() const {
        return (r << 16) | (g << 8) | b;
    }

    // 计算颜色差异（欧氏距离）
    int distance(const Color& other) const {
        int dr = r - other.r;
        int dg = g - other.g;
        int db = b - other.b;
        return dr * dr + dg * dg + db * db;
    }

    bool matches(const Color& other, int tolerance) const {
        return distance(other) <= tolerance * tolerance;
    }
};

class Bitmap {
public:
    Bitmap(int width, int height);
    Bitmap(const Bitmap& other);
    ~Bitmap();

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    uint8_t* getData() { return m_data.get(); }
    const uint8_t* getData() const { return m_data.get(); }

    Color getPixel(int x, int y) const;
    void setPixel(int x, int y, const Color& color);

    // 保存为文件
    bool save(const std::string& filepath) const;

    // 从 HBITMAP 创建 (Windows)
    #ifdef _WIN32
    static std::unique_ptr<Bitmap> fromHBITMAP(HBITMAP hbitmap);
    #endif

private:
    int m_width;
    int m_height;
    std::unique_ptr<uint8_t[]> m_data;
};

class Screen {
public:
    // 截取整个屏幕
    static std::unique_ptr<Bitmap> capture();

    // 截取指定区域
    static std::unique_ptr<Bitmap> capture(const Rect& region);

    // 获取指定位置的颜色
    static Color getPixel(int x, int y);

    // 查找单个颜色点
    static bool findColor(const Color& color, const Rect& region,
                         int tolerance, Point& result);

    // 查找所有颜色点
    static std::vector<Point> findColors(const Color& color, const Rect& region,
                                         int tolerance, int maxCount = 0);

    // 图像匹配
    static bool findImage(const std::string& imagePath, const Rect& region,
                          double threshold, Point& result);

    // 获取屏幕尺寸
    static int getScreenWidth();
    static int getScreenHeight();

    // 获取主显示器尺寸
    static Rect getScreenBounds();
};

} // namespace wingman
