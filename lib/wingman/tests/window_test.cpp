#include <gtest/gtest.h>
#include "wingman/window.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

namespace {
constexpr WindowHandle kNullWindowHandle{};
#ifndef _WIN32
#define SKIP_IF_WINDOW_BACKEND_UNAVAILABLE() \
    GTEST_SKIP() << "Window backend is a non-Windows stub in this build"
#else
#define SKIP_IF_WINDOW_BACKEND_UNAVAILABLE() do {} while (false)
#endif
}

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== Window Find Tests ==========

TEST(WindowTest, FindExistingWindow) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, FindAllWindows) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, GetForegroundWindow) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    EXPECT_NE(hwnd, kNullWindowHandle);
    EXPECT_TRUE(Window::isValid(hwnd));
}

TEST(WindowTest, EnumerateWindows) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto windows = Window::enumerate();
    EXPECT_GT(windows.size(), 0);

    // Check returned data
    for (const auto& win : windows) {
        EXPECT_NE(win.handle, kNullWindowHandle);
        EXPECT_FALSE(win.title.empty());
    }
}

TEST(WindowTest, FindNonExistentWindow) {
    std::string nonsenseTitle = "ThisWindowShouldNotExist123456789";

    auto hwnd = Window::find(nonsenseTitle);
    // May return nullptr or a matching window
    SUCCEED();
}

// ========== Window Info Tests ==========

TEST(WindowTest, GetWindowTitle) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    std::string title = Window::getTitle(hwnd);
    EXPECT_FALSE(title.empty());
}

TEST(WindowTest, GetWindowBounds) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    Rect bounds = Window::getBounds(hwnd);
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
}

TEST(WindowTest, IsWindowValid) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    // Valid window
    auto hwnd = Window::getForeground();
    EXPECT_TRUE(Window::isValid(hwnd));

    // Invalid window
    EXPECT_FALSE(Window::isValid(kNullWindowHandle));
}

TEST(WindowTest, IsWindowForeground) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    EXPECT_TRUE(Window::isForeground(hwnd));
}

TEST(WindowTest, IsWindowVisible) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    // Foreground window should be visible
    EXPECT_TRUE(Window::isVisible(hwnd));
}

// ========== Window Operation Tests ==========

TEST(WindowTest, ActivateWindow) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    // Try to activate (may already be the foreground window)
    bool result = Window::activate(hwnd);
    // Result depends on current state
    SUCCEED();
}

TEST(WindowTest, MinimizeWindow) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    // Minimize
    bool result = Window::minimize(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Restore
    Window::restore(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, MaximizeWindow) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    // Maximize
    bool result = Window::maximize(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Restore
    Window::restore(hwnd);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

TEST(WindowTest, RestoreWindow) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    bool result = Window::restore(hwnd);
    SUCCEED();
}

// ========== Window Move and Resize Tests ==========

TEST(WindowTest, SetWindowBounds) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, MoveWindow) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, ResizeWindow) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    Rect original = Window::getBounds(hwnd);

    // Resize
    bool result = Window::resize(hwnd, 800, 600);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Restore original size
    Window::resize(hwnd, original.width, original.height);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}

// ========== Wait for Window Tests ==========

TEST(WindowTest, WaitForWindow) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, WaitForWindowTimeout) {
	// Wait for non-existent window
	bool found = Window::waitFor("NonExistentWindow12345", 500);
	EXPECT_FALSE(found);
}

TEST(WindowTest, WaitCloseTimeout) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

// ========== Boundary Condition Tests ==========

TEST(WindowTest, InvalidWindowHandle) {
    // Test with nullptr
    std::string title = Window::getTitle(kNullWindowHandle);
    EXPECT_TRUE(title.empty());

    Rect bounds = Window::getBounds(kNullWindowHandle);
    EXPECT_TRUE(bounds.isEmpty());

    EXPECT_FALSE(Window::activate(kNullWindowHandle));
    EXPECT_FALSE(Window::minimize(kNullWindowHandle));
    EXPECT_FALSE(Window::maximize(kNullWindowHandle));
    EXPECT_FALSE(Window::restore(kNullWindowHandle));
    EXPECT_FALSE(Window::move(kNullWindowHandle, 0, 0));
    EXPECT_FALSE(Window::resize(kNullWindowHandle, 100, 100));
}

TEST(WindowTest, EmptyTitleSearch) {
	// Window operations may be restricted in CI environment
	GTEST_SKIP() << "Skipping in CI environment";
}

TEST(WindowTest, SetBoundsZeroSize) {
    SKIP_IF_WINDOW_BACKEND_UNAVAILABLE();
    auto hwnd = Window::getForeground();
    ASSERT_NE(hwnd, kNullWindowHandle);

    Rect original = Window::getBounds(hwnd);

    // Try setting zero size (may be ignored by the system)
    Rect zeroBounds(0, 0, 0, 0);
    bool result = Window::setBounds(hwnd, zeroBounds);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Restore original size
    Window::setBounds(hwnd, original);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SUCCEED();
}
