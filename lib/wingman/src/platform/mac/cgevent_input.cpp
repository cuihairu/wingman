#include "wingman/platform/iinput.hpp"
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>
#include <thread>
#include <chrono>
#include <unordered_map>

namespace wingman::platform::mac {

/**
 * @brief macOS CGEvent input implementation
 *
 * Uses Core Graphics Event API for input simulation.
 */
class CGEventInput : public IInput {
public:
    CGEventInput() = default;
    ~CGEventInput() override {
        shutdown();
    }

    bool initialize(const InputConfig& config) override {
        config_ = config;
        // Get accessibility permissions
        initialized_ = AXIsProcessTrusted() != 0;
        if (!initialized_) {
            spdlog::warn("[CGEventInput] Missing accessibility permissions");
        }
        return initialized_;
    }

    void shutdown() override {
        initialized_ = false;
    }

    void mouseMove(int x, int y) override {
        if (!initialized_) return;

        CGEventRef event = CGEventCreateMouseEvent(
            nullptr,
            kCGEventMouseMoved,
            CGPointMake(x, y),
            kCGMouseButtonLeft
        );
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void mouseMoveRelative(int deltaX, int deltaY) override {
        if (!initialized_) return;

        CGEventRef event = CGEventCreate(nullptr);
        CGEventSetType(event, kCGEventMouseMoved);
        CGEventSetIntegerValueField(event, kCGMouseEventDeltaX, deltaX);
        CGEventSetIntegerValueField(event, kCGMouseEventDeltaY, deltaY);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void mouseDown(MouseButton button) override {
        if (!initialized_) return;

        CGEventType type = getMouseButtonType(button, true);
        CGMouseButton cgButton = getCGMouseButton(button);

        CGPoint pos = getCurrentMousePosition();

        CGEventRef event = CGEventCreateMouseEvent(
            nullptr,
            type,
            pos,
            cgButton
        );
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void mouseUp(MouseButton button) override {
        if (!initialized_) return;

        CGEventType type = getMouseButtonType(button, false);
        CGMouseButton cgButton = getCGMouseButton(button);

        CGPoint pos = getCurrentMousePosition();

        CGEventRef event = CGEventCreateMouseEvent(
            nullptr,
            type,
            pos,
            cgButton
        );
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
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

        CGEventRef event = CGEventCreateScrollWheelEvent(
            nullptr,
            kCGScrollEventUnitLine,
            1,
            delta
        );
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void mouseWheelHorizontal(int delta) override {
        if (!initialized_) return;

        CGEventRef event = CGEventCreateScrollWheelEvent(
            nullptr,
            kCGScrollEventUnitLine,
            2,
            0,
            delta
        );
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void keyDown(KeyCode key) override {
        if (!initialized_) return;

        CGKeyCode cgKeyCode = toCGKeyCode(key);
        CGEventRef event = CGEventCreateKeyboardEvent(nullptr, cgKeyCode, true);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void keyUp(KeyCode key) override {
        if (!initialized_) return;

        CGKeyCode cgKeyCode = toCGKeyCode(key);
        CGEventRef event = CGEventCreateKeyboardEvent(nullptr, cgKeyCode, false);
        CGEventPost(kCGHIDEventTap, event);
        CFRelease(event);
        sleep();
    }

    void keyPress(KeyCode key) override {
        if (!initialized_) return;

        keyDown(key);
        sleep();
        keyUp(key);
    }

    void keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) override {
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
            CGEventRef event = CGEventCreateKeyboardEvent(nullptr, 0, true);
            CGEventKeyboardSetUnicodeString(event, 1, &c);
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
            sleep();
        }
    }

    Point getMousePosition() override {
        CGPoint pos = getCurrentMousePosition();
        return Point{static_cast<int>(pos.x), static_cast<int>(pos.y)};
    }

    bool isKeyPressed(KeyCode key) override {
        CGKeyCode cgKeyCode = toCGKeyCode(key);
        return CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, cgKeyCode) != 0;
    }

    bool isMouseButtonPressed(MouseButton button) override {
        CGPoint pos = getCurrentMousePosition();
        CGEventRef event = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, pos, kCGMouseButtonLeft);

        bool pressed = false;
        if (button == MouseButton::Left) {
            pressed = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft);
        } else if (button == MouseButton::Right) {
            pressed = CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonRight);
        }

        CFRelease(event);
        return pressed;
    }

    void mouseDragBegin(MouseButton button) override {
        mouseDown(button);
    }

    void mouseDragEnd(MouseButton button) override {
        mouseUp(button);
    }

    void setInputDelay(int delayMicroseconds) override {
        config_.inputDelay = delayMicroseconds;
    }

    std::string getBackendName() const override {
        return "CGEvent";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "CGEvent",
            "1.0",
            initialized_,
            "macOS Core Graphics Event API"
        };
    }

    InputConfig getConfig() const override {
        return config_;
    }

    bool supportsTextInput() const override {
        return true;
    }

    bool supportsRelativeMovement() const override {
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

    static CGPoint getCurrentMousePosition() {
        CGEventRef event = CGEventCreate(nullptr);
        CGPoint pos = CGEventGetLocation(event);
        CFRelease(event);
        return pos;
    }

    static CGEventType getMouseButtonType(MouseButton button, bool isDown) {
        switch (button) {
            case MouseButton::Left:
                return isDown ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
            case MouseButton::Right:
                return isDown ? kCGEventRightMouseDown : kCGEventRightMouseUp;
            case MouseButton::Middle:
                return isDown ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
            default:
                return kCGEventMouseMoved;
        }
    }

    static CGMouseButton getCGMouseButton(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return kCGMouseButtonLeft;
            case MouseButton::Right: return kCGMouseButtonRight;
            case MouseButton::Middle: return kCGMouseButtonCenter;
            default: return kCGMouseButtonLeft;
        }
    }

    static CGKeyCode toCGKeyCode(KeyCode key) {
        // macOS virtual keycode mapping
        static const std::unordered_map<KeyCode, CGKeyCode> keyMap = {
            {KeyCode::A, kVK_ANSI_A}, {KeyCode::B, kVK_ANSI_B}, {KeyCode::C, kVK_ANSI_C},
            {KeyCode::D, kVK_ANSI_D}, {KeyCode::E, kVK_ANSI_E}, {KeyCode::F, kVK_ANSI_F},
            {KeyCode::G, kVK_ANSI_G}, {KeyCode::H, kVK_ANSI_H}, {KeyCode::I, kVK_ANSI_I},
            {KeyCode::J, kVK_ANSI_J}, {KeyCode::K, kVK_ANSI_K}, {KeyCode::L, kVK_ANSI_L},
            {KeyCode::M, kVK_ANSI_M}, {KeyCode::N, kVK_ANSI_N}, {KeyCode::O, kVK_ANSI_O},
            {KeyCode::P, kVK_ANSI_P}, {KeyCode::Q, kVK_ANSI_Q}, {KeyCode::R, kVK_ANSI_R},
            {KeyCode::S, kVK_ANSI_S}, {KeyCode::T, kVK_ANSI_T}, {KeyCode::U, kVK_ANSI_U},
            {KeyCode::V, kVK_ANSI_V}, {KeyCode::W, kVK_ANSI_W}, {KeyCode::X, kVK_ANSI_X},
            {KeyCode::Y, kVK_ANSI_Y}, {KeyCode::Z, kVK_ANSI_Z},
            {KeyCode::D0, kVK_ANSI_0}, {KeyCode::D1, kVK_ANSI_1}, {KeyCode::D2, kVK_ANSI_2},
            {KeyCode::D3, kVK_ANSI_3}, {KeyCode::D4, kVK_ANSI_4}, {KeyCode::D5, kVK_ANSI_5},
            {KeyCode::D6, kVK_ANSI_6}, {KeyCode::D7, kVK_ANSI_7}, {KeyCode::D8, kVK_ANSI_8},
            {KeyCode::D9, kVK_ANSI_9},
            {KeyCode::F1, kVK_F1}, {KeyCode::F2, kVK_F2}, {KeyCode::F3, kVK_F3},
            {KeyCode::F4, kVK_F4}, {KeyCode::F5, kVK_F5}, {KeyCode::F6, kVK_F6},
            {KeyCode::F7, kVK_F7}, {KeyCode::F8, kVK_F8}, {KeyCode::F9, kVK_F9},
            {KeyCode::F10, kVK_F10}, {KeyCode::F11, kVK_F11}, {KeyCode::F12, kVK_F12},
            {KeyCode::Return, kVK_Return}, {KeyCode::Tab, kVK_Tab}, {KeyCode::Space, kVK_Space},
            {KeyCode::Escape, kVK_Escape}, {KeyCode::Backspace, kVK_Delete},
            {KeyCode::Shift, kVK_Shift}, {KeyCode::Control, kVK_Control},
            {KeyCode::Alt, kVK_Option}, {KeyCode::Meta, kVK_Command},
            {KeyCode::Left, kVK_LeftArrow}, {KeyCode::Right, kVK_RightArrow},
            {KeyCode::Up, kVK_UpArrow}, {KeyCode::Down, kVK_DownArrow},
        };

        auto it = keyMap.find(key);
        return it != keyMap.end() ? it->second : kVK_Space;
    }
};

} // namespace wingman::platform::mac
#endif // __APPLE__
