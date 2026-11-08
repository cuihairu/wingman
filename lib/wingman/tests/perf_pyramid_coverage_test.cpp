#include <gtest/gtest.h>
#include "wingman/performance.hpp"
#include "wingman/screen.hpp"

#include <filesystem>
#include <string>

using namespace wingman;

// PerformanceManager::fastFindImage 金字塔路径补测（2026-11-08 覆盖率收口）：
// buildPyramid/pyramidMatch 此前零覆盖——现有 performance_test 只用 32x32 模板，
// 不满足 `templ.cols > 50 && templ.rows > 50` 的金字塔条件（159 行），走的是普通
// matchTemplate 路径。本文件用 >50x50 模板（默认 useImagePyramids=true、
// minPyramidLevel=2）驱动：
//   1. 不匹配路径：合成噪声模板（屏幕上必然不存在）→ pyramidMatch 在顶层
//      maxVal<threshold 即返回 false（380-382 行）→ fastFindImage false（166 行）。
//   2. 命中路径：真实屏幕捕获裁剪 60x60 作模板（真实子图 → 每层相关系数≈1），
//      验证逐层预测 refinement 与顶层 result 落点（358-370/384-390 行）。
// 屏幕捕获失败或裁剪区域近乎单色（CCOEFF_NORMED 对常量区域退化）时 GTEST_SKIP
// （环境不可产生有效内容，非用例失败）。

#ifdef WINGMAN_ENABLE_VISION

namespace {

const std::string kTempDir = (std::filesystem::temp_directory_path() /
                              "wingman_pyramid_test").string();

std::string writeImage(const std::string& name, const cv::Mat& bgr) {
    std::filesystem::create_directories(kTempDir);
    const std::string path = (std::filesystem::path(kTempDir) / name).string();
    cv::imwrite(path, bgr);
    return path;
}

} // namespace

TEST(PerfPyramidCoverageTest, FastFindImagePyramidNoMatchReturnsFalse) {
    auto& mgr = PerformanceManager::instance();
    const PerformanceConfig origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.useImagePyramids = true;   // 默认即 true，显式钉住
    cfg.minPyramidLevel = 2;
    mgr.setConfig(cfg);
    mgr.clearCache();

    // 60x60 合成随机噪声模板：>50x50 触发金字塔路径；噪声不在屏幕上 → 无匹配
    cv::Mat noise(60, 60, CV_8UC3);
    cv::randu(noise, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    const std::string path = writeImage("pyramid_noise.png", noise);

    Point result;
    // 区域 100x100：模板小于区域（155 行通过），捕获成功则进入金字塔分支
    EXPECT_FALSE(mgr.fastFindImage(path, Rect(0, 0, 100, 100), 0.95, result));

    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(kTempDir);
}

TEST(PerfPyramidCoverageTest, FastFindImagePyramidHitRefinesResult) {
    auto& mgr = PerformanceManager::instance();
    const PerformanceConfig origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.useImagePyramids = true;
    cfg.minPyramidLevel = 2;
    mgr.setConfig(cfg);
    mgr.clearCache();

    // 捕获真实屏幕区域，从中裁剪 60x60 子图作模板（子图 → 每层相关系数≈1）。
    auto bitmap = Screen::capture(Rect(0, 0, 120, 120));
    if (!bitmap) {
        mgr.setConfig(origCfg);
        GTEST_SKIP() << "Screen::capture unavailable in this environment";
    }
    if (bitmap->getWidth() < 80 || bitmap->getHeight() < 80) {
        mgr.setConfig(origCfg);
        GTEST_SKIP() << "screen too small for a 60x60 crop";
    }

    // BGRA → BGR（fastFindImage 内部对屏幕做 BGRA2BGR，模板经 imread 已是 BGR）
    cv::Mat screenMat(bitmap->getHeight(), bitmap->getWidth(), CV_8UC4,
                      const_cast<uint8_t*>(bitmap->getData()));
    cv::Mat screenBGR;
    cv::cvtColor(screenMat, screenBGR, cv::COLOR_BGRA2BGR);
    const cv::Mat crop = screenBGR(cv::Rect(10, 10, 60, 60)).clone();

    // 常量区域 → CCOEFF_NORMED 退化（除零），跳过而非断言
    cv::Scalar mean, stddev;
    cv::meanStdDev(crop, mean, stddev);
    if (stddev[0] + stddev[1] + stddev[2] < 1.0) {
        mgr.setConfig(origCfg);
        GTEST_SKIP() << "screen crop is near-constant (CCOEFF_NORMED degenerate)";
    }

    const std::string path = writeImage("pyramid_hit.png", crop);

    // 模板是 (10,10) 处的真实子图 → 应在 (10,10) 命中（±2 容差）。两次捕获间屏幕
    // 可能漂移（光标/动画），重试至多 3 次再断言。
    bool found = false;
    Point result;
    for (int attempt = 0; attempt < 3 && !found; ++attempt) {
        found = mgr.fastFindImage(path, Rect(0, 0, 120, 120), 0.95, result);
    }
    ASSERT_TRUE(found) << "pyramid hit not found after 3 attempts";
    EXPECT_NEAR(result.x, 10, 2);
    EXPECT_NEAR(result.y, 10, 2);

    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(kTempDir);
}

#endif // WINGMAN_ENABLE_VISION
