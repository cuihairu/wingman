#ifdef __linux__

// Undefine linux macro (some compilers define it as 1)
#undef linux

// Forward declare Linux platform implementation classes
namespace wingman::platform::linux {
    class X11Capture;
    class XTestInput;
    class X11Window;
    class X11Screen;
}

// Include implementations
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
 * @brief Create X11 input simulation instance
 */
std::unique_ptr<IInput> createXTestInput(const InputConfig& config) {
    auto input = std::unique_ptr<IInput>(new XTestInput());
    input->initialize(config);
    return input;
}

/**
 * @brief Create X11 window management instance
 */
std::unique_ptr<IWindow> createX11Window() {
    auto window = std::unique_ptr<IWindow>(new X11Window());
    window->initialize();
    return window;
}

/**
 * @brief Create X11 screen management instance
 */
std::unique_ptr<IScreen> createX11Screen() {
    auto screen = std::unique_ptr<IScreen>(new X11Screen());
    screen->initialize();
    return screen;
}

/**
 * @brief Create X11 screen capture instance
 */
std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config) {
    auto capture = std::unique_ptr<ICapture>(new X11Capture());
    capture->initialize(config);
    return capture;
}

} // namespace wingman::platform::linux

#endif // __linux__
