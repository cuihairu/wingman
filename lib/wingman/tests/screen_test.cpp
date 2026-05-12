#include <gtest/gtest.h>
#include "wingman/screen.hpp"
#include <fstream>
#include <algorithm>

using namespace wingman;

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== Color 测试 ==========

TEST(ScreenTest, ColorFromRGB) {
    Color c = Color::fromRGB(0xFF0000);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
}

TEST(ScreenTest, ColorToRGB) {
    Color c(255, 128, 64);
    EXPECT_EQ(c.toRGB(), 0xFF8040);
}

TEST(ScreenTest, ColorDistance) {
    Color c1(255, 0, 0);
    Color c2(255, 10, 0);
    EXPECT_EQ(c1.distance(c2), 100);
}

TEST(ScreenTest, ColorMatches) {
    Color c1(100, 100, 100);
    Color c2(105, 105, 105);
    EXPECT_TRUE(c1.matches(c2, 8));  // sqrt(75) ≈ 8.66
    EXPECT_FALSE(c1.matches(c2, 5));
}

TEST(ScreenTest, ColorExactMatch) {
    Color c1(100, 100, 100);
    Color c2(100, 100, 100);
    EXPECT_TRUE(c1.matches(c2, 0));
}

// ========== Point 测试 ==========

TEST(ScreenTest, PointConstructor) {
    Point p(10, 20);
    EXPECT_EQ(p.x, 10);
    EXPECT_EQ(p.y, 20);
}

TEST(ScreenTest, PointDefaultConstructor) {
    Point p;
    EXPECT_EQ(p.x, 0);
    EXPECT_EQ(p.y, 0);
}

TEST(ScreenTest, PointEquality) {
    Point p1(10, 20);
    Point p2(10, 20);
    Point p3(10, 30);
    EXPECT_EQ(p1, p2);
    EXPECT_NE(p1, p3);
}

// ========== Rect 测试 ==========

TEST(ScreenTest, RectConstructor) {
    Rect r(10, 20, 100, 200);
    EXPECT_EQ(r.x, 10);
    EXPECT_EQ(r.y, 20);
    EXPECT_EQ(r.width, 100);
    EXPECT_EQ(r.height, 200);
}

TEST(ScreenTest, RectIsEmpty) {
    Rect r1(0, 0, 0, 100);
    EXPECT_TRUE(r1.isEmpty());

    Rect r2(0, 0, 100, 0);
    EXPECT_TRUE(r2.isEmpty());

    Rect r3(0, 0, 100, 100);
    EXPECT_FALSE(r3.isEmpty());
}

TEST(ScreenTest, RectContains) {
    Rect r(10, 10, 100, 100);

    Point p1(50, 50);  // 内部
    EXPECT_TRUE(r.contains(p1));

    Point p2(10, 10);  // 边界
    EXPECT_TRUE(r.contains(p2));

    Point p3(109, 109);  // 边界
    EXPECT_TRUE(r.contains(p3));

    Point p4(0, 0);  // 外部
    EXPECT_FALSE(r.contains(p4));

    Point p5(110, 110);  // 外部
    EXPECT_FALSE(r.contains(p5));
}

// ========== Screen 功能测试 ==========

TEST(ScreenTest, GetScreenBounds) {
    Rect bounds = Screen::getScreenBounds();
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
}

TEST(ScreenTest, GetScreenWidth) {
    int width = Screen::getScreenWidth();
    EXPECT_GT(width, 0);
    EXPECT_EQ(width, Screen::getScreenBounds().width);
}

TEST(ScreenTest, GetScreenHeight) {
    int height = Screen::getScreenHeight();
    EXPECT_GT(height, 0);
    EXPECT_EQ(height, Screen::getScreenBounds().height);
}

TEST(ScreenTest, GetPixel) {
    Color c = Screen::getPixel(0, 0);
    // 只要不崩溃就算通过
    SUCCEED();
}

TEST(ScreenTest, CaptureFullScreen) {
    auto bitmap = Screen::capture();
    ASSERT_NE(bitmap, nullptr);
    EXPECT_GT(bitmap->getWidth(), 0);
    EXPECT_GT(bitmap->getHeight(), 0);
}

TEST(ScreenTest, CaptureRegion) {
    Rect region(100, 100, 200, 200);
    auto bitmap = Screen::capture(region);
    ASSERT_NE(bitmap, nullptr);
    EXPECT_EQ(bitmap->getWidth(), 200);
    EXPECT_EQ(bitmap->getHeight(), 200);
}

TEST(ScreenTest, FindColorSimple) {
    // 在屏幕左上角查找黑色
    Color targetColor(0, 0, 0);
    Rect region(0, 0, 100, 100);
    Point result;

    // 容差设为0，精确匹配
    bool found = Screen::findColor(targetColor, region, 0, result);
    // 结果取决于屏幕内容，只检查不崩溃
    SUCCEED();
}

TEST(ScreenTest, FindColorsMultiple) {
    Color targetColor(0, 0, 0);
    Rect region(0, 0, 100, 100);

    auto points = Screen::findColors(targetColor, region, 10, 10);
    // 结果取决于屏幕内容，只检查不崩溃
    EXPECT_GE(points.size(), 0);
}

// ========== Bitmap 测试 ==========

TEST(ScreenTest, BitmapCreate) {
    Bitmap bmp(100, 100);
    EXPECT_EQ(bmp.getWidth(), 100);
    EXPECT_EQ(bmp.getHeight(), 100);
}

TEST(ScreenTest, BitmapGetSetPixel) {
    Bitmap bmp(100, 100);
    Color c(255, 128, 64);

    bmp.setPixel(50, 50, c);
    Color result = bmp.getPixel(50, 50);

    EXPECT_EQ(result.r, c.r);
    EXPECT_EQ(result.g, c.g);
    EXPECT_EQ(result.b, c.b);
}

TEST(ScreenTest, BitmapCopy) {
    Bitmap bmp1(100, 100);
    Color c(255, 128, 64);
    bmp1.setPixel(50, 50, c);

    Bitmap bmp2(bmp1);
    Color result = bmp2.getPixel(50, 50);

    EXPECT_EQ(result.r, c.r);
    EXPECT_EQ(result.g, c.g);
    EXPECT_EQ(result.b, c.b);
}
