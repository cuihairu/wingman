#pragma once
// XTestInput（注入 + 查询回读）/ 顶层 Clipboard 装配（X11/xclip 后端接线）/
// 顶层 Window 装配（X11 窗口管理接线）/ 真实 WM 集成（自起 Xvfb + openbox）。
// 仅 Linux 编译；运行时无 X display（headless CI 等）时各用例 GTEST_SKIP。
// 本地验证：Xvfb -screen 0 1280x800x24 :99 & 然后 DISPLAY=:99 ctest -R X11Platform。
#if defined(__linux__)

#include <gtest/gtest.h>

#include "wingman/platform/screen_factory.hpp"
#include "wingman/platform/input_factory.hpp"
#include "wingman/platform/iscreen.hpp"
#include "wingman/platform/icapture.hpp"
#include "wingman/platform/iinput.hpp"
#include "wingman/platform/iclipboard.hpp"
#include "wingman/platform/iwindow.hpp"
#include "wingman/clipboard.hpp"
#include "wingman/filewatcher.hpp"
#include "wingman/window.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "clipboard_lock_guard.hpp"
#include "clipboard_poll.hpp"
#include "x11_test_lock.hpp"
#include "platform/linux/x11_display.hpp"  // openX11Display：瞬态 accept 拒绝重试
#include "wingman/screen.hpp"  // Bitmap 完整定义（icapture.hpp 仅前向声明）

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <sys/file.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

// 平台工厂由 x11_factory.cpp（include-unity 聚合 TU）导出，无公开头文件；
// 与 input_factory.cpp / clipboard.cpp 的前向声明模式一致。
// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间限定需要 undo（同 x11_factory.cpp）
#undef linux
namespace wingman::platform::linux {
std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config);
std::unique_ptr<IWindow> createX11Window();
// inotify 后端工厂（filewatcher.cpp 内部声明、x11_factory.cpp 定义），
// 用独立实例验证 shutdown 清理——FileWatcher 单例 shutdown 会污染后续用例
std::unique_ptr<IFileWatcher> createInotifyFileWatcher();
}

namespace {

using wingman::platform::linux::openX11Display;  // 断开→重连瞬态拒绝重试（x11_display.hpp）

// XOpenDisplay 探测（结果缓存）：DISPLAY 未设置或连不上时各用例跳过而非失败。
// 探测本身也必须抗瞬态拒绝：这里的假阴性会把整个 fixture 误判为无 X 而全量
// skip，把真实回归吞成绿色。
bool x11Available() {
    static const bool cached = [] {
        Display* display = openX11Display(nullptr);
        if (!display) {
            return false;
        }
        XCloseDisplay(display);
        return true;
    }();
    return cached;
}

// X11Clipboard 的文本链路是 fork + xclip 外部进程实现，xclip 属运行期可选依赖。
bool xclipAvailable() {
    static const bool cached = std::system("command -v xclip >/dev/null 2>&1") == 0;
    return cached;
}

// 等待窗口随 CPU 超售自适应（同 dashboard 二轮根治口径）：共享机 load 达到核数
// 数倍时，固定秒数的防挂死护栏会被调度饥饿本身击穿——护栏报警的其实是负载，
// 不是被测行为。factor = clamp(load1/nproc, 1, 8)。
double x11OversubscriptionFactor() {
    double load = 0.0;
    if (::getloadavg(&load, 1) != 1) {
        return 1.0;
    }
    const long nproc = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) {
        return 1.0;
    }
    const double factor = load / static_cast<double>(nproc);
    if (factor < 1.0) return 1.0;
    return factor > 8.0 ? 8.0 : factor;
}

// 窗口用例的 RAII 测试窗口。Xvfb 无窗口管理器，EWMH 属性（_NET_CLIENT_LIST /
// _NET_ACTIVE_WINDOW）本应由 WM 维护——测试进程直接写根窗口属性模拟 WM 行为
// （与 WM 同为属性写者，合法）。析构销毁窗口并删除根属性，避免污染并行/后续
// 用例（属性随 X server 存活，gtest PRE_TEST 下每用例独立进程）。
class TestX11Window {
public:
    TestX11Window(int x, int y, int width, int height, const char* title) {
        display_ = openX11Display(nullptr);
        if (!display_) return;
        Window root = DefaultRootWindow(display_);
        window_ = XCreateSimpleWindow(display_, root, x, y, width, height,
                                      0, 0, 0);  // 无边框：XMoveWindow 定位边框外缘，getBounds 回读内容区原点，带边框会偏 1px

        XChangeProperty(display_, window_,
                        XInternAtom(display_, "_NET_WM_NAME", False),
                        XInternAtom(display_, "UTF8_STRING", False), 8,
                        PropModeReplace,
                        reinterpret_cast<const unsigned char*>(title),
                        static_cast<int>(std::strlen(title)));

        XClassHint hint;
        hint.res_name = const_cast<char*>("wingman-test");
        hint.res_class = const_cast<char*>("WingmanTest");
        XSetClassHint(display_, window_, &hint);

        const long pid = static_cast<long>(::getpid());
        XChangeProperty(display_, window_,
                        XInternAtom(display_, "_NET_WM_PID", False),
                        XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&pid), 1);

        // 模拟 WM 维护根窗口的 client 列表（enumerate() 的数据源）
        XChangeProperty(display_, root,
                        XInternAtom(display_, "_NET_CLIENT_LIST", False),
                        XA_WINDOW, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&window_), 1);

        XMapWindow(display_, window_);
        // XSync 而非 XFlush：门面用的是另一条连接，跨连接读回（enumerate/
        // getBounds/foreground）必须看到已落地的窗口与根属性；XFlush 只保证
        // 发出、不保证 server 已处理，负载下会读到半初始化状态（2026-09-27
        // flake 登记项之一）
        XSync(display_, False);
    }

    ~TestX11Window() {
        if (!display_) return;
        if (window_ != 0) {
            Window root = DefaultRootWindow(display_);
            XDestroyWindow(display_, window_);
            // 根属性是全局状态（flock 只保证本用例运行期独占），必须清理
            XDeleteProperty(display_, root,
                            XInternAtom(display_, "_NET_CLIENT_LIST", False));
            XDeleteProperty(display_, root,
                            XInternAtom(display_, "_NET_ACTIVE_WINDOW", False));
            XSync(display_, False);  // 根属性清理对后续用例立即可见（同 ctor 理由）
        }
        XCloseDisplay(display_);
    }

    TestX11Window(const TestX11Window&) = delete;
    TestX11Window& operator=(const TestX11Window&) = delete;

    bool valid() const { return display_ != nullptr && window_ != 0; }
    wingman::WindowHandle handle() const { return window_; }

    // 模拟 WM 把本窗口设为前台（写根 _NET_ACTIVE_WINDOW）
    void setActive() {
        Window root = DefaultRootWindow(display_);
        XChangeProperty(display_, root,
                        XInternAtom(display_, "_NET_ACTIVE_WINDOW", False),
                        XA_WINDOW, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&window_), 1);
        XSync(display_, False);  // 门面另一条连接要能立刻读到此属性（同 ctor 理由）
    }

private:
    Display* display_ = nullptr;
    Window window_ = 0;
};

} // namespace

class X11PlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!x11Available()) {
            GTEST_SKIP() << "X display unavailable "
                            "(try: Xvfb -screen 0 1280x800x24 :99 & DISPLAY=:99 ctest)";
        }
    }
};


#endif // __linux__
