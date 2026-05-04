#include "wingman/input.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#endif

#include <random>
#include <cmath>
#include <algorithm>

namespace wingman {

// ============================================================================
// 工具函数
// ============================================================================

void Input::delay(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void Input::randomDelay(int minMs, int maxMs) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(minMs, maxMs);
    delay(dist(gen));
}

// ============================================================================
// Windows 实现
// ============================================================================

#ifdef _WIN32

Point Input::getMousePosition() {
    POINT pt;
    GetCursorPos(&pt);
    return Point(pt.x, pt.y);
}

void Input::move(int x, int y) {
    SetCursorPos(x, y);
}

void Input::move(int x, int y, int durationMs) {
    Point current = getMousePosition();

    if (durationMs <= 10 || current.x == x && current.y == y) {
        move(x, y);
        return;
    }

    // 使用贝塞尔曲线生成平滑路径
    int steps = durationMs / 10;
    float tStep = 1.0f / steps;

    for (int i = 1; i <= steps; ++i) {
        float t = tStep * i;

        // 二次贝塞尔曲线
        int controlX = current.x + (x - current.x) / 2;
        int controlY = current.y + (y - current.y) / 2;

        // 添加一些随机性
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> jitter(-5, 5);
        controlX += jitter(gen);
        controlY += jitter(gen);

        // 贝塞尔曲线公式
        int newX = static_cast<int>(
            std::pow(1 - t, 2) * current.x +
            2 * (1 - t) * t * controlX +
            std::pow(t, 2) * x
        );
        int newY = static_cast<int>(
            std::pow(1 - t, 2) * current.y +
            2 * (1 - t) * t * controlY +
            std::pow(t, 2) * y
        );

        move(newX, newY);
        delay(10);
    }
}

int Input::getMouseFlag(MouseButton button, bool isDown) {
    int base = 0;
    switch (button) {
        case MouseButton::Left:
            base = MOUSEEVENTF_LEFTDOWN;
            break;
        case MouseButton::Right:
            base = MOUSEEVENTF_RIGHTDOWN;
            break;
        case MouseButton::Middle:
            base = MOUSEEVENTF_MIDDLEDOWN;
            break;
    }
    return isDown ? base : (base << 1);
}

void Input::sendMouseInput(int flags, int dx, int dy, int data) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = flags;
    input.mi.mouseData = data;
    SendInput(1, &input, sizeof(INPUT));
}

void Input::click(int x, int y, MouseButton button) {
    move(x, y);
    randomDelay(10, 30);  // 添加人性化随机延迟
    sendMouseInput(getMouseFlag(button, true), 0, 0, 0);
    randomDelay(10, 30);
    sendMouseInput(getMouseFlag(button, false), 0, 0, 0);
}

void Input::doubleClick(int x, int y, MouseButton button) {
    click(x, y, button);
    randomDelay(50, 100);
    click(x, y, button);
}

void Input::mouseDown(MouseButton button) {
    sendMouseInput(getMouseFlag(button, true), 0, 0, 0);
}

void Input::mouseUp(MouseButton button) {
    sendMouseInput(getMouseFlag(button, false), 0, 0, 0);
}

void Input::scroll(int x, int y, int delta) {
    move(x, y);
    sendMouseInput(MOUSEEVENTF_WHEEL, 0, 0, delta * WHEEL_DELTA);
}

void Input::sendKeyInput(int vkCode, bool isDown) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vkCode);
    input.ki.dwFlags = isDown ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void Input::keyDown(int vkCode) {
    sendKeyInput(vkCode, true);
}

void Input::keyUp(int vkCode) {
    sendKeyInput(vkCode, false);
}

void Input::key(int vkCode) {
    keyDown(vkCode);
    randomDelay(10, 30);
    keyUp(vkCode);
}

void Input::type(const std::string& text) {
    type(text, 10);
}

void Input::type(const std::string& text, int delayMs) {
    for (char c : text) {
        SHORT vk = VkKeyScanA(c);
        if (vk != -1) {
            int scanCode = MapVirtualKeyA(vk & 0xFF, 0);

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = static_cast<WORD>(vk & 0xFF);
            inputs[0].ki.wScan = static_cast<WORD>(scanCode);
            inputs[0].ki.dwFlags = (vk & 0x100) ? 2 : 0;  // Shift key

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = static_cast<WORD>(vk & 0xFF);
            inputs[1].ki.wScan = static_cast<WORD>(scanCode);
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP | ((vk & 0x100) ? 2 : 0);

            SendInput(2, inputs, sizeof(INPUT));
        }
        randomDelay(delayMs - 5, delayMs + 5);
    }
}

bool Input::isKeyDown(int vkCode) {
    return (GetKeyState(vkCode) & 0x8000) != 0;
}

bool Input::isMouseDown(MouseButton button) {
    switch (button) {
        case MouseButton::Left:
            return (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
        case MouseButton::Right:
            return (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
        case MouseButton::Middle:
            return (GetKeyState(VK_MBUTTON) & 0x8000) != 0;
    }
    return false;
}

// ============================================================================
// Linux/X11 实现
// ============================================================================

#else

static Display* getInputDisplay() {
    static Display* display = nullptr;
    if (!display) {
        display = XOpenDisplay(nullptr);
    }
    return display;
}

Point Input::getMousePosition() {
    Display* display = getInputDisplay();
    if (!display) {
        return Point();
    }

    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    XQueryPointer(display, DefaultRootWindow(display),
                 &root, &child, &rootX, &rootY, &winX, &winY, &mask);

    return Point(rootX, rootY);
}

void Input::move(int x, int y) {
    Display* display = getInputDisplay();
    if (!display) {
        return;
    }

    XWarpPointer(display, None, DefaultRootWindow(display),
                 0, 0, 0, 0, x, y);
    XFlush(display);
}

void Input::move(int x, int y, int durationMs) {
    Point current = getMousePosition();
    if (durationMs <= 10 || (current.x == x && current.y == y)) {
        move(x, y);
        return;
    }

    int steps = durationMs / 10;
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        int newX = static_cast<int>(current.x + (x - current.x) * t);
        int newY = static_cast<int>(current.y + (y - current.y) * t);
        move(newX, newY);
        delay(10);
    }
}

void Input::click(int x, int y, MouseButton button) {
    move(x, y);
    delay(20);

    Display* display = getInputDisplay();
    if (!display) {
        return;
    }

    unsigned int buttonCode = Button1;
    switch (button) {
        case MouseButton::Left:
            buttonCode = Button1;
            break;
        case MouseButton::Right:
            buttonCode = Button3;
            break;
        case MouseButton::Middle:
            buttonCode = Button2;
            break;
    }

    XTestFakeButtonEvent(display, buttonCode, True, CurrentTime);
    XFlush(display);
    delay(20);
    XTestFakeButtonEvent(display, buttonCode, False, CurrentTime);
    XFlush(display);
}

void Input::doubleClick(int x, int y, MouseButton button) {
    click(x, y, button);
    delay(50);
    click(x, y, button);
}

void Input::mouseDown(MouseButton button) {
    Display* display = getInputDisplay();
    if (!display) {
        return;
    }

    unsigned int buttonCode = Button1;
    switch (button) {
        case MouseButton::Left:
            buttonCode = Button1;
            break;
        case MouseButton::Right:
            buttonCode = Button3;
            break;
        case MouseButton::Middle:
            buttonCode = Button2;
            break;
    }

    XTestFakeButtonEvent(display, buttonCode, True, CurrentTime);
    XFlush(display);
}

void Input::mouseUp(MouseButton button) {
    Display* display = getInputDisplay();
    if (!display) {
        return;
    }

    unsigned int buttonCode = Button1;
    switch (button) {
        case MouseButton::Left:
            buttonCode = Button1;
            break;
        case MouseButton::Right:
            buttonCode = Button3;
            break;
        case MouseButton::Middle:
            buttonCode = Button2;
            break;
    }

    XTestFakeButtonEvent(display, buttonCode, False, CurrentTime);
    XFlush(display);
}

void Input::scroll(int x, int y, int delta) {
    move(x, y);
    Display* display = getInputDisplay();
    if (!display) {
        return;
    }

    int button = (delta > 0) ? Button4 : Button5;
    int clicks = std::abs(delta) / 120;

    for (int i = 0; i < clicks; ++i) {
        XTestFakeButtonEvent(display, button, True, CurrentTime);
        XFlush(display);
        XTestFakeButtonEvent(display, button, False, CurrentTime);
        XFlush(display);
    }
}

void Input::keyDown(int vkCode) {
    // TODO: 实现按键按下
}

void Input::keyUp(int vkCode) {
    // TODO: 实现按键释放
}

void Input::key(int vkCode) {
    // TODO: 实现按键
}

void Input::type(const std::string& text) {
    // TODO: 实现文本输入
}

void Input::type(const std::string& text, int delayMs) {
    // TODO: 实现文本输入
}

bool Input::isKeyDown(int vkCode) {
    Display* display = getInputDisplay();
    if (!display) {
        return false;
    }

    char keys[32];
    XQueryKeymap(display, keys);
    KeyCode keyCode = XKeysymToKeycode(display, vkCode);
    if (keyCode == NoSymbol) {
        return false;
    }

    return (keys[keyCode / 8] & (1 << (keyCode % 8))) != 0;
}

bool Input::isMouseDown(MouseButton button) {
    Display* display = getInputDisplay();
    if (!display) {
        return false;
    }

    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int mask;
    XQueryPointer(display, DefaultRootWindow(display),
                 &root, &child, &rootX, &rootY, &winX, &winY, &mask);

    switch (button) {
        case MouseButton::Left:
            return (mask & Button1Mask) != 0;
        case MouseButton::Right:
            return (mask & Button3Mask) != 0;
        case MouseButton::Middle:
            return (mask & Button2Mask) != 0;
    }
    return false;
}

#endif

} // namespace wingman
