#pragma once

#include "wingman/screen.hpp"
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wingman {

#ifdef _WIN32
using WindowHandle = HWND;
#else
using WindowHandle = uint64_t;
#endif

struct WindowInfo {
    WindowHandle handle;
    std::string title;
    Rect bounds;
    bool isForeground;

    WindowInfo() : handle(0), isForeground(false) {}
};

class Window {
public:
    // === Find window ===

    // Find window by title (partial match)
    static WindowHandle find(const std::string& title);

    // Find all windows matching title
    static std::vector<WindowHandle> findAll(const std::string& title);

    // Get foreground window
    static WindowHandle getForeground();

    // Get all window list
    static std::vector<WindowInfo> enumerate();

    // === Window operations ===

    // Activate window (set as foreground)
    static bool activate(WindowHandle hwnd);

    // Minimize window
    static bool minimize(WindowHandle hwnd);

    // Maximize window
    static bool maximize(WindowHandle hwnd);

    // Restore window
    static bool restore(WindowHandle hwnd);

    // Close window
    static bool close(WindowHandle hwnd);

    // === Window information ===

    // Get window title
    static std::string getTitle(WindowHandle hwnd);

    // Get window bounds
    static Rect getBounds(WindowHandle hwnd);

    // Set window position and size
    static bool setBounds(WindowHandle hwnd, const Rect& bounds);

    // Check if window is valid
    static bool isValid(WindowHandle hwnd);

    // Check if window is in foreground
    static bool isForeground(WindowHandle hwnd);

    // Check if window is visible
    static bool isVisible(WindowHandle hwnd);

    // === Move window ===

    // Move window to specified position
    static bool move(WindowHandle hwnd, int x, int y);

    // Resize window
    static bool resize(WindowHandle hwnd, int width, int height);

    // === Utility functions ===

    // 句柄 → 稳定整型 key（watcher/map 消费）。Windows 的 HWND 是不透明
    // 指针，reinterpret_cast 中转收敛在实现层，公共调用点保持平台中立
    static uint64_t key(WindowHandle hwnd);

    // key() 的逆变换（RPC/脚本载荷往返还原句柄）
    static WindowHandle fromKey(uint64_t key);

    // Wait for window to appear
    static bool waitFor(const std::string& title, int timeoutMs = 5000);

    // Wait for window to close
    static bool waitClose(const std::string& title, int timeoutMs = 5000);
};

} // namespace wingman
