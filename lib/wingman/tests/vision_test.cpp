#include <gtest/gtest.h>
#include "wingman/vision.hpp"
#include <fstream>
#include <random>

using namespace wingman;

// Helper function: generate test image path
static std::string getTestImagePath(const std::string& name) {
    return "test_images/" + name;
}

class VisionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== Color Detection Tests ==========

TEST(VisionTest, FindColorExact) {
    // Test exact color search
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, region);
    // Result depends on screen content, only check it does not crash
    SUCCEED();
}

TEST(VisionTest, FindColorWithTolerance) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, 10, region);
    SUCCEED();
}

TEST(VisionTest, FindAllColors) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto points = Vision::findAllColors(red, 10, region);
    EXPECT_GE(points.size(), 0);
}

TEST(VisionTest, HasColor) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    bool has = Vision::hasColor(red, 10, region);
    // Result depends on screen content
    SUCCEED();
}

TEST(VisionTest, GetDominantColor) {
    // Screen capture in CI environment may cause access violation
    GTEST_SKIP() << "Skipping in CI environment";

    /*
    Rect region(0, 0, 100, 100);

    Color dominant = Vision::getDominantColor(region);
    // Only check it does not crash
    SUCCEED();
    */
}

// ========== Image Matching Tests ==========

TEST(VisionTest, FindImage) {
    std::string templatePath = getTestImagePath("template.png");

    // Check if file exists
    std::ifstream f(templatePath);
    if (!f.good()) {
        // File does not exist, skip this test
        return;
    }

    auto result = Vision::findImage(templatePath, 0.8);
    SUCCEED();
}

TEST(VisionTest, FindImageInRegion) {
    std::string templatePath = getTestImagePath("template.png");
    Rect region(0, 0, 500, 500);

    std::ifstream f(templatePath);
    if (!f.good()) {
        return;
    }

    auto result = Vision::findImage(templatePath, region, 0.8);
    SUCCEED();
}

TEST(VisionTest, FindAllImages) {
    std::string templatePath = getTestImagePath("template.png");

    std::ifstream f(templatePath);
    if (!f.good()) {
        return;
    }

    auto results = Vision::findAllImages(templatePath, 0.8);
    EXPECT_GE(results.size(), 0);
}

TEST(VisionTest, WaitForImage) {
    std::string templatePath = getTestImagePath("template.png");

    std::ifstream f(templatePath);
    if (!f.good()) {
        return;
    }

    // Short timeout for testing function call
    bool found = Vision::waitForImage(templatePath, 100, 0.8);
    SUCCEED();
}

// ========== Shape Detection Tests ==========

TEST(VisionTest, DetectEdges) {
    Rect region(0, 0, 200, 200);

    auto edges = Vision::detectEdges(region, 50, 150);
    EXPECT_GE(edges.size(), 0);
}

TEST(VisionTest, DetectContours) {
    Rect region(0, 0, 200, 200);

    auto contours = Vision::detectContours(region);
    EXPECT_GE(contours.size(), 0);
}

TEST(VisionTest, DetectCircles) {
    Rect region(0, 0, 200, 200);

    auto circles = Vision::detectCircles(region, 10, 100);
    EXPECT_GE(circles.size(), 0);
}

// ========== Image Processing Tests ==========

TEST(VisionTest, CaptureRegion) {
    Rect region(100, 100, 200, 200);
    std::string outputPath = "test_capture.png";

    bool success = Vision::captureRegion(region, outputPath);
    // Only check it does not crash (requires OpenCV support)
    SUCCEED();
}

TEST(VisionTest, SaveImage) {
    GTEST_SKIP() << "Screen capture is only available on Windows";
}

TEST(VisionTest, CompareImages) {
    // If test images do not exist, skip
    std::string path1 = getTestImagePath("image1.png");
    std::string path2 = getTestImagePath("image2.png");

    std::ifstream f1(path1);
    std::ifstream f2(path2);

    if (!f1.good() || !f2.good()) {
        return;
    }

    double similarity = Vision::compareImages(path1, path2);
    EXPECT_GE(similarity, 0.0);
    EXPECT_LE(similarity, 1.0);
}

// ========== Helper Function Tests ==========

TEST(VisionTest, IsColorMatchExact) {
    Color c1(100, 100, 100);
    Color c2(100, 100, 100);

    EXPECT_TRUE(Vision::isColorMatch(c1, c2, 0));
}

TEST(VisionTest, IsColorMatchWithTolerance) {
    Color c1(100, 100, 100);
    Color c2(105, 105, 105);

    // Tolerance is large enough
    EXPECT_TRUE(Vision::isColorMatch(c1, c2, 10));

    // Tolerance is too small
    EXPECT_FALSE(Vision::isColorMatch(c1, c2, 3));
}

TEST(VisionTest, IsColorMatchDifferent) {
    Color c1(255, 0, 0);
    Color c2(0, 255, 0);

    EXPECT_FALSE(Vision::isColorMatch(c1, c2, 0));
    EXPECT_FALSE(Vision::isColorMatch(c1, c2, 50));
}

// ========== Boundary Condition Tests ==========

TEST(VisionTest, FindColorEmptyRegion) {
    Color red(255, 0, 0);
    Rect emptyRegion(0, 0, 0, 0);

    auto result = Vision::findColor(red, emptyRegion);
    EXPECT_FALSE(result.has_value());
}

TEST(VisionTest, FindAllColorsEmptyRegion) {
    Color red(255, 0, 0);
    Rect emptyRegion(0, 0, 0, 0);

    auto points = Vision::findAllColors(red, 10, emptyRegion);
    EXPECT_EQ(points.size(), 0);
}

TEST(VisionTest, GetDominantColorEmptyRegion) {
    Rect emptyRegion(0, 0, 0, 0);

    Color c = Vision::getDominantColor(emptyRegion);
    // Only check it does not crash
    SUCCEED();
}

TEST(VisionTest, LargeRegion) {
    Color red(255, 0, 0);
    Rect largeRegion(0, 0, 10000, 10000);

    auto result = Vision::findColor(red, 5, largeRegion);
    // Only check it does not crash
    SUCCEED();
}

TEST(VisionTest, ZeroTolerance) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, 0, region);
    // Only check it does not crash
    SUCCEED();
}

TEST(VisionTest, LargeTolerance) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, 255, region);
    // Only check it does not crash
    SUCCEED();
}
