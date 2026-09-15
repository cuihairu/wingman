// Linux X11 平台实现集成测试：X11Screen（显示器元数据）/ X11Capture（真捕获）/
// XTestInput（注入 + 查询回读）/ 顶层 Clipboard 装配（X11/xclip 后端接线）。
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
#include "wingman/clipboard.hpp"
#include "clipboard_lock_guard.hpp"
#include "wingman/screen.hpp"  // Bitmap 完整定义（icapture.hpp 仅前向声明）

#include <X11/Xlib.h>

#include <cstdlib>
#include <memory>
#include <string>

// 平台工厂由 x11_factory.cpp（include-unity 聚合 TU）导出，无公开头文件；
// 与 input_factory.cpp / clipboard.cpp 的前向声明模式一致。
// gcc 在 Linux 上把 `linux` 定义为 1（遗留宏），命名空间限定需要 undo（同 x11_factory.cpp）
#undef linux
namespace wingman::platform::linux {
std::unique_ptr<ICapture> createX11Capture(const CaptureConfig& config);
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

#endif // __linux__
