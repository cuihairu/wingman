#pragma once

#include "wingman/screen.hpp"
#include <string>
#include <thread>
#include <chrono>

namespace wingman {

// 鼠标按钮
enum class MouseButton {
    Left,
    Middle,
    Right
};

// 按键状态
enum class KeyState {
    Pressed,
    Released
};

class Input {
public:
    // === 鼠标操作 ===

    // 移动鼠标到指定位置
    static void move(int x, int y);

    // 移动鼠标并带持续时间（贝塞尔曲线平滑）
    static void move(int x, int y, int durationMs);

    // 点击鼠标
    static void click(int x, int y, MouseButton button = MouseButton::Left);

    // 双击
    static void doubleClick(int x, int y, MouseButton button = MouseButton::Left);

    // 按下鼠标按钮
    static void mouseDown(MouseButton button = MouseButton::Left);

    // 释放鼠标按钮
    static void mouseUp(MouseButton button = MouseButton::Left);

    // 滚动鼠标滚轮
    static void scroll(int x, int y, int delta);

    // === 键盘操作 ===

    // 按下按键
    static void keyDown(int vkCode);

    // 释放按键
    static void keyUp(int vkCode);

    // 按键（按下+释放）
    static void key(int vkCode);

    // 发送文本
    static void type(const std::string& text);

    // 发送文本带延迟
    static void type(const std::string& text, int delayMs);

    // === 状态查询 ===

    // 获取当前鼠标位置
    static Point getMousePosition();

    // 检查按键是否按下
    static bool isKeyDown(int vkCode);

    // 检查鼠标按钮是否按下
    static bool isMouseDown(MouseButton button);

    // === 工具函数 ===

    // 延迟
    static void delay(int milliseconds);

    // 生成随机延迟
    static void randomDelay(int minMs, int maxMs);

private:
#ifdef _WIN32
    // 发送鼠标输入
    static void sendMouseInput(int flags, int dx, int dy, int data);

    // 发送键盘输入
    static void sendKeyInput(int vkCode, bool isDown);

    // 鼠标输入标志
    static int getMouseFlag(MouseButton button, bool isDown);
#endif
};

} // namespace wingman
