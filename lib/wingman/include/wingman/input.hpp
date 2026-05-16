#pragma once

#include "wingman/screen.hpp"
#include <string>
#include <thread>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wingman {

#ifdef _WIN32

enum class MouseButton {
    Left,
    Middle,
    Right
};

class Input {
public:
    static void move(int x, int y);
    static void move(int x, int y, int durationMs);
    static void click(int x, int y, MouseButton button = MouseButton::Left);
    static void doubleClick(int x, int y, MouseButton button = MouseButton::Left);
    static void mouseDown(MouseButton button = MouseButton::Left);
    static void mouseUp(MouseButton button = MouseButton::Left);
    static void scroll(int x, int y, int delta);

    static void keyDown(int vkCode);
    static void keyUp(int vkCode);
    static void key(int vkCode);
    static void type(const std::string& text);
    static void type(const std::string& text, int delayMs);

    static Point getMousePosition();
    static bool isKeyDown(int vkCode);
    static bool isMouseDown(MouseButton button);

    static void delay(int milliseconds);
    static void randomDelay(int minMs, int maxMs);

private:
    static void sendMouseInput(int flags, int dx, int dy, int data);
    static void sendKeyInput(int vkCode, bool isDown);
    static int getMouseFlag(MouseButton button, bool isDown);
};

#endif // _WIN32

} // namespace wingman
