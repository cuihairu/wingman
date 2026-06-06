#ifdef __linux__

// Undefine linux macro (some compilers define it as 1)
#undef linux

// Forward declare Linux platform implementation classes
namespace wingman::platform::linux {
    class X11Capture;
    class XTestInput;
    class X11Window;
    class X11Screen;
    class X11Clipboard;
    class InotifyFileWatcher;
}

// Include implementations
#include "x11_capture.cpp"
#include "xtest_input.cpp"
#include "x11_window.cpp"
#include "x11_screen.cpp"
#include "x11_clipboard.cpp"
#include "inotify_filewatcher.cpp"

#include "wingman/platform/iinput.hpp"
#include "wingman/platform/iwindow.hpp"
#include "wingman/platform/iscreen.hpp"
#include "wingman/platform/icapture.hpp"
#include "wingman/platform/iclipboard.hpp"
#include "wingman/platform/ifilewatcher.hpp"
#include <memory>

namespace wingman::platform::linux {

std::unique_ptr<IInput> createXTestInput(const InputConfig& config) {
    auto input = std::unique_ptr<IInput>(new XTestInput());
    input->initialize(config);
    return input;
}

std::unique_ptr<IWindow> createX11Window() {
    auto window = std::unique_ptr<IWindow>(new X11Window());
    window->initialize();
    return window;
}

std::unique_ptr<IScreen> createX11Screen() {
    auto screen = std::unique_ptr<IScreen>(new X11Screen());
    screen->initialize();
    return screen;
}

std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config) {
    auto capture = std::unique_ptr<ICapture>(new X11Capture());
    capture->initialize(config);
    return capture;
}

std::unique_ptr<IClipboard> createX11Clipboard() {
    auto clipboard = std::unique_ptr<IClipboard>(new X11Clipboard());
    clipboard->initialize();
    return clipboard;
}

std::unique_ptr<IFileWatcher> createInotifyFileWatcher() {
    auto watcher = std::unique_ptr<IFileWatcher>(new InotifyFileWatcher());
    watcher->initialize();
    return watcher;
}

} // namespace wingman::platform::linux

#endif // __linux__
