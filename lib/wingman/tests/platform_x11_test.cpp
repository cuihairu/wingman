// Linux X11 平台实现集成测试：X11Screen（显示器元数据）/ X11Capture（真捕获）/
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
#include "wingman/window.hpp"
#include "clipboard_lock_guard.hpp"
#include "x11_test_lock.hpp"
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
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

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

#ifdef WINGMAN_ENABLE_VISION
TEST_F(X11PlatformTest, ScreenFindImageLocatesDrawnPattern) {
    // 模板匹配（vision 构建接线，与 Windows 共用实现）：根窗口画不对称图案
    // → 截取该区域存 PNG 模板 → findImage 全屏找回。图案唯一性保证命中位置
    // 确定（均匀底色下多个完美匹配会让 minMaxLoc 位置不定）；负向断言：
    // 不存在的模板路径优雅 false
    X11ServerLockGuard x11Lock;
    Display* d = XOpenDisplay(nullptr);
    ASSERT_NE(d, nullptr);
    Window root = DefaultRootWindow(d);
    GC gc = XCreateGC(d, root, 0, nullptr);
    // 红块 + 相邻绿块（宽度不对称）——黑底根窗口上的唯一图案
    XSetForeground(d, gc, 0xFF0000);
    XFillRectangle(d, root, gc, 200, 150, 32, 24);
    XSetForeground(d, gc, 0x00FF00);
    XFillRectangle(d, root, gc, 232, 150, 16, 24);
    XFlush(d);
    XFreeGC(d, gc);
    XCloseDisplay(d);

    const wingman::Rect tplRect{200, 150, 48, 24};
    auto tpl = wingman::Screen::capture(tplRect);
    ASSERT_NE(tpl, nullptr);
    // 自洽前置：截图内容与绘制一致（红/绿块内像素命中对应通道）
    EXPECT_EQ(tpl->getPixel(4, 4).r, 255);
    EXPECT_EQ(tpl->getPixel(40, 12).g, 255);

    char path[64];
    std::snprintf(path, sizeof(path), "/tmp/wingman_test_tpl_%d.png",
                  static_cast<int>(::getpid()));
    ASSERT_TRUE(tpl->save(path));

    wingman::Point hit{-1, -1};
    const wingman::Rect full{0, 0, wingman::Screen::getScreenWidth(),
                             wingman::Screen::getScreenHeight()};
    EXPECT_TRUE(wingman::Screen::findImage(path, full, 0.99, hit));
    EXPECT_EQ(hit.x, tplRect.x);
    EXPECT_EQ(hit.y, tplRect.y);

    // 模板路径不存在（imread 失败）→ 优雅 false
    EXPECT_FALSE(wingman::Screen::findImage(
        "/nonexistent/wingman-template.png", wingman::Rect{0, 0, 64, 64}, 0.9, hit));
    std::remove(path);

    // 清理画在根窗口的图案（跨轮次/其他用例自洽性）
    d = XOpenDisplay(nullptr);
    if (d) {
        XClearArea(d, DefaultRootWindow(d), tplRect.x, tplRect.y,
                   tplRect.width, tplRect.height, False);
        XFlush(d);
        XCloseDisplay(d);
    }
}
#endif // WINGMAN_ENABLE_VISION

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

// ========== 真实 WM 集成（自起 Xvfb + openbox 子进程） ==========
//
// 上面的窗口用例在无 WM 的 Xvfb 上由测试进程直写根属性模拟 WM；本节起独立的
// Xvfb + openbox，验证必须由真实 WM 异步兑现的路径：XIconifyWindow 图标化
// （unmap + _NET_WM_STATE_HIDDEN）、_NET_WM_STATE 消息（maximize/restore 状态
// 原子翻转）、_NET_ACTIVE_WINDOW 消息（activate → 前台焦点）、WM 自维护的
// _NET_CLIENT_LIST（enumerate 无需手写属性）。Xvfb / openbox 任一缺失时优雅
// skip（运行期可选依赖，同 xclip 先例）。
// 环境完全自起自毁：专用 display 号（持 flock 串行化，防 ctest -j 并行互抢）+
// 子进程 PDEATHSIG 随测试进程陪葬，不触碰共享 DISPLAY 上的其他 X11 用例。

namespace {

bool xvfbAvailable() {
    static const bool cached = std::system("command -v Xvfb >/dev/null 2>&1") == 0;
    return cached;
}

bool openboxAvailable() {
    static const bool cached = std::system("command -v openbox >/dev/null 2>&1") == 0;
    return cached;
}

// 轮询等待异步条件成立（真实 WM 对图标化/最大化/激活都是异步兑现的）
bool pollUntil(const std::function<bool()>& predicate, int timeoutMs, int intervalMs = 50) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
    return predicate();
}

// 自起 Xvfb + openbox 的 RAII 环境。display 号探测与子进程生命周期全程持有
// 专用 flock（与窗口/剪贴板锁同一模式，锁面不同），ctest -j 并行的 WM 用例
// 进程间串行化。析构按 WM → server 顺序回收（先 TERM 给 Xvfb 清理 socket 的
// 机会，1s 未退再 SIGKILL 兜底）。
//
// 自愈重试：openbox 启动窗口内的外来连接竞态（见 trySetup 注释）高负载下
// 偶发且不可根治，静默沉降 + 低频探测已把概率压到接近零，残余失败由整体
// 重建兜底——搭建链路任一环失败都拆干净、换 display 号重来；归属校验
// （lock 文件 pid 比对）把「display 实际由残留 server 应答」在起 openbox
// 之前就掐断。
class WmEnvironment {
public:
    WmEnvironment() {
        lockFd_ = ::open("/tmp/wingman_test_x11_wm.lock", O_RDONLY | O_CREAT, 0666);
        if (lockFd_ != -1) {
            while (::flock(lockFd_, LOCK_EX) != 0 && errno == EINTR) {}
        }
        for (int attempt = 0; attempt < 2 && !ready_; ++attempt) {
            trySetup(attempt == 0 ? -1 : displayNumber_);
        }
    }
    ~WmEnvironment() {
        if (displaySet_) {
            if (oldDisplay_) {
                ::setenv("DISPLAY", oldDisplay_->c_str(), 1);
            } else {
                ::unsetenv("DISPLAY");
            }
        }
        teardownChildren();
        if (lockFd_ != -1) {
            ::flock(lockFd_, LOCK_UN);
            ::close(lockFd_);
        }
    }

    WmEnvironment(const WmEnvironment&) = delete;
    WmEnvironment& operator=(const WmEnvironment&) = delete;

    bool valid() const { return ready_; }
    const std::string& failReason() const { return failReason_; }

private:
    // 一次完整搭建：选 display → 起 Xvfb → 校验归属 → 起 openbox → canary 验证
    // WM 已真正管理窗口。任一环失败即返回（子进程由构造函数拆干净后重试）。
    // exclude 为上一轮失败的 display 号，重试时避开。
    void trySetup(int exclude) {
        teardownChildren();  // 重试路径先拆上一轮残留（首轮为 no-op）
        displayNumber_ = probeDisplayNumber(exclude);
        if (displayNumber_ < 0) {
            failReason_ = "no free X display number in 20..90";
            return;
        }
        std::snprintf(display_, sizeof(display_), ":%d", displayNumber_);

        char xvfbArg0[] = "Xvfb";
        char screenArg[] = "-screen";
        char screenNum[] = "0";
        char screenSpec[] = "1280x800x24";
        char* xvfbArgv[] = {xvfbArg0, display_, screenArg, screenNum, screenSpec, nullptr};
        xvfbPid_ = spawn(xvfbArgv, nullptr);
        if (!childAlive(xvfbPid_)) {
            failReason_ = "Xvfb failed to start";
            return;
        }
        if (!pollUntil([this] { return displayAccepts(); }, 5000)) {
            failReason_ = "Xvfb did not become ready within 5s";
            return;
        }
        // 归属校验：lock 文件由 X server 自己写 pid。若 :N 实际由残留的旧
        // server 应答（displayAccepts 连上的是它），本子进程的 Xvfb 必然绑定
        // 失败——pid 对不上则换号重来，避免 openbox 连上垂死连接
        if (!displayOwnedBy(xvfbPid_)) {
            failReason_ = "display answered by a stale X server (lock pid mismatch)";
            return;
        }

        char openboxArg0[] = "openbox";
        char* openboxArgv[] = {openboxArg0, nullptr};
        openboxPid_ = spawn(openboxArgv, display_);
        if (!childAlive(openboxPid_)) {
            failReason_ = "openbox failed to start";
            return;
        }
        // openbox 启动窗口（connect + EWMH 注册 + grab 初始化，亚秒级）对
        // server 上的外来连接/断开敏感：高负载下探测连接的断开与其连接建立
        // 竞态，openbox 要么 setup 即死（"Failed to open the display"），要么
        // 连接被 server 摘出事件分发——进程存活、_NET_SUPPORTING_WM_CHECK 已
        // 写，但 MapRequest 永不投递（root 重定向空闲、socket Recv-Q=0），
        // canary 卡 IsUnmapped 直到超时。实测 50ms 间隔连断轮询 ~40% 失败，
        // spawn 后静默 600ms 再低频探测 0/45：先沉降覆盖启动窗口，探测间隔
        // 拉到 500ms 降低残余竞态面；openbox 中途死亡也在此被识别换轮重建
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        if (!pollUntil([this] { return wmRegistered(); }, 5000, 500)) {
            failReason_ = childAlive(openboxPid_)
                ? "openbox did not register (_NET_SUPPORTING_WM_CHECK absent) within 5s"
                : "openbox died during startup";
            return;
        }

        // canary 窗口：openbox 写 _NET_SUPPORTING_WM_CHECK（EWMH 初始化）与真正
        // 进入事件循环之间还有字体/Xft 加载等尾部启动工作——期间 MapRequest
        // 无人处理，首个映射的窗口会悬着。用 canary 把这段预热吸收进环境构造，
        // 测试窗口随后的映射即时被处理，断言轮询保持紧凑。健康环境亚秒完成；
        // 8s 仍未管理说明 WM 已卡死（守死连接），交给上层换号重建
        if (!waitForWmManaging()) {
            failReason_ = "openbox did not start managing windows within 8s";
            return;
        }

        // 测试体与 wingman 后端经 DISPLAY 环境发现本环境；析构时还原
        if (const char* old = ::getenv("DISPLAY")) oldDisplay_ = old;
        ::setenv("DISPLAY", display_, 1);
        displaySet_ = true;
        std::cerr << "[wm-env] using display " << display_
                  << " (xvfb=" << xvfbPid_ << " openbox=" << openboxPid_ << ")\n";
        ready_ = true;
    }

    // 选一个空闲 display 号（socket 与 lock 文件都不存在才算空闲；lock 是 X
    // server 的互斥凭据，早于 socket 存在，只探 socket 会漏掉占号未监听的 server）
    int probeDisplayNumber(int exclude) const {
        for (int candidate = 90; candidate >= 20; --candidate) {
            if (candidate == exclude) continue;
            char socketPath[64], lockPath[32];
            std::snprintf(socketPath, sizeof(socketPath), "/tmp/.X11-unix/X%d", candidate);
            std::snprintf(lockPath, sizeof(lockPath), "/tmp/.X%d-lock", candidate);
            if (::access(socketPath, F_OK) != 0 && ::access(lockPath, F_OK) != 0) {
                return candidate;
            }
        }
        return -1;
    }

    // /tmp/.X<n>-lock 由 X server 启动时写入自身 pid（先于监听 socket）。
    // 读取并比对 pid，确认当前应答 :N 的 server 就是本子进程
    bool displayOwnedBy(pid_t expected) const {
        char lockPath[32];
        std::snprintf(lockPath, sizeof(lockPath), "/tmp/.X%d-lock", displayNumber_);
        FILE* f = ::fopen(lockPath, "r");
        if (!f) return false;
        char buf[32] = {};
        const size_t n = ::fread(buf, 1, sizeof(buf) - 1, f);
        ::fclose(f);
        return n > 0 && std::strtol(buf, nullptr, 10) == static_cast<long>(expected);
    }

    void teardownChildren() {
        terminateChild(openboxPid_);
        openboxPid_ = -1;
        terminateChild(xvfbPid_);
        xvfbPid_ = -1;
    }

    // fork + exec：子进程 PDEATHSIG 随父陪葬（防测试崩溃泄漏 Xvfb/openbox），
    // 输出重定向日志文件（WM 启动失败时可查 /tmp/wingman_wm_child.log）
    pid_t spawn(char* const argv[], const char* childDisplay) {
        pid_t pid = ::fork();
        if (pid != 0) return pid;  // 父进程；-1 由 childAlive 上抛
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (::getppid() == 1) ::_exit(127);  // prctl 前父已亡的窗口期
        if (childDisplay) ::setenv("DISPLAY", childDisplay, 1);
        const int log = ::open("/tmp/wingman_wm_child.log", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (log != -1) {
            ::dup2(log, STDOUT_FILENO);
            ::dup2(log, STDERR_FILENO);
        }
        ::execvp(argv[0], argv);
        ::_exit(127);
    }

    static bool childAlive(pid_t pid) {
        if (pid <= 0) return false;
        return ::waitpid(pid, nullptr, WNOHANG) == 0;
    }

    static void terminateChild(pid_t pid) {
        if (pid <= 0) return;
        ::kill(pid, SIGTERM);
        for (int i = 0; i < 20; ++i) {  // 最多 1s 等 TERM 兑现（Xvfb 清理 socket）
            if (::waitpid(pid, nullptr, WNOHANG) == pid) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        ::kill(pid, SIGKILL);
        ::waitpid(pid, nullptr, 0);
    }

    bool displayAccepts() const {
        Display* d = XOpenDisplay(display_);
        if (!d) return false;
        XCloseDisplay(d);
        return true;
    }

    // openbox 就绪标志：接管 SubstructureRedirect 的 WM 会在根窗口写入
    // _NET_SUPPORTING_WM_CHECK（探测用独立连接，不动测试进程环境）
    bool wmRegistered() const {
        Display* d = XOpenDisplay(display_);
        if (!d) return false;
        Atom actualType;
        int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        const Atom check = XInternAtom(d, "_NET_SUPPORTING_WM_CHECK", False);
        bool registered = false;
        if (XGetWindowProperty(d, DefaultRootWindow(d), check, 0, 1, False,
                               XA_WINDOW, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            registered = (nItems > 0);
            XFree(data);
        }
        XCloseDisplay(d);
        return registered;
    }

    // 映射 canary 窗口并等它进入 WM 维护的 _NET_CLIENT_LIST（专用探测连接，
    // 不污染测试进程环境；canary 用后即毁）。与 WmTestWindow 相同的 ICCCM
    // 合规属性——openbox 会怠慢缺 WM_HINTS/WM_NORMAL_HINTS 的裸窗口
    bool waitForWmManaging() {
        Display* d = XOpenDisplay(display_);
        if (!d) return false;
        const Window canary = XCreateSimpleWindow(d, DefaultRootWindow(d),
                                                  0, 0, 64, 48, 0, 0, 0);
        XWMHints wmHints{};
        wmHints.flags = InputHint | StateHint;
        wmHints.input = True;
        wmHints.initial_state = NormalState;
        XSetWMHints(d, canary, &wmHints);
        XSizeHints sizeHints{};
        sizeHints.flags = PPosition | PSize;
        sizeHints.x = 0;
        sizeHints.y = 0;
        sizeHints.width = 64;
        sizeHints.height = 48;
        XSetNormalHints(d, canary, &sizeHints);
        XMapWindow(d, canary);
        XFlush(d);
        const bool managed = pollUntil([&] { return listContains(d, canary); }, 8000);
        XDestroyWindow(d, canary);
        XFlush(d);
        XCloseDisplay(d);
        return managed;
    }

    static bool listContains(Display* d, Window target) {
        Atom actualType;
        int actualFormat;
        unsigned long nItems, bytesAfter;
        unsigned char* data = nullptr;
        const Atom list = XInternAtom(d, "_NET_CLIENT_LIST", False);
        bool found = false;
        if (XGetWindowProperty(d, DefaultRootWindow(d), list, 0, 64, False,
                               XA_WINDOW, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &data) == Success && data) {
            const auto* windows = reinterpret_cast<const Window*>(data);
            for (unsigned long i = 0; i < nItems; ++i) {
                if (windows[i] == target) {
                    found = true;
                    break;
                }
            }
            XFree(data);
        }
        return found;
    }

    int displayNumber_ = -1;
    char display_[16] = {};
    pid_t xvfbPid_ = -1;
    pid_t openboxPid_ = -1;
    int lockFd_ = -1;
    bool ready_ = false;
    bool displaySet_ = false;
    std::string failReason_;
    std::optional<std::string> oldDisplay_;
};

// 真实 WM 下的测试窗口：只创建/命名/映射，不写任何根属性——WM 自己维护
// _NET_CLIENT_LIST / _NET_ACTIVE_WINDOW，手写反而与 WM 状态打架。
class WmTestWindow {
public:
    WmTestWindow(int x, int y, int width, int height, const char* title) {
        display_ = XOpenDisplay(nullptr);  // WmEnvironment 已设 DISPLAY
        if (!display_) return;
        window_ = XCreateSimpleWindow(display_, DefaultRootWindow(display_),
                                      x, y, width, height, 0, 0, 0);
        XStoreName(display_, window_, title);
        XClassHint hint;
        hint.res_name = const_cast<char*>("wingman-wm-test");
        hint.res_class = const_cast<char*>("WingmanWmTest");
        XSetClassHint(display_, window_, &hint);
        // ICCCM 合规：openbox 会怠慢缺 WM_HINTS（initial state）/WM_NORMAL_HINTS
        // 的裸 Xlib 窗口（实测 15s+ 不管理；xmessage 等合规客户端即时管理）
        XWMHints wmHints{};
        wmHints.flags = InputHint | StateHint;
        wmHints.input = True;
        wmHints.initial_state = NormalState;
        XSetWMHints(display_, window_, &wmHints);
        XSizeHints sizeHints{};
        sizeHints.flags = PPosition | PSize;
        sizeHints.x = x;
        sizeHints.y = y;
        sizeHints.width = width;
        sizeHints.height = height;
        XSetNormalHints(display_, window_, &sizeHints);
        XMapWindow(display_, window_);
        XFlush(display_);
    }

    ~WmTestWindow() {
        if (!display_) return;
        if (window_ != 0) {
            XDestroyWindow(display_, window_);
            XFlush(display_);
        }
        XCloseDisplay(display_);
    }

    WmTestWindow(const WmTestWindow&) = delete;
    WmTestWindow& operator=(const WmTestWindow&) = delete;

    bool valid() const { return display_ != nullptr && window_ != 0; }
    wingman::WindowHandle handle() const { return window_; }

private:
    Display* display_ = nullptr;
    Window window_ = 0;
};

} // namespace

TEST(X11WmIntegrationTest, MinimizeAndShowRoundtrip) {
    if (!xvfbAvailable() || !openboxAvailable()) {
        GTEST_SKIP() << "Xvfb/openbox not installed — 真实 WM 集成为运行期可选依赖";
    }
    WmEnvironment env;
    ASSERT_TRUE(env.valid()) << env.failReason();
    // isMinimized 在顶层 facade 未导出，经平台后端直查（同 X11WindowPlatformFeatures 先例）
    auto backend = wingman::platform::linux::createX11Window();

    WmTestWindow win(30, 40, 300, 200, "Wingman WM Minimize Window");
    ASSERT_TRUE(win.valid());
    // 映射经 WM 兑现为 viewable（异步）
    ASSERT_TRUE(pollUntil([&] { return wingman::Window::isVisible(win.handle()); }, 3000));
    EXPECT_FALSE(backend->isMinimized(win.handle()));

    // XIconifyWindow → WM 图标化：unmap + 写 _NET_WM_STATE_HIDDEN
    EXPECT_TRUE(wingman::Window::minimize(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return !wingman::Window::isVisible(win.handle()); }, 3000));
    EXPECT_TRUE(backend->isMinimized(win.handle()));

    // XMapWindow → WM 取消图标化（show/hide 为后端方法，facade 未导出）
    EXPECT_TRUE(backend->show(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return wingman::Window::isVisible(win.handle()); }, 3000));
    EXPECT_FALSE(backend->isMinimized(win.handle()));
}

TEST(X11WmIntegrationTest, MaximizeRestoreRoundtrip) {
    if (!xvfbAvailable() || !openboxAvailable()) {
        GTEST_SKIP() << "Xvfb/openbox not installed — 真实 WM 集成为运行期可选依赖";
    }
    WmEnvironment env;
    ASSERT_TRUE(env.valid()) << env.failReason();
    auto backend = wingman::platform::linux::createX11Window();

    WmTestWindow win(50, 60, 200, 120, "Wingman WM Maximize Window");
    ASSERT_TRUE(win.valid());
    ASSERT_TRUE(pollUntil([&] { return wingman::Window::isVisible(win.handle()); }, 3000));

    // maximize() 发送 _NET_WM_STATE MAXIMIZED_HORZ（产品语义只请求水平方向），
    // openbox 兑现后写状态原子并加宽窗口到工作区
    const auto before = wingman::Window::getBounds(win.handle());
    EXPECT_TRUE(wingman::Window::maximize(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return backend->isMaximized(win.handle()); }, 3000));
    const auto maximized = wingman::Window::getBounds(win.handle());
    EXPECT_GT(maximized.width, before.width);

    EXPECT_TRUE(wingman::Window::restore(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return !backend->isMaximized(win.handle()); }, 3000));
}

TEST(X11WmIntegrationTest, ActivateForegroundAndEnumerate) {
    if (!xvfbAvailable() || !openboxAvailable()) {
        GTEST_SKIP() << "Xvfb/openbox not installed — 真实 WM 集成为运行期可选依赖";
    }
    WmEnvironment env;
    ASSERT_TRUE(env.valid()) << env.failReason();

    WmTestWindow win(10, 10, 220, 140, "Wingman WM Active Window");
    ASSERT_TRUE(win.valid());

    // WM 自维护 _NET_CLIENT_LIST：映射后无需手写属性即可被枚举/查找
    ASSERT_TRUE(pollUntil(
        [&] { return wingman::Window::find("WM Active") == win.handle(); }, 3000));

    // _NET_ACTIVE_WINDOW 消息 → WM 设焦点并更新根属性；isForeground/getForeground
    // 读的就是这份 WM 维护的状态
    EXPECT_TRUE(wingman::Window::activate(win.handle()));
    EXPECT_TRUE(pollUntil([&] { return wingman::Window::isForeground(win.handle()); }, 3000));
    EXPECT_EQ(wingman::Window::getForeground(), win.handle());
}

#endif // __linux__
