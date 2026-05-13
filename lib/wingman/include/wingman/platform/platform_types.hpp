#pragma once

#include <string>
#include <cstdint>
#include "wingman/screen.hpp"

namespace wingman::platform {

/**
 * @brief 捕获后端类型
 */
enum class CaptureBackend : uint8_t {
    Auto,               // 自动选择最佳实现
    // Windows 后端
    GDI,                // Windows GDI+
    DXGI,               // Windows DXGI Desktop Duplication
    DirectX,            // Windows DirectX
    WindowsGraphicsCapture, // Windows.Graphics.Capture API
    // macOS 后端
    ScreenCaptureKit,   // macOS ScreenCaptureKit (推荐)
    CoreGraphics,       // macOS Core Graphics
    // Linux 后端
    X11,                // Linux X11
    PipeWire,           // Linux PipeWire
    // 测试用
    Mock,               // Mock 实现（用于测试）
};

/**
 * @brief 输入后端类型
 */
enum class InputBackend : uint8_t {
    Auto,
    // Windows 后端
    SendInput,          // Windows SendInput API
    DirectInput,        // Windows DirectInput
    // macOS 后端
    CGEvent,            // macOS CGEvent
    NSEvent,            // macOS NSEvent
    // Linux 后端
    XTest,              // Linux XTest Extension
    libinput,           // Linux libinput (evdev)
    // 测试用
    Mock,
};

/**
 * @brief 鼠标按钮
 */
enum class MouseButton : uint8_t {
    None,
    Left,
    Right,
    Middle,
    X1,
    X2,
};

/**
 * @brief 键码
 */
enum class KeyCode : uint16_t {
    // 修饰键
    Shift = 0x10,
    Control = 0x11,
    Alt = 0x12,
    Meta = 0x5B,      // Windows 键 / Command 键

    // 字母键
    A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 数字键
    D0 = 0x30, D1, D2, D3, D4, D5, D6, D7, D8, D9,

    // 功能键
    F1 = 0x70, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // 特殊键
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

    // 小键盘
    Numpad0 = 0x60, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadAdd = 0x6B,
    NumpadSubtract = 0x6D,
    NumpadMultiply = 0x6A,
    NumpadDivide = 0x6F,
    NumpadDecimal = 0x6E,
};

/**
 * @brief 显示模式
 */
struct DisplayMode {
    int width;
    int height;
    int refreshRate;      // Hz
    int bitDepth;         // 色深

    bool operator==(const DisplayMode& other) const {
        return width == other.width && height == other.height &&
               refreshRate == other.refreshRate && bitDepth == other.bitDepth;
    }
};

/**
 * @brief 捕获配置
 */
struct CaptureConfig {
    CaptureBackend preferredBackend = CaptureBackend::Auto;
    int monitorIndex = 0;
    int fps = 60;
    bool includeCursor = true;
    bool cursorHighlight = false;  // 高亮显示光标
    int maxRetries = 3;            // 捕获失败重试次数
};

/**
 * @brief 输入配置
 */
struct InputConfig {
    InputBackend preferredBackend = InputBackend::Auto;
    bool simulateHardwareInput = true;   // 硬件模拟 vs 软件模拟
    int inputDelay = 0;                  // 输入延迟（微秒），用于防检测
};

/**
 * @brief 窗口句柄类型
 */
#if defined(_WIN32)
using WindowHandle = HWND;
#elif defined(__APPLE__)
using WindowHandle = void*;  // CGWindowID 或 NSWindow*
#else
using WindowHandle = unsigned long;
#endif

/**
 * @brief 窗口信息
 */
struct WindowInfo {
    WindowHandle handle;
    std::string title;
    Rect bounds;
    bool isForeground;
    uint32_t processId;
};

/**
 * @brief 后端信息
 */
struct BackendInfo {
    std::string name;
    std::string version;
    bool isAvailable;
    std::string description;
};

/**
 * @brief 获取后端名称
 */
inline std::string getCaptureBackendName(CaptureBackend backend) {
    switch (backend) {
        case CaptureBackend::Auto: return "Auto";
        case CaptureBackend::GDI: return "GDI";
        case CaptureBackend::DXGI: return "DXGI";
        case CaptureBackend::DirectX: return "DirectX";
        case CaptureBackend::WindowsGraphicsCapture: return "WindowsGraphicsCapture";
        case CaptureBackend::ScreenCaptureKit: return "ScreenCaptureKit";
        case CaptureBackend::CoreGraphics: return "CoreGraphics";
        case CaptureBackend::X11: return "X11";
        case CaptureBackend::PipeWire: return "PipeWire";
        case CaptureBackend::Mock: return "Mock";
        default: return "Unknown";
    }
}

inline std::string getInputBackendName(InputBackend backend) {
    switch (backend) {
        case InputBackend::Auto: return "Auto";
        case InputBackend::SendInput: return "SendInput";
        case InputBackend::DirectInput: return "DirectInput";
        case InputBackend::CGEvent: return "CGEvent";
        case InputBackend::NSEvent: return "NSEvent";
        case InputBackend::XTest: return "XTest";
        case InputBackend::libinput: return "libinput";
        case InputBackend::Mock: return "Mock";
        default: return "Unknown";
    }
}

} // namespace wingman::platform
