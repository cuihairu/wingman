// Linux X11 平台实现集成测试：X11Screen（显示器元数据）/ X11Capture（真捕获）/
// XTestInput（注入 + 查询回读）/ 顶层 Clipboard 装配（X11/xclip 后端接线）/
// 顶层 Window 装配（X11 窗口管理接线）。
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
#include "wingman/window.hpp"
#include "clipboard_lock_guard.hpp"
#include "x11_test_lock.hpp"
#include "wingman/screen.hpp"  // Bitmap 完整定义（icapture.hpp 仅前向声明）

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

// 平台工厂由 x11_factory.cpp（include-unity 聚合 TU）导出，无公开头文件；
// 与 input_factory.cpp / clipboard.cpp 的前向声明模式一致。
// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间限定需要 undo（同 x11_factory.cpp）
#undef linux
namespace wingman::platform::linux {
std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config);
std::unique_ptr<IWindow> createX11Window();
}

namespace {

// XOpenDisplay 探测（结果缓存）：DISPLAY 未设置或连不上时各用例跳过而非失败。
bool x11Available() {
    static const bool cached = [] {
        Display* display = XOpenDisplay(nullptr);
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

// 窗口用例的 RAII 测试窗口。Xvfb 无窗口管理器，EWMH 属性（_NET_CLIENT_LIST /
// _NET_ACTIVE_WINDOW）本应由 WM 维护——测试进程直接写根窗口属性模拟 WM 行为
// （与 WM 同为属性写者，合法）。析构销毁窗口并删除根属性，避免污染并行/后续
// 用例（属性随 X server 存活，gtest PRE_TEST 下每用例独立进程）。
class TestX11Window {
public:
    TestX11Window(int x, int y, int width, int height, const char* title) {
        display_ = XOpenDisplay(nullptr);
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
        XFlush(display_);
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
            XFlush(display_);
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
        XFlush(display_);
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

// ========== 显示器元数据（走 createPlatformScreen 分发链） ==========

TEST_F(X11PlatformTest, ScreenMetadataViaPlatformFactory) {
    auto screen = wingman::platform::createPlatformScreen();
    ASSERT_NE(screen, nullptr);
    EXPECT_EQ(screen->getBackendName(), "X11");
    EXPECT_GE(screen->getMonitorCount(), 1);

    const auto bounds = screen->getMonitorBounds(0);
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
    EXPECT_FALSE(screen->getMonitorName(0).empty());
}

// ========== 截图（X11Capture：XGetImage 真读回 Xvfb 画面） ==========

TEST_F(X11PlatformTest, CaptureScreenMatchesMonitorBounds) {
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);
    ASSERT_TRUE(capture->isAvailable());
    ASSERT_GE(capture->getMonitorCount(), 1);

    const auto bounds = capture->getMonitorBounds(0);
    auto full = capture->captureScreen(0);
    ASSERT_NE(full, nullptr);
    EXPECT_EQ(full->getWidth(), bounds.width);
    EXPECT_EQ(full->getHeight(), bounds.height);
}

TEST_F(X11PlatformTest, CaptureRegionReturnsRequestedSize) {
    auto capture = wingman::platform::linux::createX11Capture({});
    ASSERT_NE(capture, nullptr);
    ASSERT_TRUE(capture->isAvailable());

    const wingman::platform::Rect region{0, 0, 64, 64};
    auto bitmap = capture->captureRegion(region);
    ASSERT_NE(bitmap, nullptr);
    EXPECT_EQ(bitmap->getWidth(), 64);
    EXPECT_EQ(bitmap->getHeight(), 64);
}

// ========== 顶层 Screen 装配（此前 Linux 恒 nullptr 的 stub，断链接线回归守卫） ==========

TEST_F(X11PlatformTest, ScreenCaptureRegionReturnsBitmap) {
    auto bitmap = wingman::Screen::capture(wingman::Rect{0, 0, 64, 32});
    ASSERT_NE(bitmap, nullptr);
    EXPECT_EQ(bitmap->getWidth(), 64);
    EXPECT_EQ(bitmap->getHeight(), 32);
}

TEST_F(X11PlatformTest, ScreenCaptureFullScreenMatchesDimensions) {
    auto full = wingman::Screen::capture();
    ASSERT_NE(full, nullptr);
    EXPECT_EQ(full->getWidth(), wingman::Screen::getScreenWidth());
    EXPECT_EQ(full->getHeight(), wingman::Screen::getScreenHeight());
}

TEST_F(X11PlatformTest, ScreenGetPixelOutOfBoundsIsSafe) {
    // 越界坐标曾会因 XGetImage BadMatch 触发默认 error handler exit 进程；
    // 现以宽容 handler 呈现为 Color() 默认值（Xvfb 根窗口黑 → 越界返回黑）
    const auto pixel = wingman::Screen::getPixel(-5, -5);
    EXPECT_EQ(pixel.r, 0);
    EXPECT_EQ(pixel.g, 0);
    EXPECT_EQ(pixel.b, 0);
}

TEST_F(X11PlatformTest, ScreenFindColorOnRootWindow) {
    // 自洽断言：先抓图算出「扫描序首命中位」，再对 findColor 结果精确对照
    // （不假设根窗口底色是否均匀，Xvfb/桌面均确定）
    const wingman::Rect region{0, 0, 32, 32};
    auto bitmap = wingman::Screen::capture(region);
    ASSERT_NE(bitmap, nullptr);

    const auto base = bitmap->getPixel(2, 3);
    wingman::Point expected{-1, -1};
    for (int y = 0; y < bitmap->getHeight() && expected.x < 0; ++y) {
        for (int x = 0; x < bitmap->getWidth(); ++x) {
            if (bitmap->getPixel(x, y).matches(base, 0)) {
                expected.x = x;
                expected.y = y;
                break;
            }
        }
    }
    ASSERT_GE(expected.x, 0) << "base color not found in captured region?!";

    wingman::Point result;
    EXPECT_TRUE(wingman::Screen::findColor(base, region, 0, result));
    EXPECT_EQ(result.x, expected.x);
    EXPECT_EQ(result.y, expected.y);

    // 容差 10 下补色需与区域内所有像素距离均超 10 才断言不命中（均匀底色成立）
    const auto inverted = wingman::Color(
        static_cast<uint8_t>(~base.r), static_cast<uint8_t>(~base.g),
        static_cast<uint8_t>(~base.b));
    const int dr = static_cast<int>(inverted.r) - static_cast<int>(base.r);
    const int dg = static_cast<int>(inverted.g) - static_cast<int>(base.g);
    const int db = static_cast<int>(inverted.b) - static_cast<int>(base.b);
    if (dr * dr + dg * dg + db * db > 10 * 10) {
        EXPECT_FALSE(wingman::Screen::findColor(inverted, region, 10, result));
    }

    auto many = wingman::Screen::findColors(base, wingman::Rect{0, 0, 8, 4}, 0, 5);
    EXPECT_GE(many.size(), 1u);
    EXPECT_LE(many.size(), 5u);
}

// ========== 输入（XTest 注入 → XQueryPointer/XQueryKeymap 回读） ==========

TEST_F(X11PlatformTest, InputMouseMoveRoundtrip) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->getBackendName(), "XTest");

    input->mouseMove(100, 200);
    const auto position = input->getMousePosition();
    EXPECT_EQ(position.x, 100);
    EXPECT_EQ(position.y, 200);
}

TEST_F(X11PlatformTest, InputKeyDownReflectsInKeymap) {
    auto input = wingman::platform::createDefaultInput();
    ASSERT_NE(input, nullptr);

    input->keyDown(wingman::platform::KeyCode::Space);
    EXPECT_TRUE(input->isKeyPressed(wingman::platform::KeyCode::Space));
    input->keyUp(wingman::platform::KeyCode::Space);
    EXPECT_FALSE(input->isKeyPressed(wingman::platform::KeyCode::Space));
}

// ========== 剪贴板（顶层装配断链回归守卫 + xclip 文本链路） ==========

TEST_F(X11PlatformTest, TopLevelClipboardUsesX11Backend) {
    // 装配断链回归守卫：顶层 Clipboard 在 Linux 曾恒为 NullClipboard，
    // X11Clipboard 全库零消费者；有 X 时必须装配为 X11/xclip 后端。
    EXPECT_EQ(wingman::Clipboard::instance().getBackendName(), "X11/xclip");
}

TEST_F(X11PlatformTest, ClipboardTextRoundtrip) {
    // X11 selection 无跨进程锁：与 ClipboardTest.* 并行（ctest -j）时需 flock 串行化
    ClipboardLockGuard clipboardLock;
    auto& clipboard = wingman::Clipboard::instance();
    const std::string payload = "wingman-x11-clipboard-e2e";
    if (!clipboard.setText(payload)) {
        if (!xclipAvailable()) {
            GTEST_SKIP() << "xclip not installed — setText 走优雅失败路径（运行期可选依赖）";
        }
        FAIL() << "setText failed although xclip is available";
    }
    EXPECT_EQ(clipboard.getText(), payload);
    EXPECT_TRUE(clipboard.hasText());
}

// ========== 窗口（顶层 Window 装配断链回归守卫 + X11 窗口管理） ==========

TEST_F(X11PlatformTest, WindowEnumerateFindTitle) {
    // 装配断链回归守卫：顶层 Window 在非 Windows 曾恒空 stub，
    // X11Window 全库零消费者；有 X 时 enumerate/find 必须真出结果。
    X11ServerLockGuard x11Lock;
    TestX11Window win(30, 40, 220, 150, "Wingman Test Window");
    ASSERT_TRUE(win.valid());
    win.setActive();

    auto all = wingman::Window::enumerate();
    ASSERT_FALSE(all.empty());
    const wingman::WindowInfo* info = nullptr;
    for (const auto& wi : all) {
        if (wi.handle == win.handle()) info = &wi;
    }
    ASSERT_NE(info, nullptr) << "test window missing from enumerate()";
    EXPECT_EQ(info->title, "Wingman Test Window");
    EXPECT_EQ(info->bounds.x, 30);
    EXPECT_EQ(info->bounds.y, 40);
    EXPECT_EQ(info->bounds.width, 220);
    EXPECT_EQ(info->bounds.height, 150);
    EXPECT_TRUE(info->isForeground);

    EXPECT_EQ(wingman::Window::find("Test Window"), win.handle());
    EXPECT_EQ(wingman::Window::find("no-such-window-title"), 0u);
    auto matches = wingman::Window::findAll("Wingman");
    EXPECT_EQ(matches.size(), 1u);
    EXPECT_EQ(wingman::Window::getTitle(win.handle()), "Wingman Test Window");
    EXPECT_EQ(wingman::Window::getForeground(), win.handle());
    // 已存在 → waitFor 立即真，不耗超时
    EXPECT_TRUE(wingman::Window::waitFor("Test Window", 500));
}

TEST_F(X11PlatformTest, WindowBoundsMoveResize) {
    X11ServerLockGuard x11Lock;
    TestX11Window win(10, 20, 200, 100, "Wingman Move Window");
    ASSERT_TRUE(win.valid());

    EXPECT_TRUE(wingman::Window::isValid(win.handle()));
    EXPECT_TRUE(wingman::Window::isVisible(win.handle()));

    // XMoveWindow/XResizeWindow 直接生效，无需窗口管理器（Xvfb 可验证）
    EXPECT_TRUE(wingman::Window::move(win.handle(), 77, 88));
    auto b = wingman::Window::getBounds(win.handle());
    EXPECT_EQ(b.x, 77);
    EXPECT_EQ(b.y, 88);
    EXPECT_EQ(b.width, 200);
    EXPECT_EQ(b.height, 100);

    EXPECT_TRUE(wingman::Window::resize(win.handle(), 111, 122));
    b = wingman::Window::getBounds(win.handle());
    EXPECT_EQ(b.x, 77);
    EXPECT_EQ(b.y, 88);
    EXPECT_EQ(b.width, 111);
    EXPECT_EQ(b.height, 122);

    EXPECT_TRUE(wingman::Window::setBounds(win.handle(), {11, 22, 101, 103}));
    b = wingman::Window::getBounds(win.handle());
    EXPECT_EQ(b.x, 11);
    EXPECT_EQ(b.y, 22);
    EXPECT_EQ(b.width, 101);
    EXPECT_EQ(b.height, 103);
}

TEST_F(X11PlatformTest, WindowInvalidHandleIsSafe) {
    // 回归守卫：无效句柄曾触发 BadWindow → Xlib 默认 error handler exit()
    // 杀死整个进程；现在以空值/false 优雅呈现（宽容 handler，同 x11_capture），
    // 且写操作经顶层 isValid 前置不再假成功。
    X11ServerLockGuard x11Lock;
    const wingman::WindowHandle dead = 0xDEADBEEF;

    EXPECT_FALSE(wingman::Window::isValid(dead));
    EXPECT_EQ(wingman::Window::getTitle(dead), "");
    const auto emptyBounds = wingman::Window::getBounds(dead);
    EXPECT_EQ(emptyBounds.x, 0);
    EXPECT_EQ(emptyBounds.y, 0);
    EXPECT_EQ(emptyBounds.width, 0);
    EXPECT_EQ(emptyBounds.height, 0);
    EXPECT_FALSE(wingman::Window::isVisible(dead));
    EXPECT_FALSE(wingman::Window::isForeground(dead));
    EXPECT_FALSE(wingman::Window::activate(dead));
    EXPECT_FALSE(wingman::Window::close(dead));
    EXPECT_FALSE(wingman::Window::minimize(dead));
    EXPECT_FALSE(wingman::Window::maximize(dead));
    EXPECT_FALSE(wingman::Window::restore(dead));
    EXPECT_FALSE(wingman::Window::move(dead, 1, 2));
    EXPECT_FALSE(wingman::Window::resize(dead, 3, 4));
    EXPECT_FALSE(wingman::Window::setBounds(dead, {0, 0, 10, 10}));
    EXPECT_FALSE(wingman::Window::waitFor("never-appears-anywhere", 200));
    EXPECT_TRUE(wingman::Window::waitClose("never-appears-anywhere", 200));
}

TEST_F(X11PlatformTest, X11WindowPlatformFeatures) {
    X11ServerLockGuard x11Lock;
    auto window = wingman::platform::linux::createX11Window();
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->getBackendName(), "X11");

    TestX11Window win(0, 0, 100, 80, "Wingman Platform Window");
    ASSERT_TRUE(win.valid());

    // WM_CLASS（res_name / res_class 任一匹配）
    EXPECT_EQ(window->findByClassName("wingman-test"), win.handle());
    EXPECT_EQ(window->findByClassName("WingmanTest"), win.handle());

    // _NET_WM_PID
    auto byPid = window->findByProcessId(static_cast<uint32_t>(::getpid()));
    EXPECT_NE(std::find(byPid.begin(), byPid.end(), win.handle()), byPid.end());
    const auto pid = window->getProcessId(win.handle());
    ASSERT_TRUE(pid.has_value());
    EXPECT_EQ(*pid, static_cast<uint32_t>(::getpid()));

    // hide/show 翻转：XUnmapWindow/XMapWindow 直接生效，无需窗口管理器
    EXPECT_TRUE(window->hide(win.handle()));
    EXPECT_FALSE(window->isVisible(win.handle()));
    EXPECT_TRUE(window->show(win.handle()));
    EXPECT_TRUE(window->isVisible(win.handle()));

    // activate：XRaiseWindow + XSetInputFocus 真执行（无 WM 时
    // _NET_ACTIVE_WINDOW 无人维护，只断言请求本身成功）
    EXPECT_TRUE(window->activate(win.handle()));
}

#endif // __linux__
