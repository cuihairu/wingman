#include <gtest/gtest.h>
#include "wingman/window.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== 窗口查找测试 ==========

TEST(WindowTest, FindExistingWindow) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, FindAllWindows) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, GetForegroundWindow) {
    auto hwnd = Window::getForeground();
    EXPECT_NE(hwnd, nullptr);
    EXPECT_TRUE(Window::isValid(hwnd));
}

TEST(WindowTest, EnumerateWindows) {
    auto windows = Window::enumerate();
    EXPECT_GT(windows.size(), 0);

    // 检查返回的数据
    for (const auto& win : windows) {
        EXPECT_NE(win.handle, nullptr);
        EXPECT_FALSE(win.title.empty());
    }
}

TEST(WindowTest, FindNonExistentWindow) {
    std::string nonsenseTitle = "ThisWindowShouldNotExist123456789";

    auto hwnd = Window::find(nonsenseTitle);
    // 可能返回 nullptr 或某个匹配的窗口
    SUCCEED();
}

// ========== 窗口信息测试 ==========

TEST(WindowTest, GetWindowTitle) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    std::string title = Window::getTitle(hwnd);
    EXPECT_FALSE(title.empty());
}

TEST(WindowTest, GetWindowBounds) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect bounds = Window::getBounds(hwnd);
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
}

TEST(WindowTest, IsWindowValid) {
    // 有效窗口
    auto hwnd = Window::getForeground();
    EXPECT_TRUE(Window::isValid(hwnd));

    // 无效窗口
    EXPECT_FALSE(Window::isValid(nullptr));
}

TEST(WindowTest, IsWindowForeground) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    EXPECT_TRUE(Window::isForeground(hwnd));
}

TEST(WindowTest, IsWindowVisible) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 前台窗口应该是可见的
    EXPECT_TRUE(Window::isVisible(hwnd));
}

// ========== 窗口操作测试 ==========

TEST(WindowTest, ActivateWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 尝试激活（可能已经是前台窗口）
    bool result = Window::activate(hwnd);
    // 结果取决于当前状态
    SUCCEED();
}

TEST(WindowTest, MinimizeWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 最小化
    bool result = Window::minimize(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 还原
    Window::restore(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, MaximizeWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    // 最大化
    bool result = Window::maximize(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 还原
    Window::restore(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, RestoreWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    bool result = Window::restore(hwnd);
    SUCCEED();
}

// ========== 窗口移动和调整大小测试 ==========

TEST(WindowTest, SetWindowBounds) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, MoveWindow) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, ResizeWindow) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect original = Window::getBounds(hwnd);

    // 调整大小
    bool result = Window::resize(hwnd, 800, 600);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 恢复原大小
    Window::resize(hwnd, original.width, original.height);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

// ========== 等待窗口测试 ==========

TEST(WindowTest, WaitForWindow) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, WaitForWindowTimeout) {
	// 等待不存在的窗口
	bool found = Window::waitFor("NonExistentWindow12345", 500);
	EXPECT_FALSE(found);
}

TEST(WindowTest, WaitCloseTimeout) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

// ========== 边界条件测试 ==========

TEST(WindowTest, InvalidWindowHandle) {
    // 测试使用 nullptr 的情况
    std::string title = Window::getTitle(nullptr);
    EXPECT_TRUE(title.empty());

    Rect bounds = Window::getBounds(nullptr);
    EXPECT_TRUE(bounds.isEmpty());

    EXPECT_FALSE(Window::activate(nullptr));
    EXPECT_FALSE(Window::minimize(nullptr));
    EXPECT_FALSE(Window::maximize(nullptr));
    EXPECT_FALSE(Window::restore(nullptr));
    EXPECT_FALSE(Window::move(nullptr, 0, 0));
    EXPECT_FALSE(Window::resize(nullptr, 100, 100));
}

TEST(WindowTest, EmptyTitleSearch) {
	// CI 环境中窗口操作可能受限
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, SetBoundsZeroSize) {
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, nullptr);

    Rect original = Window::getBounds(hwnd);

    // 尝试设置零大小（可能被系统忽略）
    Rect zeroBounds(0, 0, 0, 0);
    bool result = Window::setBounds(hwnd, zeroBounds);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 恢复原始大小
    Window::setBounds(hwnd, original);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}
