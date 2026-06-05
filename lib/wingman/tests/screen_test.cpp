#include <gtest/gtest.h>
#include "wingman/screen.hpp"
#include <fstream>
#include <algorithm>
#include <filesystem>

using namespace wingman;

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== Color Tests ==========

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
    // distance = 5^2 + 5^2 + 5^2 = 75
    // tolerance^2 must be >= 75 to match
    EXPECT_TRUE(c1.matches(c2, 9));  // 9^2 = 81 >= 75
    EXPECT_FALSE(c1.matches(c2, 8));  // 8^2 = 64 < 75
}

TEST(ScreenTest, ColorExactMatch) {
    Color c1(100, 100, 100);
    Color c2(100, 100, 100);
    EXPECT_TRUE(c1.matches(c2, 0));
}

// ========== Point Tests ==========

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

// ========== Rect Tests ==========

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

    Point p1(50, 50);  // Inside
    EXPECT_TRUE(r.contains(p1));

    Point p2(10, 10);  // Boundary
    EXPECT_TRUE(r.contains(p2));

    Point p3(109, 109);  // Boundary
    EXPECT_TRUE(r.contains(p3));

    Point p4(0, 0);  // Outside
    EXPECT_FALSE(r.contains(p4));

    Point p5(110, 110);  // Outside
    EXPECT_FALSE(r.contains(p5));
}

// ========== Screen Functional Tests (Windows only) ==========

#ifdef _WIN32

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
    // Just verify it doesn't crash
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
    // Search for black in top-left corner
    Color targetColor(0, 0, 0);
    Rect region(0, 0, 100, 100);
    Point result;

    // Exact match with zero tolerance
    bool found = Screen::findColor(targetColor, region, 0, result);
    // Result depends on screen content, just verify no crash
    SUCCEED();
}

TEST(ScreenTest, FindColorsMultiple) {
    Color targetColor(0, 0, 0);
    Rect region(0, 0, 100, 100);

    auto points = Screen::findColors(targetColor, region, 10, 10);
    // Result depends on screen content, just verify no crash
    EXPECT_GE(points.size(), 0);
}

#endif // _WIN32

// ========== Bitmap Tests ==========

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

TEST(ScreenTest, BitmapSaveWritesFile) {
    Bitmap bmp(2, 2);
    bmp.setPixel(0, 0, Color(255, 0, 0));
    bmp.setPixel(1, 0, Color(0, 255, 0));
    bmp.setPixel(0, 1, Color(0, 0, 255));
    bmp.setPixel(1, 1, Color(255, 255, 255));

    const auto outputPath = std::filesystem::temp_directory_path() / "wingman-screen-test.png";
    std::error_code ec;
    std::filesystem::remove(outputPath, ec);

    EXPECT_TRUE(bmp.save(outputPath.string()));
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    std::filesystem::remove(outputPath, ec);
}

// ========== Additional Color Tests ==========

TEST(ScreenTest, ColorDefaultConstructor) {
    Color c;
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    EXPECT_EQ(c.a, 255);
}

TEST(ScreenTest, ColorWithAlpha) {
    Color c(100, 150, 200, 128);
    EXPECT_EQ(c.r, 100);
    EXPECT_EQ(c.g, 150);
    EXPECT_EQ(c.b, 200);
    EXPECT_EQ(c.a, 128);
}

TEST(ScreenTest, ColorFromRGBBlack) {
    Color c = Color::fromRGB(0x000000);
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
}

TEST(ScreenTest, ColorFromRGBWhite) {
    Color c = Color::fromRGB(0xFFFFFF);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 255);
    EXPECT_EQ(c.b, 255);
}

TEST(ScreenTest, ColorToRGBRoundtrip) {
    Color original(42, 87, 213);
    uint32_t rgb = original.toRGB();
    Color restored = Color::fromRGB(rgb);
    EXPECT_EQ(restored.r, original.r);
    EXPECT_EQ(restored.g, original.g);
    EXPECT_EQ(restored.b, original.b);
}

TEST(ScreenTest, ColorDistanceSame) {
    Color c(100, 100, 100);
    EXPECT_EQ(c.distance(c), 0);
}

TEST(ScreenTest, ColorDistanceMax) {
    Color black(0, 0, 0);
    Color white(255, 255, 255);
    EXPECT_EQ(black.distance(white), 255*255 + 255*255 + 255*255);
}

// ========== Additional Point Tests ==========

TEST(ScreenTest, PointAssignment) {
    Point p1(10, 20);
    Point p2;
    p2 = p1;
    EXPECT_EQ(p2.x, 10);
    EXPECT_EQ(p2.y, 20);
}

TEST(ScreenTest, PointInequality) {
    Point p1(0, 0);
    Point p2(1, 0);
    Point p3(0, 1);
    EXPECT_NE(p1, p2);
    EXPECT_NE(p1, p3);
}

// ========== Additional Rect Tests ==========

TEST(ScreenTest, RectDefaultConstructor) {
    Rect r;
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 0);
    EXPECT_EQ(r.width, 0);
    EXPECT_EQ(r.height, 0);
    EXPECT_TRUE(r.isEmpty());
}

TEST(ScreenTest, RectNegativeDimensions) {
    Rect r(0, 0, -10, -5);
    EXPECT_TRUE(r.isEmpty());
}

TEST(ScreenTest, RectContainsEdgeCases) {
    Rect r(10, 10, 100, 100);
    EXPECT_TRUE(r.contains(Point(10, 10)));
    EXPECT_FALSE(r.contains(Point(110, 10)));
    EXPECT_FALSE(r.contains(Point(10, 110)));
    EXPECT_FALSE(r.contains(Point(110, 110)));
}

TEST(ScreenTest, RectContainsInEmptyRect) {
    Rect r(0, 0, 0, 0);
    EXPECT_FALSE(r.contains(Point(0, 0)));
}

// ========== Additional Bitmap Tests ==========

TEST(ScreenTest, BitmapGetPixelUnset) {
    Bitmap bmp(10, 10);
    Color c = bmp.getPixel(5, 5);
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
}

TEST(ScreenTest, BitmapMultipleSetPixel) {
    Bitmap bmp(10, 10);
    bmp.setPixel(0, 0, Color(255, 0, 0));
    bmp.setPixel(9, 9, Color(0, 255, 0));
    bmp.setPixel(5, 5, Color(0, 0, 255));

    EXPECT_EQ(bmp.getPixel(0, 0).r, 255);
    EXPECT_EQ(bmp.getPixel(9, 9).g, 255);
    EXPECT_EQ(bmp.getPixel(5, 5).b, 255);
}

#ifdef __APPLE__

TEST(ScreenTest, GetScreenBoundsOnMac) {
    Rect bounds = Screen::getScreenBounds();
    EXPECT_GT(bounds.width, 0);
    EXPECT_GT(bounds.height, 0);
}

TEST(ScreenTest, GetScreenDimensionsOnMac) {
    EXPECT_GT(Screen::getScreenWidth(), 0);
    EXPECT_GT(Screen::getScreenHeight(), 0);
}

#endif // __APPLE__

// ========== Bitmap move semantics ==========

TEST(ScreenTest, BitmapMoveConstructor) {
    Bitmap bmp1(50, 50);
    bmp1.setPixel(10, 10, Color(255, 128, 64));
    Bitmap bmp2(std::move(bmp1));
    EXPECT_EQ(bmp2.getWidth(), 50);
    EXPECT_EQ(bmp2.getHeight(), 50);
    Color c = bmp2.getPixel(10, 10);
    EXPECT_EQ(c.r, 255);
    EXPECT_EQ(c.g, 128);
    EXPECT_EQ(c.b, 64);
    EXPECT_EQ(bmp1.getWidth(), 0);
    EXPECT_EQ(bmp1.getHeight(), 0);
}

TEST(ScreenTest, BitmapMoveAssignment) {
    Bitmap bmp1(30, 30);
    bmp1.setPixel(5, 5, Color(10, 20, 30));
    Bitmap bmp2(10, 10);
    bmp2 = std::move(bmp1);
    EXPECT_EQ(bmp2.getWidth(), 30);
    EXPECT_EQ(bmp2.getHeight(), 30);
    Color c = bmp2.getPixel(5, 5);
    EXPECT_EQ(c.r, 10);
    EXPECT_EQ(c.g, 20);
    EXPECT_EQ(c.b, 30);
    EXPECT_EQ(bmp1.getWidth(), 0);
}

TEST(ScreenTest, BitmapCopyAssignment) {
    Bitmap bmp1(20, 20);
    bmp1.setPixel(3, 3, Color(100, 150, 200));
    Bitmap bmp2(5, 5);
    bmp2 = bmp1;
    EXPECT_EQ(bmp2.getWidth(), 20);
    EXPECT_EQ(bmp2.getHeight(), 20);
    Color c = bmp2.getPixel(3, 3);
    EXPECT_EQ(c.r, 100);
    EXPECT_EQ(c.g, 150);
    EXPECT_EQ(c.b, 200);
}

TEST(ScreenTest, BitmapSelfAssignment) {
    Bitmap bmp(15, 15);
    bmp.setPixel(7, 7, Color(42, 42, 42));
    bmp = bmp;
    EXPECT_EQ(bmp.getWidth(), 15);
    Color c = bmp.getPixel(7, 7);
    EXPECT_EQ(c.r, 42);
}

TEST(ScreenTest, BitmapGetPixelOutOfBounds) {
    Bitmap bmp(10, 10);
    Color c = bmp.getPixel(-1, 0);
    EXPECT_EQ(c.r, 0);
    EXPECT_EQ(c.g, 0);
    EXPECT_EQ(c.b, 0);
    c = bmp.getPixel(10, 0);
    EXPECT_EQ(c.r, 0);
    c = bmp.getPixel(0, -1);
    EXPECT_EQ(c.r, 0);
    c = bmp.getPixel(0, 10);
    EXPECT_EQ(c.r, 0);
}

TEST(ScreenTest, BitmapSetPixelOutOfBounds) {
    Bitmap bmp(10, 10);
    // Should not crash
    bmp.setPixel(-1, 0, Color(255, 0, 0));
    bmp.setPixel(10, 0, Color(255, 0, 0));
    bmp.setPixel(0, -1, Color(255, 0, 0));
    bmp.setPixel(0, 10, Color(255, 0, 0));
}

TEST(ScreenTest, BitmapFromNonexistentFileReturnsNull) {
    auto bmp = Bitmap::fromFile("/nonexistent/path/image.png");
    EXPECT_EQ(bmp, nullptr);
}

TEST(ScreenTest, BitmapGetDataReturnsValidPointer) {
    Bitmap bmp(5, 5);
    EXPECT_NE(bmp.getData(), nullptr);
}

TEST(ScreenTest, BitmapWidthHeight) {
    Bitmap bmp(10, 20);
    EXPECT_EQ(bmp.getWidth(), 10);
    EXPECT_EQ(bmp.getHeight(), 20);
}
