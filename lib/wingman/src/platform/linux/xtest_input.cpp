#ifdef __linux__

#include "wingman/platform/iinput.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

namespace wingman::platform::linux {

class XTestInput : public IInput {
public:
    XTestInput() = default;
    ~XTestInput() override { shutdown(); }

    bool initialize(const InputConfig& config) override {
        config_ = config;
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("XTestInput: failed to open X display");
            return false;
        }
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        if (display_) {
            XCloseDisplay(display_);
            display_ = nullptr;
        }
        initialized_ = false;
    }

    // ========== Mouse ==========
    void mouseMove(int x, int y) override {
        if (!initialized_) return;
        XTestFakeMotionEvent(display_, -1, x, y, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseMoveRelative(int deltaX, int deltaY) override {
        if (!initialized_) return;
        XTestFakeRelativeMotionEvent(display_, deltaX, deltaY, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseDown(MouseButton button) override {
        if (!initialized_) return;
        XTestFakeButtonEvent(display_, getButtonCode(button), True, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseUp(MouseButton button) override {
        if (!initialized_) return;
        XTestFakeButtonEvent(display_, getButtonCode(button), False, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseClick(MouseButton button) override {
        mouseDown(button);
        mouseUp(button);
    }

    void mouseDoubleClick(MouseButton button) override {
        mouseClick(button);
        sleep();
        mouseClick(button);
    }

    void mouseWheel(int delta) override {
        if (!initialized_) return;
        // X11 buttons 4 = scroll up, 5 = scroll down
        unsigned int btn = delta > 0 ? 4 : 5;
        int count = abs(delta);
        for (int i = 0; i < count; i++) {
            XTestFakeButtonEvent(display_, btn, True, CurrentTime);
            XTestFakeButtonEvent(display_, btn, False, CurrentTime);
        }
        XFlush(display_);
        sleep();
    }

    void mouseWheelHorizontal(int delta) override {
        if (!initialized_) return;
        // X11 buttons 6 = scroll right, 7 = scroll left
        unsigned int btn = delta > 0 ? 6 : 7;
        int count = abs(delta);
        for (int i = 0; i < count; i++) {
            XTestFakeButtonEvent(display_, btn, True, CurrentTime);
            XTestFakeButtonEvent(display_, btn, False, CurrentTime);
        }
        XFlush(display_);
        sleep();
    }

    // ========== Keyboard ==========
    void keyDown(KeyCode key) override {
        if (!initialized_) return;
        KeySym sym = toKeySym(key);
        const ::KeyCode code = XKeysymToKeycode(display_, sym);
        XTestFakeKeyEvent(display_, code, True, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void keyUp(KeyCode key) override {
        if (!initialized_) return;
        KeySym sym = toKeySym(key);
        const ::KeyCode code = XKeysymToKeycode(display_, sym);
        XTestFakeKeyEvent(display_, code, False, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void keyPress(KeyCode key) override {
        keyDown(key);
        keyUp(key);
    }

    void keyCombination(const std::vector<KeyCode>& modifiers, KeyCode key) override {
        for (KeyCode mod : modifiers) keyDown(mod);
        keyPress(key);
        for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) keyUp(*it);
    }

    void textInput(const std::string& text) override {
        if (!initialized_) return;
        // Use XTest to simulate typing via keysym lookup
        for (char c : text) {
            KeySym sym = XStringToKeysym(std::string(1, c).c_str());
            if (sym == NoSymbol) continue;
            const ::KeyCode code = XKeysymToKeycode(display_, sym);
            if (code == 0) continue;
            XTestFakeKeyEvent(display_, code, True, CurrentTime);
            XTestFakeKeyEvent(display_, code, False, CurrentTime);
            XFlush(display_);
            sleep();
        }
    }

    // ========== Query ==========
    Point getMousePosition() override {
        if (!initialized_) return {};
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;
        XQueryPointer(display_, DefaultRootWindow(display_),
                      &root, &child, &rootX, &rootY, &winX, &winY, &mask);
        return {rootX, rootY};
    }

    bool isKeyPressed(KeyCode key) override {
        if (!initialized_) return false;
        char keys_return[32];
        XQueryKeymap(display_, keys_return);
        KeySym sym = toKeySym(key);
        const ::KeyCode code = XKeysymToKeycode(display_, sym);
        return (keys_return[code / 8] & (1 << (code % 8))) != 0;
    }

    bool isMousePressed(MouseButton button) override {
        if (!initialized_) return false;
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;
        XQueryPointer(display_, DefaultRootWindow(display_),
                      &root, &child, &rootX, &rootY, &winX, &winY, &mask);
        unsigned int btnMask = 0;
        switch (button) {
            case MouseButton::Left: btnMask = Button1Mask; break;
            case MouseButton::Middle: btnMask = Button2Mask; break;
            case MouseButton::Right: btnMask = Button3Mask; break;
            default: break;
        }
        return (mask & btnMask) != 0;
    }

    void mouseDragBegin(MouseButton button) override { mouseDown(button); }
    void mouseDragEnd(MouseButton button) override { mouseUp(button); }

    void setInputDelay(int delayMicroseconds) override {
        config_.inputDelay = delayMicroseconds;
    }

    InputConfig getConfig() const override { return config_; }
    bool supportsTextInput() const override { return true; }
    bool supportsRelativeMovement() const override { return true; }
    std::string getBackendName() const override { return "XTest"; }
    BackendInfo getBackendInfo() const override {
        return {"XTest", "1.0", initialized_, "X11 XTest extension"};
    }

private:
    Display* display_ = nullptr;
    InputConfig config_;
    bool initialized_ = false;

    void sleep() {
        if (config_.inputDelay > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(config_.inputDelay));
        }
    }

    static unsigned int getButtonCode(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return 1;
            case MouseButton::Middle: return 2;
            case MouseButton::Right: return 3;
            case MouseButton::X1: return 8;
            case MouseButton::X2: return 9;
            default: return 1;
        }
    }

    static KeySym toKeySym(KeyCode key) {
        switch (key) {
            case KeyCode::A: return XK_a; case KeyCode::B: return XK_b;
            case KeyCode::C: return XK_c; case KeyCode::D: return XK_d;
            case KeyCode::E: return XK_e; case KeyCode::F: return XK_f;
            case KeyCode::G: return XK_g; case KeyCode::H: return XK_h;
            case KeyCode::I: return XK_i; case KeyCode::J: return XK_j;
            case KeyCode::K: return XK_k; case KeyCode::L: return XK_l;
            case KeyCode::M: return XK_m; case KeyCode::N: return XK_n;
            case KeyCode::O: return XK_o; case KeyCode::P: return XK_p;
            case KeyCode::Q: return XK_q; case KeyCode::R: return XK_r;
            case KeyCode::S: return XK_s; case KeyCode::T: return XK_t;
            case KeyCode::U: return XK_u; case KeyCode::V: return XK_v;
            case KeyCode::W: return XK_w; case KeyCode::X: return XK_x;
            case KeyCode::Y: return XK_y; case KeyCode::Z: return XK_z;
            case KeyCode::Num0: return XK_0; case KeyCode::Num1: return XK_1;
            case KeyCode::Num2: return XK_2; case KeyCode::Num3: return XK_3;
            case KeyCode::Num4: return XK_4; case KeyCode::Num5: return XK_5;
            case KeyCode::Num6: return XK_6; case KeyCode::Num7: return XK_7;
            case KeyCode::Num8: return XK_8; case KeyCode::Num9: return XK_9;
            case KeyCode::F1: return XK_F1; case KeyCode::F2: return XK_F2;
            case KeyCode::F3: return XK_F3; case KeyCode::F4: return XK_F4;
            case KeyCode::F5: return XK_F5; case KeyCode::F6: return XK_F6;
            case KeyCode::F7: return XK_F7; case KeyCode::F8: return XK_F8;
            case KeyCode::F9: return XK_F9; case KeyCode::F10: return XK_F10;
            case KeyCode::F11: return XK_F11; case KeyCode::F12: return XK_F12;
            case KeyCode::Space: return XK_space;
            case KeyCode::Enter: return XK_Return;
            case KeyCode::Escape: return XK_Escape;
            case KeyCode::Tab: return XK_Tab;
            case KeyCode::Backspace: return XK_BackSpace;
            case KeyCode::Delete: return XK_Delete;
            case KeyCode::Insert: return XK_Insert;
            case KeyCode::Home: return XK_Home;
            case KeyCode::End: return XK_End;
            case KeyCode::PageUp: return XK_Page_Up;
            case KeyCode::PageDown: return XK_Page_Down;
            case KeyCode::Left: return XK_Left;
            case KeyCode::Up: return XK_Up;
            case KeyCode::Right: return XK_Right;
            case KeyCode::Down: return XK_Down;
            case KeyCode::Shift: return XK_Shift_L;
            case KeyCode::Control: return XK_Control_L;
            case KeyCode::Alt: return XK_Alt_L;
            case KeyCode::CapsLock: return XK_Caps_Lock;
            case KeyCode::PrintScreen: return XK_Print;
            case KeyCode::Pause: return XK_Pause;
            default: return NoSymbol;
        }
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
