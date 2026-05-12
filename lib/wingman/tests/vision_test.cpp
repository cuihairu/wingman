#include <gtest/gtest.h>
#include "wingman/vision.hpp"
#include <fstream>
#include <random>

using namespace wingman;

// 辅助函数：生成测试图像路径
static std::string getTestImagePath(const std::string& name) {
    return "test_images/" + name;
}

class VisionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== 颜色检测测试 ==========

TEST(VisionTest, FindColorExact) {
    // 测试精确查找颜色
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, region);
    // 结果取决于屏幕内容，只检查不崩溃
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
    // 结果取决于屏幕内容
    SUCCEED();
}

TEST(VisionTest, GetDominantColor) {
    Rect region(0, 0, 100, 100);

    Color dominant = Vision::getDominantColor(region);
    // 只检查不崩溃
    SUCCEED();
}

// ========== 图像匹配测试 ==========

TEST(VisionTest, FindImage) {
    std::string templatePath = getTestImagePath("template.png");

    // 检查文件是否存在
    std::ifstream f(templatePath);
    if (!f.good()) {
        // 文件不存在，跳过此测试
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

    // 短超时，用于测试函数调用
    bool found = Vision::waitForImage(templatePath, 100, 0.8);
    SUCCEED();
}

// ========== 形状检测测试 ==========

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

// ========== 图像处理测试 ==========

TEST(VisionTest, CaptureRegion) {
    Rect region(100, 100, 200, 200);
    std::string outputPath = "test_capture.png";

    bool success = Vision::captureRegion(region, outputPath);
    // 只检查不崩溃（需要 OpenCV 支持）
    SUCCEED();
}

TEST(VisionTest, SaveImage) {
    // 先捕获屏幕
    auto bitmap = Screen::capture();
    ASSERT_NE(bitmap, nullptr);

    std::string outputPath = "test_save.png";
    bool success = Vision::saveImage(outputPath, *bitmap);

    // 只检查不崩溃
    SUCCEED();
}

TEST(VisionTest, CompareImages) {
    // 如果测试图像不存在，跳过
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

// ========== 辅助函数测试 ==========

TEST(VisionTest, IsColorMatchExact) {
    Color c1(100, 100, 100);
    Color c2(100, 100, 100);

    EXPECT_TRUE(Vision::isColorMatch(c1, c2, 0));
}

TEST(VisionTest, IsColorMatchWithTolerance) {
    Color c1(100, 100, 100);
    Color c2(105, 105, 105);

    // 容差足够大
    EXPECT_TRUE(Vision::isColorMatch(c1, c2, 10));

    // 容差太小
    EXPECT_FALSE(Vision::isColorMatch(c1, c2, 3));
}

TEST(VisionTest, IsColorMatchDifferent) {
    Color c1(255, 0, 0);
    Color c2(0, 255, 0);

    EXPECT_FALSE(Vision::isColorMatch(c1, c2, 0));
    EXPECT_FALSE(Vision::isColorMatch(c1, c2, 50));
}

// ========== 边界条件测试 ==========

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
    // 只检查不崩溃
    SUCCEED();
}

TEST(VisionTest, LargeRegion) {
    Color red(255, 0, 0);
    Rect largeRegion(0, 0, 10000, 10000);

    auto result = Vision::findColor(red, 5, largeRegion);
    // 只检查不崩溃
    SUCCEED();
}

TEST(VisionTest, ZeroTolerance) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, 0, region);
    // 只检查不崩溃
    SUCCEED();
}

TEST(VisionTest, LargeTolerance) {
    Color red(255, 0, 0);
    Rect region(0, 0, 100, 100);

    auto result = Vision::findColor(red, 255, region);
    // 只检查不崩溃
    SUCCEED();
}
