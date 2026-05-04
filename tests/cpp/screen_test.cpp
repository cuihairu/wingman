#include <gtest/gtest.h>
#include "wingman/screen.hpp"

using namespace wingman;

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Screen Tests
// ============================================================================

TEST_F(ScreenTest, GetScreenDimensions) {
    int width = Screen::getScreenWidth();
    int height = Screen::getScreenHeight();

    EXPECT_GT(width, 0);
    EXPECT_GT(height, 0);
}

TEST_F(ScreenTest, CaptureScreen) {
    auto bitmap = Screen::capture();
    ASSERT_NE(bitmap, nullptr);
    EXPECT_GT(bitmap->getWidth(), 0);
    EXPECT_GT(bitmap->getHeight(), 0);
}

TEST_F(ScreenTest, GetPixel) {
    Color color = Screen::getPixel(0, 0);
    // Color should be valid (0-255 for each component)
    EXPECT_GE(color.r, 0);
    EXPECT_LE(color.r, 255);
    EXPECT_GE(color.g, 0);
    EXPECT_LE(color.g, 255);
    EXPECT_GE(color.b, 0);
    EXPECT_LE(color.b, 255);
    EXPECT_GE(color.a, 0);
    EXPECT_LE(color.a, 255);
}

TEST_F(ScreenTest, FindColor) {
    // Try to find black color in a small region
    Color black(0, 0, 0, 255);
    Rect region{0, 0, 100, 100};
    Point result;

    bool found = Screen::findColor(black, region, 0, result);
    // Result may or may not be found depending on screen content
    // Just verify the function works without crashing
    SUCCEED();
}

TEST_F(ScreenTest, ColorStruct) {
    Color color(255, 128, 64, 255);
    EXPECT_EQ(color.r, 255);
    EXPECT_EQ(color.g, 128);
    EXPECT_EQ(color.b, 64);
    EXPECT_EQ(color.a, 255);
}
