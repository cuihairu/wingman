#pragma once

#include "wingman/screen.hpp"
#include <string>
#include <vector>
#include <windows.h>

namespace wingman {

using WindowHandle = HWND;

struct WindowInfo {
    WindowHandle handle;
    std::string title;
    Rect bounds;
    bool isForeground;

    WindowInfo() : handle(0), isForeground(false) {}
};

class Window {
public:
    // === 查找窗口 ===

    // 按标题查找窗口（部分匹配）
    static WindowHandle find(const std::string& title);

    // 查找所有匹配标题的窗口
    static std::vector<WindowHandle> findAll(const std::string& title);

    // 获取前台窗口
    static WindowHandle getForeground();

    // 获取所有窗口列表
    static std::vector<WindowInfo> enumerate();

    // === 窗口操作 ===

    // 激活窗口（设置为前台）
    static bool activate(WindowHandle hwnd);

    // 最小化窗口
    static bool minimize(WindowHandle hwnd);

    // 最大化窗口
    static bool maximize(WindowHandle hwnd);

    // 还原窗口
    static bool restore(WindowHandle hwnd);

    // 关闭窗口
    static bool close(WindowHandle hwnd);

    // === 窗口信息 ===

    // 获取窗口标题
    static std::string getTitle(WindowHandle hwnd);

    // 获取窗口边界
    static Rect getBounds(WindowHandle hwnd);

    // 设置窗口位置和大小
    static bool setBounds(WindowHandle hwnd, const Rect& bounds);

    // 检查窗口是否有效
    static bool isValid(WindowHandle hwnd);

    // 检查窗口是否在前台
    static bool isForeground(WindowHandle hwnd);

    // 检查窗口是否可见
    static bool isVisible(WindowHandle hwnd);

    // === 移动窗口 ===

    // 移动窗口到指定位置
    static bool move(WindowHandle hwnd, int x, int y);

    // 调整窗口大小
    static bool resize(WindowHandle hwnd, int width, int height);

    // === 工具函数 ===

    // 等待窗口出现
    static bool waitFor(const std::string& title, int timeoutMs = 5000);

    // 等待窗口消失
    static bool waitClose(const std::string& title, int timeoutMs = 5000);
};

} // namespace wingman
