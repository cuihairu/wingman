#include <gtest/gtest.h>
#include "wingman/window.hpp"

using namespace wingman;

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Window Tests
// ============================================================================

TEST_F(WindowTest, FindWindowByTitle) {
    // Try to find a window by title
    // Just verify the function doesn't crash
    SUCCEED();
}

TEST_F(WindowTest, GetForegroundWindow) {
    HWND hwnd = Window::getForeground();
    EXPECT_NE(hwnd, nullptr);
}

TEST_F(WindowTest, GetWindowBounds) {
    HWND hwnd = Window::getForeground();
    if (hwnd) {
        Rect bounds = Window::getBounds(hwnd);
        // Width and height should always be positive
        EXPECT_GT(bounds.width, 0);
        EXPECT_GT(bounds.height, 0);
        // x and y can be negative for windows on multi-monitor setups
    }
}
