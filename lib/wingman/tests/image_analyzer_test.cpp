#include <gtest/gtest.h>
#include "wingman/vision/image_analyzer.hpp"
#include <filesystem>
#include <atomic>
#include <chrono>
#include <thread>

using namespace wingman;
using namespace wingman::vision;

// ========== Static helper tests ==========

TEST(ImageAnalyzerTest, ColorDistanceSame) {
    Color c(100, 200, 50);
    EXPECT_EQ(ImageAnalyzer::colorDistance(c, c), 0);
}

TEST(ImageAnalyzerTest, ColorDistanceBlackWhite) {
    Color black(0, 0, 0);
    Color white(255, 255, 255);
    EXPECT_EQ(ImageAnalyzer::colorDistance(black, white), 255 * 255 * 3);
}

TEST(ImageAnalyzerTest, ColorDistanceSingleChannel) {
    Color c1(0, 0, 0);
    Color c2(10, 0, 0);
    EXPECT_EQ(ImageAnalyzer::colorDistance(c1, c2), 100);
}

TEST(ImageAnalyzerTest, ColorMatchesExactZeroTolerance) {
    Color c(50, 50, 50);
    EXPECT_TRUE(ImageAnalyzer::colorMatches(c, c, 0));
}

TEST(ImageAnalyzerTest, ColorMatchesWithinTolerance) {
    Color c1(100, 100, 100);
    Color c2(105, 105, 105);
    // distance = 75, tolerance=9 => 81 >= 75
    EXPECT_TRUE(ImageAnalyzer::colorMatches(c1, c2, 9));
    EXPECT_FALSE(ImageAnalyzer::colorMatches(c1, c2, 8));
}

TEST(ImageAnalyzerTest, ClampRegionFullyInside) {
    Bitmap bmp(100, 100);
    Rect region(10, 10, 50, 50);
    Rect clamped = ImageAnalyzer::clampRegion(region, bmp);
    EXPECT_EQ(clamped.x, 10);
    EXPECT_EQ(clamped.y, 10);
    EXPECT_EQ(clamped.width, 50);
    EXPECT_EQ(clamped.height, 50);
}

TEST(ImageAnalyzerTest, ClampRegionExceedsBounds) {
    Bitmap bmp(100, 100);
    Rect region(80, 80, 50, 50);
    Rect clamped = ImageAnalyzer::clampRegion(region, bmp);
    EXPECT_EQ(clamped.x, 80);
    EXPECT_EQ(clamped.y, 80);
    EXPECT_EQ(clamped.width, 20);
    EXPECT_EQ(clamped.height, 20);
}

TEST(ImageAnalyzerTest, ClampRegionNegativeOrigin) {
    Bitmap bmp(100, 100);
    Rect region(-10, -10, 50, 50);
    Rect clamped = ImageAnalyzer::clampRegion(region, bmp);
    EXPECT_EQ(clamped.x, 0);
    EXPECT_EQ(clamped.y, 0);
    EXPECT_EQ(clamped.width, 50);
    EXPECT_EQ(clamped.height, 50);
}

TEST(ImageAnalyzerTest, ClampRegionLargerThanBitmap) {
    Bitmap bmp(50, 50);
    Rect region(0, 0, 200, 200);
    Rect clamped = ImageAnalyzer::clampRegion(region, bmp);
    EXPECT_EQ(clamped.x, 0);
    EXPECT_EQ(clamped.y, 0);
    EXPECT_EQ(clamped.width, 50);
    EXPECT_EQ(clamped.height, 50);
}

// ========== findColor tests ==========

TEST(ImageAnalyzerTest, FindColorExactMatch) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);
    bmp.setPixel(5, 5, red);

    ImageAnalyzer analyzer;
    auto result = analyzer.findColor(bmp, red, Rect{0, 0, 10, 10}, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->position.x, 5);
    EXPECT_EQ(result->position.y, 5);
    EXPECT_EQ(result->foundColor.r, 255);
    EXPECT_EQ(result->distance, 0);
    EXPECT_DOUBLE_EQ(result->confidence, 1.0);
}

TEST(ImageAnalyzerTest, FindColorNotFound) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);

    ImageAnalyzer analyzer;
    auto result = analyzer.findColor(bmp, red, Rect{0, 0, 10, 10}, 0);
    EXPECT_FALSE(result.has_value());
}

TEST(ImageAnalyzerTest, FindColorWithTolerance) {
    Bitmap bmp(10, 10);
    Color target(100, 100, 100);
    Color actual(103, 103, 103);
    bmp.setPixel(3, 3, actual);

    ImageAnalyzer analyzer;
    auto result = analyzer.findColor(bmp, target, Rect{0, 0, 10, 10}, 10);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->position.x, 3);
    EXPECT_EQ(result->position.y, 3);
}

TEST(ImageAnalyzerTest, FindColorEmptyRegionUsesFullBitmap) {
    Bitmap bmp(5, 5);
    Color blue(0, 0, 255);
    bmp.setPixel(4, 4, blue);

    ImageAnalyzer analyzer;
    // Empty region should search the full bitmap
    auto result = analyzer.findColor(bmp, blue, Rect{}, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->position.x, 4);
    EXPECT_EQ(result->position.y, 4);
}

TEST(ImageAnalyzerTest, FindColorInSubregion) {
    Bitmap bmp(20, 20);
    Color green(0, 255, 0);
    bmp.setPixel(15, 15, green);

    ImageAnalyzer analyzer;
    // Search in region that does not include (15,15)
    auto result = analyzer.findColor(bmp, green, Rect{0, 0, 10, 10}, 0);
    EXPECT_FALSE(result.has_value());
}

// ========== findColors tests ==========

TEST(ImageAnalyzerTest, FindColorsMultipleMatches) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);
    bmp.setPixel(0, 0, red);
    bmp.setPixel(5, 5, red);
    bmp.setPixel(9, 9, red);

    ImageAnalyzer analyzer;
    auto results = analyzer.findColors(bmp, red, Rect{0, 0, 10, 10}, 0, 0);
    EXPECT_EQ(results.size(), 3u);
}

TEST(ImageAnalyzerTest, FindColorsWithMaxCount) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);
    bmp.setPixel(0, 0, red);
    bmp.setPixel(5, 5, red);
    bmp.setPixel(9, 9, red);

    ImageAnalyzer analyzer;
    auto results = analyzer.findColors(bmp, red, Rect{0, 0, 10, 10}, 0, 2);
    EXPECT_EQ(results.size(), 2u);
}

TEST(ImageAnalyzerTest, FindColorsNoneFound) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);

    ImageAnalyzer analyzer;
    auto results = analyzer.findColors(bmp, red, Rect{0, 0, 10, 10}, 0, 0);
    EXPECT_EQ(results.size(), 0u);
}

TEST(ImageAnalyzerTest, FindColorsEmptyRegionUsesFullBitmap) {
    Bitmap bmp(5, 5);
    Color white(255, 255, 255);
    bmp.setPixel(2, 2, white);

    ImageAnalyzer analyzer;
    auto results = analyzer.findColors(bmp, white, Rect{}, 0, 0);
    EXPECT_EQ(results.size(), 1u);
}

// ========== findImage tests ==========

TEST(ImageAnalyzerTest, FindImageTemplateLargerThanRegion) {
    Bitmap bmp(10, 10);
    Bitmap tpl(20, 20);

    ImageAnalyzer analyzer;
    auto result = analyzer.findImage(bmp, tpl, Rect{0, 0, 10, 10}, 0.8);
    EXPECT_FALSE(result.has_value());
}

TEST(ImageAnalyzerTest, FindImageExactMatch) {
    // Create a bitmap and a template that is a sub-region of it
    Bitmap bmp(20, 20);
    Color red(255, 0, 0);
    // Fill a 5x5 block at (5,5) with red
    for (int y = 5; y < 10; ++y)
        for (int x = 5; x < 10; ++x)
            bmp.setPixel(x, y, red);

    Bitmap tpl(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            tpl.setPixel(x, y, red);

    ImageAnalyzer analyzer;
    auto result = analyzer.findImage(bmp, tpl, Rect{0, 0, 20, 20}, 0.9);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->position.x, 5);
    EXPECT_EQ(result->position.y, 5);
    EXPECT_GE(result->confidence, 0.9);
}

TEST(ImageAnalyzerTest, FindImageNoMatch) {
    Bitmap bmp(20, 20);
    // bmp is all black
    Bitmap tpl(5, 5);
    Color white(255, 255, 255);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            tpl.setPixel(x, y, white);

    ImageAnalyzer analyzer;
    auto result = analyzer.findImage(bmp, tpl, Rect{0, 0, 20, 20}, 0.99);
    EXPECT_FALSE(result.has_value());
}

TEST(ImageAnalyzerTest, FindImageFromNonexistentPathReturnsNull) {
    Bitmap bmp(10, 10);
    ImageAnalyzer analyzer;
    auto result = analyzer.findImage(bmp, "/nonexistent/template.png", Rect{}, 0.8);
    EXPECT_FALSE(result.has_value());
}

TEST(ImageAnalyzerTest, FindImageEmptyRegionUsesFullBitmap) {
    Bitmap bmp(15, 15);
    Color blue(0, 0, 200);
    for (int y = 10; y < 15; ++y)
        for (int x = 10; x < 15; ++x)
            bmp.setPixel(x, y, blue);

    Bitmap tpl(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 5; ++x)
            tpl.setPixel(x, y, blue);

    ImageAnalyzer analyzer;
    auto result = analyzer.findImage(bmp, tpl, Rect{}, 0.9);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->position.x, 10);
    EXPECT_EQ(result->position.y, 10);
}

// ========== findColorFromSource tests ==========

TEST(ImageAnalyzerTest, FindColorFromNullSource) {
    ImageAnalyzer analyzer;
    auto result = analyzer.findColorFromSource(nullptr, Color{255, 0, 0}, Rect{}, 0);
    EXPECT_FALSE(result.has_value());
}

// ========== findColorAsync tests ==========

TEST(ImageAnalyzerTest, FindColorAsyncCallbackInvoked) {
    Bitmap bmp(5, 5);
    Color red(255, 0, 0);
    bmp.setPixel(2, 2, red);

    std::atomic<bool> called{false};
    ImageAnalyzer analyzer;
    analyzer.findColorAsync(bmp, red, Rect{0, 0, 5, 5}, 0,
        [&](std::optional<ColorMatch> result) {
            called.store(true);
        });

    // Wait for async callback
    for (int i = 0; i < 50 && !called.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(called.load());
}

TEST(ImageAnalyzerTest, FindColorAsyncFindsMatch) {
    Bitmap bmp(10, 10);
    Color green(0, 255, 0);
    bmp.setPixel(7, 7, green);

    std::atomic<bool> called{false};
    std::optional<ColorMatch> asyncResult;
    ImageAnalyzer analyzer;
    analyzer.findColorAsync(bmp, green, Rect{0, 0, 10, 10}, 0,
        [&](std::optional<ColorMatch> result) {
            asyncResult = result;
            called.store(true);
        });

    for (int i = 0; i < 50 && !called.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(asyncResult.has_value());
    EXPECT_EQ(asyncResult->position.x, 7);
    EXPECT_EQ(asyncResult->position.y, 7);
}

// ========== PatternMatcher::matchColorPattern tests ==========

TEST(PatternMatcherTest, MatchColorPatternEmptyColors) {
    Bitmap bmp(10, 10);
    auto result = PatternMatcher::matchColorPattern(bmp, {}, {}, Rect{0, 0, 10, 10}, 0);
    EXPECT_FALSE(result.has_value());
}

TEST(PatternMatcherTest, MatchColorPatternMismatchedSizes) {
    Bitmap bmp(10, 10);
    std::vector<Color> colors = {Color{255, 0, 0}};
    std::vector<Point> offsets = {Point{0, 0}, Point{1, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 10, 10}, 0);
    EXPECT_FALSE(result.has_value());
}

TEST(PatternMatcherTest, MatchColorPatternSingleColor) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);
    bmp.setPixel(3, 3, red);

    std::vector<Color> colors = {red};
    std::vector<Point> offsets = {Point{0, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 10, 10}, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->x, 3);
    EXPECT_EQ(result->y, 3);
}

TEST(PatternMatcherTest, MatchColorPatternMultipleColors) {
    Bitmap bmp(20, 20);
    Color red(255, 0, 0);
    Color green(0, 255, 0);

    // Set pattern: red at (5,5), green at (7,5)
    bmp.setPixel(5, 5, red);
    bmp.setPixel(7, 5, green);

    std::vector<Color> colors = {red, green};
    std::vector<Point> offsets = {Point{0, 0}, Point{2, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 20, 20}, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->x, 5);
    EXPECT_EQ(result->y, 5);
}

TEST(PatternMatcherTest, MatchColorPatternNotFound) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);
    Color green(0, 255, 0);

    // Only set red, no green nearby
    bmp.setPixel(5, 5, red);

    std::vector<Color> colors = {red, green};
    std::vector<Point> offsets = {Point{0, 0}, Point{1, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 10, 10}, 0);
    EXPECT_FALSE(result.has_value());
}

TEST(PatternMatcherTest, MatchColorPatternOutOfBounds) {
    Bitmap bmp(10, 10);
    Color red(255, 0, 0);
    // Place red at the edge so second check would be out of bounds
    bmp.setPixel(9, 9, red);

    std::vector<Color> colors = {red, red};
    std::vector<Point> offsets = {Point{0, 0}, Point{1, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 10, 10}, 0);
    EXPECT_FALSE(result.has_value());
}

TEST(PatternMatcherTest, MatchColorPatternWithTolerance) {
    Bitmap bmp(10, 10);
    Color target(100, 100, 100);
    Color actual(102, 102, 102);
    bmp.setPixel(3, 3, actual);

    std::vector<Color> colors = {target};
    std::vector<Point> offsets = {Point{0, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 10, 10}, 10);
    ASSERT_TRUE(result.has_value());
}

// ========== PatternMatcher::templateMatch tests ==========

TEST(PatternMatcherTest, TemplateMatchIdenticalContent) {
    // Create a bitmap with a gradient pattern (non-uniform)
    Bitmap bmp(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            bmp.setPixel(x, y, Color(x * 25, y * 25, 128));

    // Template is the exact same content as a sub-region
    Bitmap tpl(3, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            tpl.setPixel(x, y, bmp.getPixel(x, y));

    Point bestPos;
    double score = PatternMatcher::templateMatch(bmp, tpl, bestPos, Rect{0, 0, 10, 10});
    // Perfect match at (0,0) since template is copied from there
    EXPECT_GT(score, 0.99);
    EXPECT_EQ(bestPos.x, 0);
    EXPECT_EQ(bestPos.y, 0);
}

TEST(PatternMatcherTest, TemplateMatchLargerTemplate) {
    Bitmap bmp(5, 5);
    Bitmap tpl(10, 10);

    Point bestPos;
    double score = PatternMatcher::templateMatch(bmp, tpl, bestPos, Rect{0, 0, 5, 5});
    EXPECT_DOUBLE_EQ(score, 0.0);
}

TEST(PatternMatcherTest, TemplateMatchEmptyRegionUsesFullBitmap) {
    // Non-uniform bitmap with gradient
    Bitmap bmp(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            bmp.setPixel(x, y, Color(x * 25, y * 25, 128));

    // Template copied from top-left
    Bitmap tpl(2, 2);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            tpl.setPixel(x, y, bmp.getPixel(x, y));

    Point bestPos;
    double score = PatternMatcher::templateMatch(bmp, tpl, bestPos);
    EXPECT_GT(score, 0.99);
}

TEST(PatternMatcherTest, TemplateMatchReturnsBestPosition) {
    // Use a non-uniform background so NCC can differentiate positions
    Bitmap bmp(20, 20);
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
            bmp.setPixel(x, y, Color(x * 5, y * 5, 100));

    // Place a distinct non-uniform 3x3 pattern at (10, 10)
    for (int y = 10; y < 13; ++y)
        for (int x = 10; x < 13; ++x)
            bmp.setPixel(x, y, Color(255 - x * 10, 255 - y * 10, 200));

    // Template is a copy of that 3x3 block
    Bitmap tpl(3, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            tpl.setPixel(x, y, bmp.getPixel(10 + x, 10 + y));

    Point bestPos;
    double score = PatternMatcher::templateMatch(bmp, tpl, bestPos, Rect{0, 0, 20, 20});
    EXPECT_EQ(bestPos.x, 10);
    EXPECT_EQ(bestPos.y, 10);
}

// ========== PatternMatcher::matchEdge tests ==========

TEST(PatternMatcherTest, MatchEdgeHorizontalEdgeDetected) {
    Bitmap bmp(10, 10);
    // Top half black, bottom half white => horizontal edge between rows 4 and 5
    for (int y = 5; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            bmp.setPixel(x, y, Color(255, 255, 255));

    // horizontal=false scans vertically (detects horizontal edges)
    auto result = PatternMatcher::matchEdge(bmp, Rect{0, 0, 10, 10}, false, 50);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->y, 4);  // Edge detected between row 4 and 5
}

TEST(PatternMatcherTest, MatchEdgeVerticalEdgeDetected) {
    Bitmap bmp(10, 10);
    // Left half black, right half white => vertical edge between columns 4 and 5
    for (int y = 0; y < 10; ++y)
        for (int x = 5; x < 10; ++x)
            bmp.setPixel(x, y, Color(255, 255, 255));

    // horizontal=true scans horizontally (detects vertical edges)
    auto result = PatternMatcher::matchEdge(bmp, Rect{0, 0, 10, 10}, true, 50);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->x, 4);
}

TEST(PatternMatcherTest, MatchEdgeNoEdge) {
    Bitmap bmp(10, 10);
    // Uniform color => no edge
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            bmp.setPixel(x, y, Color(100, 100, 100));

    auto result = PatternMatcher::matchEdge(bmp, Rect{0, 0, 10, 10}, true, 50);
    EXPECT_FALSE(result.has_value());
}

TEST(PatternMatcherTest, MatchEdgeEmptyRegionUsesFullBitmap) {
    Bitmap bmp(10, 10);
    for (int y = 5; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            bmp.setPixel(x, y, Color(255, 255, 255));

    // horizontal=false to detect horizontal edges
    auto result = PatternMatcher::matchEdge(bmp, Rect{}, false, 50);
    ASSERT_TRUE(result.has_value());
}

TEST(PatternMatcherTest, MatchEdgeVerticalNoEdge) {
    Bitmap bmp(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            bmp.setPixel(x, y, Color(100, 100, 100));

    auto result = PatternMatcher::matchEdge(bmp, Rect{0, 0, 10, 10}, false, 50);
    EXPECT_FALSE(result.has_value());
}

// ========== Confidence value tests ==========

TEST(ImageAnalyzerTest, FindColorConfidenceNotPerfect) {
    Bitmap bmp(10, 10);
    Color target(100, 100, 100);
    Color actual(110, 110, 110);  // distance = 300
    bmp.setPixel(0, 0, actual);

    ImageAnalyzer analyzer;
    auto result = analyzer.findColor(bmp, target, Rect{0, 0, 10, 10}, 20);
    ASSERT_TRUE(result.has_value());
    EXPECT_LT(result->confidence, 1.0);
    EXPECT_GT(result->confidence, 0.0);
    EXPECT_GT(result->distance, 0);
}

// ========== Edge case: single pixel bitmap ==========

TEST(ImageAnalyzerTest, FindColorSinglePixelBitmap) {
    Bitmap bmp(1, 1);
    Color c(42, 42, 42);
    bmp.setPixel(0, 0, c);

    ImageAnalyzer analyzer;
    auto result = analyzer.findColor(bmp, c, Rect{0, 0, 1, 1}, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->position.x, 0);
    EXPECT_EQ(result->position.y, 0);
}

TEST(ImageAnalyzerTest, FindColorsSinglePixelBitmap) {
    Bitmap bmp(1, 1);
    Color c(42, 42, 42);
    bmp.setPixel(0, 0, c);

    ImageAnalyzer analyzer;
    auto results = analyzer.findColors(bmp, c, Rect{0, 0, 1, 1}, 0, 0);
    EXPECT_EQ(results.size(), 1u);
}

// ========== PatternMatcher with 3-color pattern ==========

TEST(PatternMatcherTest, MatchColorPatternThreeColors) {
    Bitmap bmp(30, 10);
    Color r(255, 0, 0);
    Color g(0, 255, 0);
    Color b(0, 0, 255);

    // Place pattern at x=10
    bmp.setPixel(10, 5, r);
    bmp.setPixel(15, 5, g);
    bmp.setPixel(20, 5, b);

    std::vector<Color> colors = {r, g, b};
    std::vector<Point> offsets = {Point{0, 0}, Point{5, 0}, Point{10, 0}};
    auto result = PatternMatcher::matchColorPattern(bmp, colors, offsets, Rect{0, 0, 30, 10}, 0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->x, 10);
    EXPECT_EQ(result->y, 5);
}

// ========== templateMatch with uniform black bitmap ==========

TEST(PatternMatcherTest, TemplateMatchBlackOnBlack) {
    Bitmap bmp(8, 8);
    Bitmap tpl(3, 3);
    // All zero (black)

    Point bestPos;
    double score = PatternMatcher::templateMatch(bmp, tpl, bestPos, Rect{0, 0, 8, 8});
    // Both all-zero => denominator is 0 => score = 0
    EXPECT_DOUBLE_EQ(score, 0.0);
}
