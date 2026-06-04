#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wingman {

struct Point {
    int x;
    int y;

    Point(int x = 0, int y = 0) : x(x), y(y) {}

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Point& other) const {
        return !(*this == other);
    }
};

struct Rect {
    int x;
    int y;
    int width;
    int height;

    Rect(int x = 0, int y = 0, int w = 0, int h = 0)
        : x(x), y(y), width(w), height(h) {}

    bool isEmpty() const {
        return width <= 0 || height <= 0;
    }

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

    // Create from 0xRRGGBB format
    static Color fromRGB(uint32_t rgb) {
        return Color(
            static_cast<uint8_t>((rgb >> 16) & 0xFF),
            static_cast<uint8_t>((rgb >> 8) & 0xFF),
            static_cast<uint8_t>(rgb & 0xFF)
        );
    }

    // Convert to 0xRRGGBB format
    uint32_t toRGB() const {
        return (r << 16) | (g << 8) | b;
    }

    // Calculate color distance (Euclidean distance)
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
    Bitmap(Bitmap&& other) noexcept;
    ~Bitmap();

    Bitmap& operator=(const Bitmap& other);
    Bitmap& operator=(Bitmap&& other) noexcept;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    uint8_t* getData() { return m_data.get(); }
    const uint8_t* getData() const { return m_data.get(); }

    Color getPixel(int x, int y) const;
    void setPixel(int x, int y, const Color& color);

    // Save to file
    bool save(const std::string& filepath) const;

    // Load from image file (PNG, BMP, JPG, etc.)
    static std::unique_ptr<Bitmap> fromFile(const std::string& filepath);

#ifdef _WIN32
    // Create from HBITMAP
    static std::unique_ptr<Bitmap> fromHBITMAP(HBITMAP hbitmap);
#endif

private:
    int m_width;
    int m_height;
    std::unique_ptr<uint8_t[]> m_data;
};

class Screen {
public:
    // Capture entire screen
    static std::unique_ptr<Bitmap> capture();

    // Capture specified region
    static std::unique_ptr<Bitmap> capture(const Rect& region);

    // Get color at specified position
    static Color getPixel(int x, int y);

    // Find single color point
    static bool findColor(const Color& color, const Rect& region,
                         int tolerance, Point& result);

    // Find all color points
    static std::vector<Point> findColors(const Color& color, const Rect& region,
                                         int tolerance, int maxCount = 0);

    // Image matching
    static bool findImage(const std::string& imagePath, const Rect& region,
                          double threshold, Point& result);

    // Get screen dimensions
    static int getScreenWidth();
    static int getScreenHeight();

    // Get primary monitor bounds
    static Rect getScreenBounds();
};

} // namespace wingman
