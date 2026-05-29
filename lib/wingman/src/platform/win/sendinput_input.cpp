#include "wingman/platform/iinput.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <Windows.h>
#include <thread>
#include <chrono>

namespace wingman::platform::win {

/**
 * @brief Windows SendInput implementation
 *
 * Uses Windows SendInput API for input simulation.
 */
class SendInputInput : public IInput {
public:
    SendInputInput() = default;
    ~SendInputInput() override {
        shutdown();
    }

    bool initialize(const InputConfig& config) override {
        config_ = config;
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    void mouseMove(int x, int y) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(x * 65535.0 / GetSystemMetrics(SM_CXSCREEN));
        input.mi.dy = static_cast<LONG>(y * 65535.0 / GetSystemMetrics(SM_CYSCREEN));
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

        sendInput(1, &input);
    }

    void mouseMoveRelative(int deltaX, int deltaY) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = deltaX;
        input.mi.dy = deltaY;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;

        sendInput(1, &input);
    }

    void mouseDown(MouseButton button) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = getMouseDownFlag(button);

        sendInput(1, &input);
    }

    void mouseUp(MouseButton button) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = getMouseUpFlag(button);

        sendInput(1, &input);
    }

    void mouseClick(MouseButton button) override {
        if (!initialized_) return;

        mouseDown(button);
        sleep();
        mouseUp(button);
    }

    void mouseDoubleClick(MouseButton button) override {
        if (!initialized_) return;

        mouseClick(button);
        sleep();
        mouseClick(button);
    }

    void mouseWheel(int delta) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(delta * WHEEL_DELTA);

        sendInput(1, &input);
    }

    void mouseWheelHorizontal(int delta) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        input.mi.mouseData = static_cast<DWORD>(delta * WHEEL_DELTA);

        sendInput(1, &input);
    }

    void keyDown(KeyCode key) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(key);
        input.ki.dwFlags = 0;

        sendInput(1, &input);
    }

    void keyUp(KeyCode key) override {
        if (!initialized_) return;

        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = static_cast<WORD>(key);
        input.ki.dwFlags = KEYEVENTF_KEYUP;

        sendInput(1, &input);
    }

    void keyPress(KeyCode key) override {
        if (!initialized_) return;

        keyDown(key);
        sleep();
        keyUp(key);
    }

    void keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) {
        if (!initialized_) return;

        for (KeyCode mod : modifiers) {
            keyDown(mod);
        }
        sleep();
        keyPress(key);
        sleep();
        for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
            keyUp(*it);
        }
    }

    void textInput(const std::string& text) override {
        if (!initialized_) return;

        for (char c : text) {
            SHORT vk = VkKeyScanA(c);
            if (vk == -1) {
                // Characters that cannot be directly typed, skip
                continue;
            }

            WORD scanCode = MapVirtualKeyA(LOBYTE(vk), 0);

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = LOBYTE(vk);
            inputs[0].ki.wScan = scanCode;
            inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[0].ki.time = 0;
            inputs[0].ki.dwExtraInfo = 0;

            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = LOBYTE(vk);
            inputs[1].ki.wScan = scanCode;
            inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            inputs[1].ki.time = 0;
            inputs[1].ki.dwExtraInfo = 0;

            sendInput(2, inputs);
            sleep();
        }
    }

    Point getMousePosition() override {
        POINT pt;
        GetCursorPos(&pt);
        return Point{pt.x, pt.y};
    }

    bool isKeyPressed(KeyCode key) override {
        return (GetKeyState(static_cast<int>(key)) & 0x8000) != 0;
    }

    bool isMouseButtonPressed(MouseButton button) {
        return (GetKeyState(getVirtualKeyCode(button)) & 0x8000) != 0;
    }

    void mouseDragBegin(MouseButton button) {
        mouseDown(button);
    }

    void mouseDragEnd(MouseButton button) {
        mouseUp(button);
    }

    void setInputDelay(int delayMicroseconds) {
        config_.inputDelay = delayMicroseconds;
    }

    std::string getBackendName() const override {
        return "SendInput";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "SendInput",
            "1.0",
            initialized_,
            "Windows SendInput API"
        };
    }

    InputConfig getConfig() const {
        return config_;
    }

    bool supportsTextInput() const {
        return true;
    }

    bool supportsRelativeMovement() const {
        return true;
    }

private:
    InputConfig config_;
    bool initialized_ = false;

    void sleep() {
        if (config_.inputDelay > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(config_.inputDelay));
        }
    }

    void sendInput(UINT count, INPUT* inputs) {
        sleep();
        SendInput(count, inputs, sizeof(INPUT));
    }

    static DWORD getMouseDownFlag(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return MOUSEEVENTF_LEFTDOWN;
            case MouseButton::Right: return MOUSEEVENTF_RIGHTDOWN;
            case MouseButton::Middle: return MOUSEEVENTF_MIDDLEDOWN;
            case MouseButton::X1: return MOUSEEVENTF_XDOWN;
            case MouseButton::X2: return MOUSEEVENTF_XDOWN;
            default: return 0;
        }
    }

    static DWORD getMouseUpFlag(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return MOUSEEVENTF_LEFTUP;
            case MouseButton::Right: return MOUSEEVENTF_RIGHTUP;
            case MouseButton::Middle: return MOUSEEVENTF_MIDDLEUP;
            case MouseButton::X1: return MOUSEEVENTF_XUP;
            case MouseButton::X2: return MOUSEEVENTF_XUP;
            default: return 0;
        }
    }

    static int getVirtualKeyCode(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return VK_LBUTTON;
            case MouseButton::Right: return VK_RBUTTON;
            case MouseButton::Middle: return VK_MBUTTON;
            case MouseButton::X1: return VK_XBUTTON1;
            case MouseButton::X2: return VK_XBUTTON2;
            default: return 0;
        }
    }
};

} // namespace wingman::platform::win
#endif // _WIN32
