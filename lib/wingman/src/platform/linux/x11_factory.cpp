#ifdef __linux__

// 前向声明 Linux 平台实现类
namespace wingman::platform::linux {
    class X11Capture;
    class XTestInput;
    class X11Window;
    class X11Screen;
}

// 包含实现
#include "x11_capture.cpp"
#include "xtest_input.cpp"
#include "x11_window.cpp"
#include "x11_screen.cpp"

#include "wingman/platform/iinput.hpp"
#include "wingman/platform/iwindow.hpp"
#include "wingman/platform/iscreen.hpp"
#include "wingman/platform/icapture.hpp"
#include <memory>

namespace wingman::platform::linux {

/**
 * @brief 创建 X11 输入模拟实例
 */
std::unique_ptr<IInput> createXTestInput(const InputConfig& config) {
    auto input = std::unique_ptr<IInput>(new XTestInput());
    input->initialize(config);
    return input;
}

/**
 * @brief 创建 X11 窗口管理实例
 */
std::unique_ptr<IWindow> createX11Window() {
    auto window = std::unique_ptr<IWindow>(new X11Window());
    window->initialize();
    return window;
}

/**
 * @brief 创建 X11 屏幕管理实例
 */
std::unique_ptr<IScreen> createX11Screen() {
    auto screen = std::unique_ptr<IScreen>(new X11Screen());
    screen->initialize();
    return screen;
}

/**
 * @brief 创建 X11 屏幕捕获实例
 */
std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config) {
    auto capture = std::unique_ptr<ICapture>(new X11Capture());
    capture->initialize(config);
    return capture;
}

} // namespace wingman::platform::linux

#endif // __linux__
