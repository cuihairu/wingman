// Bitmap 纯核心实现（A2，自 screen.cpp 抽出）。
//
// 只包含零平台宏依赖的 Bitmap 内存语义（构造/拷贝/移动/赋值/像素读写），
// 使 Android NDK 构建与本机 lua_tests 都能轻量链接 Bitmap，而无需拖入
// screen.cpp 的平台分支（Win GDI+/mac CoreGraphics/Linux X11——注意
// __ANDROID__ 亦定义 __linux__，X11 装配在 NDK 下不适用）。
//
// fromFile/save/平台 Screen 实现仍在 screen.cpp（该文件在平台边界
// allowlist 内，平台宏改动不新增清单条目）。

#include "wingman/screen.hpp"

#include <cstring>

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

} // namespace wingman
