#include <gtest/gtest.h>
#include "wingman/vision.hpp"
#include "wingman/screen.hpp"  // Bitmap（合成模板/回读断言）
#include <atomic>
#include <cstdio>
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
//
// 正向命中路径由 platform_x11_test 的 ScreenFindImageLocatesDrawnPattern 覆盖
// （Xvfb 根窗口画唯一图案后模板匹配找回坐标——那里能保证屏幕内容非均匀：
// TM_CCOEFF_NORMED 对零方差模板结果未定义，黑屏直抓模板不可用）。本文件
// 跨平台（无 Xlib 依赖），断言「不命中」的优雅路径：不存在的模板路径与
// 合成模板（不存在于屏幕）在 vision 构建下走真 imread/matchTemplate，
// 无 vision 的 stub 构建下返回 not found——两模式断言一致。

static int nextTemplateId() {
    static std::atomic<int> counter{0};
    return counter.fetch_add(1);
}

// 构造非均匀位图存为模板文件（vision 构建 saveImage 真写 PNG；stub 构建
// 恒 false → 返回空串，调用方 GTEST_SKIP——负向路径用例对 stub 无意义）
static std::string makeSyntheticTemplate(int width, int height) {
    Bitmap bmp(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bmp.setPixel(x, y, Color(static_cast<uint8_t>((x * 7) % 256),
                                     static_cast<uint8_t>((y * 13) % 256), 128));
        }
    }
    const std::string path =
        "wingman_vision_synth_" + std::to_string(nextTemplateId()) + ".png";
    std::remove(path.c_str());
    if (!Vision::saveImage(path, bmp)) {
        return "";
    }
    return path;
}

TEST(VisionTest, FindImage) {
    // 模板路径不存在：imread 失败 → 优雅 not found（vision/stub 一致）
    EXPECT_FALSE(Vision::findImage("/nonexistent/wingman-vision-tpl.png", 0.8).found);

    // 合成模板不存在于屏幕：真匹配执行后必须不命中
    const std::string templatePath = makeSyntheticTemplate(32, 24);
    if (templatePath.empty()) {
        GTEST_SKIP() << "vision unavailable (stub build) — findImage not wired";
    }
    EXPECT_FALSE(Vision::findImage(templatePath, 0.8).found);
    std::remove(templatePath.c_str());
}

TEST(VisionTest, FindImageInRegion) {
    const Rect region(0, 0, 500, 500);
    EXPECT_FALSE(Vision::findImage("/nonexistent/wingman-vision-tpl.png", region, 0.8).found);

    const std::string templatePath = makeSyntheticTemplate(32, 24);
    if (templatePath.empty()) {
        GTEST_SKIP() << "vision unavailable (stub build) — findImage not wired";
    }
    const auto result = Vision::findImage(templatePath, region, 0.8);
    EXPECT_FALSE(result.found);
    std::remove(templatePath.c_str());
}

TEST(VisionTest, FindAllImages) {
    const auto none = Vision::findAllImages("/nonexistent/wingman-vision-tpl.png", 0.8);
    EXPECT_TRUE(none.empty());

    const std::string templatePath = makeSyntheticTemplate(32, 24);
    if (templatePath.empty()) {
        GTEST_SKIP() << "vision unavailable (stub build) — findImage not wired";
    }
    EXPECT_TRUE(Vision::findAllImages(templatePath, 0.8).empty());
    std::remove(templatePath.c_str());
}

TEST(VisionTest, WaitForImage) {
    // 短超时 + 不存在模板：必须及时返回 false（等待路径不挂死）
    EXPECT_FALSE(Vision::waitForImage("/nonexistent/wingman-vision-tpl.png", 100, 0.8));
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
    // saveImage 是纯文件操作（不抓屏，原「only available on Windows」skip
    // 理由不成立）：构造非均匀位图写出后 fromFile 回读逐像素断言
    Bitmap bmp(8, 6);
    bmp.setPixel(0, 0, Color(255, 0, 0));
    bmp.setPixel(7, 5, Color(0, 255, 0));
    const std::string path = "wingman_vision_save_test.png";
    std::remove(path.c_str());

    if (!Vision::saveImage(path, bmp)) {
        GTEST_SKIP() << "vision unavailable (stub build) — saveImage not wired";
    }
    auto loaded = Bitmap::fromFile(path);
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->getWidth(), 8);
    EXPECT_EQ(loaded->getHeight(), 6);
    EXPECT_EQ(loaded->getPixel(0, 0).r, 255);
    EXPECT_EQ(loaded->getPixel(7, 5).g, 255);
    std::remove(path.c_str());
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
