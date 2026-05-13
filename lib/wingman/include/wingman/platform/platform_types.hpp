#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <optional>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace wingman::platform {

// ========== 通用类型定义 ==========

/**
 * @brief 点坐标
 */
struct Point {
    int x = 0;
    int y = 0;

    Point() = default;
    Point(int x, int y) : x(x), y(y) {}

    bool isEmpty() const { return x == 0 && y == 0; }
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

/**
 * @brief 矩形区域
 */
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    Rect() = default;
    Rect(int x, int y, int width, int height)
        : x(x), y(y), width(width), height(height) {}

    bool isEmpty() const { return width <= 0 || height <= 0; }
    bool contains(const Point& p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }
    bool operator==(const Rect& other) const {
        return x == other.x && y == other.y && width == other.width && height == other.height;
    }
    bool operator!=(const Rect& other) const { return !(*this == other); }
};

/**
 * @brief 显示模式
 */
struct DisplayMode {
    int width = 0;
    int height = 0;
    int refreshRate = 60;
    int bitDepth = 32;

    DisplayMode() = default;
    DisplayMode(int width, int height, int refreshRate = 60, int bitDepth = 32)
        : width(width), height(height), refreshRate(refreshRate), bitDepth(bitDepth) {}
};

/**
 * @brief 窗口句柄类型
 */
#ifdef _WIN32
using WindowHandle = HWND;
static constexpr WindowHandle NullWindowHandle = nullptr;
#else
using WindowHandle = uint64_t;
static constexpr WindowHandle NullWindowHandle = 0;
#endif

/**
 * @brief 窗口信息
 */
struct WindowInfo {
    WindowHandle handle = NullWindowHandle;
    std::string title;
    Rect bounds;
    uint32_t processId = 0;
    bool isForeground = false;
};

/**
 * @brief 后端信息
 */
struct BackendInfo {
    std::string name;
    std::string version;
    bool isInitialized = false;
    std::string description;
};

// ========== 枚举类型 ==========

/**
 * @brief 鼠标按钮
 */
enum class MouseButton : uint8_t {
    Left,
    Middle,
    Right,
    X1,
    X2
};

/**
 * @brief 键码
 */
enum class KeyCode : uint32_t {
    // 字母键
    A = 0x41, B = 0x42, C = 0x43, D = 0x44, E = 0x45, F = 0x46, G = 0x47,
    H = 0x48, I = 0x49, J = 0x4A, K = 0x4B, L = 0x4C, M = 0x4D, N = 0x4E,
    O = 0x4F, P = 0x50, Q = 0x51, R = 0x52, S = 0x53, T = 0x54, U = 0x55,
    V = 0x56, W = 0x57, X = 0x58, Y = 0x59, Z = 0x5A,

    // 数字键
    Num0 = 0x30, Num1 = 0x31, Num2 = 0x32, Num3 = 0x33, Num4 = 0x34,
    Num5 = 0x35, Num6 = 0x36, Num7 = 0x37, Num8 = 0x38, Num9 = 0x39,

    // 功能键
    F1 = 0x70, F2 = 0x71, F3 = 0x72, F4 = 0x73, F5 = 0x74, F6 = 0x75,
    F7 = 0x76, F8 = 0x77, F9 = 0x78, F10 = 0x79, F11 = 0x7A, F12 = 0x7B,

    // 控制键
    Space = 0x20,
    Enter = 0x0D,
    Escape = 0x1B,
    Tab = 0x09,
    Backspace = 0x08,
    Delete = 0x2E,
    Insert = 0x2D,
    Home = 0x24,
    End = 0x23,
    PageUp = 0x21,
    PageDown = 0x22,

    // 方向键
    Left = 0x25,
    Up = 0x26,
    Right = 0x27,
    Down = 0x28,

    // 修饰键
    Shift = 0x10,
    Control = 0x11,
    Alt = 0x12,

    // 其他
    CapsLock = 0x14,
    ScrollLock = 0x91,
    PrintScreen = 0x2A,
    Pause = 0x13
};

/**
 * @brief 捕获后端类型
 */
enum class CaptureBackend : uint8_t {
    Auto,
    GDI,
    DXGI,
    ScreenCaptureKit,
    CoreGraphics,
    X11,
    PipeWire
};

/**
 * @brief 输入后端类型
 */
enum class InputBackend : uint8_t {
    Auto,
    SendInput,
    CGEvent,
    XTest,
    libinput
};

/**
 * @brief 窗口后端类型
 */
enum class WindowBackend : uint8_t {
    Auto,
    Win32,
    Cocoa,
    X11
};

/**
 * @brief 屏幕后端类型
 */
enum class ScreenBackend : uint8_t {
    Auto,
    Win32,
    Cocoa,
    X11
};

} // namespace wingman::platform
