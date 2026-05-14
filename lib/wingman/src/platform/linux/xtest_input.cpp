#include "wingman/platform/iinput.hpp"
#include <spdlog/spdlog.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <thread>
#include <chrono>
#include <unordered_map>

namespace wingman::platform::linux {

/**
 * @brief Linux XTest 输入模拟实现
 */
class XTestInput : public IInput {
public:
    XTestInput() : display_(nullptr), initialized_(false) {}

    ~XTestInput() override {
        shutdown();
    }

    bool initialize(const InputConfig& config) override {
        if (initialized_) return true;

        config_ = config;
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            spdlog::error("[XTestInput] Failed to open X display");
            return false;
        }

        int eventBase, errorBase;
        if (!XTestQueryExtension(display_, &eventBase, &errorBase, nullptr, nullptr)) {
            spdlog::error("[XTestInput] XTest extension not available");
            XCloseDisplay(display_);
            display_ = nullptr;
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

    void mouseMove(int x, int y) override {
        if (!initialized_) return;

        XTestFakeMotionEvent(display_, -1, x, y, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseMoveRelative(int deltaX, int deltaY) override {
        if (!initialized_) return;

        // 获取当前鼠标位置
        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;
        XQueryPointer(display_, XDefaultRootWindow(display_), &root, &child,
                     &rootX, &rootY, &winX, &winY, &mask);

        XTestFakeMotionEvent(display_, -1, rootX + deltaX, rootY + deltaY, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseDown(MouseButton button) override {
        if (!initialized_) return;

        unsigned int buttonCode = getMouseButtonCode(button);
        XTestFakeButtonEvent(display_, buttonCode, True, CurrentTime);
        XFlush(display_);
        sleep();
    }

    void mouseUp(MouseButton button) override {
        if (!initialized_) return;

        unsigned int buttonCode = getMouseButtonCode(button);
        XTestFakeButtonEvent(display_, buttonCode, False, CurrentTime);
        XFlush(display_);
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

        // XTest 不直接支持滚轮，使用按钮 4 和 5 模拟
        int button = delta > 0 ? 4 : 5;
        int clicks = std::abs(delta);

        for (int i = 0; i < clicks; ++i) {
            XTestFakeButtonEvent(display_, button, True, CurrentTime);
            XTestFakeButtonEvent(display_, button, False, CurrentTime);
        }

        XFlush(display_);
        sleep();
    }

    void mouseWheelHorizontal(int delta) override {
        if (!initialized_) return;

        // 水平滚动使用按钮 6 和 7
        int button = delta > 0 ? 7 : 6;
        int clicks = std::abs(delta);

        for (int i = 0; i < clicks; ++i) {
            XTestFakeButtonEvent(display_, button, True, CurrentTime);
            XTestFakeButtonEvent(display_, button, False, CurrentTime);
        }

        XFlush(display_);
        sleep();
    }

    void keyDown(KeyCode key) override {
        if (!initialized_) return;

        KeySym keySym = toKeySym(key);
        KeyCode keyCode = XKeysymToKeycode(display_, keySym);

        if (keyCode != 0) {
            XTestFakeKeyEvent(display_, keyCode, True, CurrentTime);
            XFlush(display_);
            sleep();
        }
    }

    void keyUp(KeyCode key) override {
        if (!initialized_) return;

        KeySym keySym = toKeySym(key);
        KeyCode keyCode = XKeysymToKeycode(display_, keySym);

        if (keyCode != 0) {
            XTestFakeKeyEvent(display_, keyCode, False, CurrentTime);
            XFlush(display_);
            sleep();
        }
    }

    void keyPress(KeyCode key) override {
        if (!initialized_) return;

        keyDown(key);
        sleep();
        keyUp(key);
    }

    void textInput(const std::string& text) override {
        if (!initialized_) return;

        for (char c : text) {
            KeySym keySym = charToKeySym(c);
            KeyCode keyCode = XKeysymToKeycode(display_, keySym);

            if (keyCode != 0) {
                XTestFakeKeyEvent(display_, keyCode, True, CurrentTime);
                XTestFakeKeyEvent(display_, keyCode, False, CurrentTime);
                XFlush(display_);
                sleep();
            }
        }
    }

    Point getMousePosition() override {
        if (!initialized_) return Point{};

        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;

        XQueryPointer(display_, XDefaultRootWindow(display_), &root, &child,
                     &rootX, &rootY, &winX, &winY, &mask);

        return Point{rootX, rootY};
    }

    bool isKeyPressed(KeyCode key) override {
        if (!initialized_) return false;

        KeySym keySym = toKeySym(key);
        KeyCode keyCode = XKeysymToKeycode(display_, keySym);

        if (keyCode == 0) return false;

        char keys[32];
        XQueryKeymap(display_, keys);

        return (keys[keyCode / 8] & (1 << (keyCode % 8))) != 0;
    }

    bool isMousePressed(MouseButton button) override {
        if (!initialized_) return false;

        Window root, child;
        int rootX, rootY, winX, winY;
        unsigned int mask;

        XQueryPointer(display_, XDefaultRootWindow(display_), &root, &child,
                     &rootX, &rootY, &winX, &winY, &mask);

        unsigned int buttonMask = getMouseButtonMask(button);
        return (mask & buttonMask) != 0;
    }

    std::string getBackendName() const override {
        return "XTest";
    }

    BackendInfo getBackendInfo() const override {
        return BackendInfo{
            "XTest",
            "1.0",
            initialized_,
            "Linux XTest Extension Input Simulation"
        };
    }

    InputConfig getConfig() const override {
        return config_;
    }

private:
    Display* display_;
    bool initialized_;
    InputConfig config_;

    void sleep() {
        if (config_.inputDelay > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(config_.inputDelay));
        }
    }

    static unsigned int getMouseButtonCode(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return 1;
            case MouseButton::Middle: return 2;
            case MouseButton::Right: return 3;
            case MouseButton::X1: return 8;  // 侧键1
            case MouseButton::X2: return 9;  // 侧键2
            default: return 1;
        }
    }

    static unsigned int getMouseButtonMask(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return Button1Mask;
            case MouseButton::Middle: return Button2Mask;
            case MouseButton::Right: return Button3Mask;
            case MouseButton::X1: return Button4Mask;
            case MouseButton::X2: return Button5Mask;
            default: return 0;
        }
    }

    static KeySym toKeySym(KeyCode key) {
        static const std::unordered_map<KeyCode, KeySym> keyMap = {
            // 字母键
            {KeyCode::A, XK_a}, {KeyCode::B, XK_b}, {KeyCode::C, XK_c},
            {KeyCode::D, XK_d}, {KeyCode::E, XK_e}, {KeyCode::F, XK_f},
            {KeyCode::G, XK_g}, {KeyCode::H, XK_h}, {KeyCode::I, XK_i},
            {KeyCode::J, XK_j}, {KeyCode::K, XK_k}, {KeyCode::L, XK_l},
            {KeyCode::M, XK_m}, {KeyCode::N, XK_n}, {KeyCode::O, XK_o},
            {KeyCode::P, XK_p}, {KeyCode::Q, XK_q}, {KeyCode::R, XK_r},
            {KeyCode::S, XK_s}, {KeyCode::T, XK_t}, {KeyCode::U, XK_u},
            {KeyCode::V, XK_v}, {KeyCode::W, XK_w}, {KeyCode::X, XK_x},
            {KeyCode::Y, XK_y}, {KeyCode::Z, XK_z},

            // 数字键
            {KeyCode::Num0, XK_0}, {KeyCode::Num1, XK_1}, {KeyCode::Num2, XK_2},
            {KeyCode::Num3, XK_3}, {KeyCode::Num4, XK_4}, {KeyCode::Num5, XK_5},
            {KeyCode::Num6, XK_6}, {KeyCode::Num7, XK_7}, {KeyCode::Num8, XK_8},
            {KeyCode::Num9, XK_9},

            // 功能键
            {KeyCode::F1, XK_F1}, {KeyCode::F2, XK_F2}, {KeyCode::F3, XK_F3},
            {KeyCode::F4, XK_F4}, {KeyCode::F5, XK_F5}, {KeyCode::F6, XK_F6},
            {KeyCode::F7, XK_F7}, {KeyCode::F8, XK_F8}, {KeyCode::F9, XK_F9},
            {KeyCode::F10, XK_F10}, {KeyCode::F11, XK_F11}, {KeyCode::F12, XK_F12},

            // 控制键
            {KeyCode::Space, XK_space},
            {KeyCode::Enter, XK_Return},
            {KeyCode::Escape, XK_Escape},
            {KeyCode::Tab, XK_Tab},
            {KeyCode::Backspace, XK_BackSpace},
            {KeyCode::Delete, XK_Delete},
            {KeyCode::Insert, XK_Insert},
            {KeyCode::Home, XK_Home},
            {KeyCode::End, XK_End},
            {KeyCode::PageUp, XK_Prior},
            {KeyCode::PageDown, XK_Next},

            // 方向键
            {KeyCode::Left, XK_Left},
            {KeyCode::Right, XK_Right},
            {KeyCode::Up, XK_Up},
            {KeyCode::Down, XK_Down},

            // 修饰键
            {KeyCode::Shift, XK_Shift_L},
            {KeyCode::Control, XK_Control_L},
            {KeyCode::Alt, XK_Alt_L},

            // 其他
            {KeyCode::CapsLock, XK_Caps_Lock},
            {KeyCode::PrintScreen, XK_Print},
            {KeyCode::Pause, XK_Pause},
        };

        auto it = keyMap.find(key);
        return it != keyMap.end() ? it->second : XK_VoidSymbol;
    }

    static KeySym charToKeySym(char c) {
        // 简单的 ASCII 到 KeySym 映射
        if (c >= 0 && c <= 127) {
            return static_cast<KeySym>(c);
        }

        // 处理一些特殊字符
        switch (c) {
            case ' ': return XK_space;
            case '\n': return XK_Return;
            case '\t': return XK_Tab;
            default:
                // 对于可打印字符，尝试直接转换
                if (c >= 32 && c <= 126) {
                    return static_cast<KeySym>(c);
                }
                return XK_VoidSymbol;
        }
    }
};

} // namespace wingman::platform::linux

#endif // __linux__
