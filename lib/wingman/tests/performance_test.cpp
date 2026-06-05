#include <gtest/gtest.h>
#include "wingman/performance.hpp"

#ifdef WINGMAN_ENABLE_VISION
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

#include <thread>
#include <filesystem>

using namespace wingman;

// Tests prefixed with "Perf" to avoid conflicts with extra_modules_test.cpp

// ========== PerformanceConfig advanced ==========

TEST(PerfConfigTest, FieldModification) {
    PerformanceConfig cfg;
    cfg.enableImageCache = false;
    cfg.maxCacheSize = 100;
    cfg.enableParallelProcessing = false;
    cfg.numThreads = 4;
    cfg.useColorReduction = true;
    cfg.colorReductionBits = 3;
    cfg.useImagePyramids = false;
    cfg.minPyramidLevel = 4;

    EXPECT_FALSE(cfg.enableImageCache);
    EXPECT_EQ(cfg.maxCacheSize, 100u);
    EXPECT_FALSE(cfg.enableParallelProcessing);
    EXPECT_EQ(cfg.numThreads, 4);
    EXPECT_TRUE(cfg.useColorReduction);
    EXPECT_EQ(cfg.colorReductionBits, 3);
    EXPECT_FALSE(cfg.useImagePyramids);
    EXPECT_EQ(cfg.minPyramidLevel, 4);
}

TEST(PerfConfigTest, CopyPreservesAllFields) {
    PerformanceConfig cfg;
    cfg.enableImageCache = false;
    cfg.maxCacheSize = 200;
    cfg.enableParallelProcessing = false;
    cfg.numThreads = 8;
    cfg.useColorReduction = true;
    cfg.colorReductionBits = 4;
    cfg.useImagePyramids = false;
    cfg.minPyramidLevel = 3;

    PerformanceConfig copy = cfg;
    EXPECT_FALSE(copy.enableImageCache);
    EXPECT_EQ(copy.maxCacheSize, 200u);
    EXPECT_FALSE(copy.enableParallelProcessing);
    EXPECT_EQ(copy.numThreads, 8);
    EXPECT_TRUE(copy.useColorReduction);
    EXPECT_EQ(copy.colorReductionBits, 4);
    EXPECT_FALSE(copy.useImagePyramids);
    EXPECT_EQ(copy.minPyramidLevel, 3);
}

// ========== CachedImage advanced ==========

TEST(PerfCachedImageTest, FieldAssignment) {
    CachedImage img;
    img.path = "test.png";
    img.accessCount = 5;
    img.lastAccess = 12345;
    EXPECT_EQ(img.path, "test.png");
    EXPECT_EQ(img.accessCount, 5u);
    EXPECT_EQ(img.lastAccess, 12345u);
}

// ========== PerformanceManager advanced ==========

TEST(PerfManagerTest, SetConfigWithZeroThreads) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.numThreads = 0;
    cfg.enableParallelProcessing = true;
    EXPECT_NO_THROW(mgr.setConfig(cfg));

    mgr.setConfig(origCfg);
}

TEST(PerfManagerTest, SetConfigWithParallelDisabled) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableParallelProcessing = false;
    cfg.numThreads = 2;
    EXPECT_NO_THROW(mgr.setConfig(cfg));

    mgr.setConfig(origCfg);
}

TEST(PerfManagerTest, ResetStatsClearsAll) {
    auto& mgr = PerformanceManager::instance();
    mgr.resetStats();
    auto stats = mgr.getStats();
    EXPECT_EQ(stats.totalCaptures, 0u);
    EXPECT_EQ(stats.totalColorSearches, 0u);
    EXPECT_EQ(stats.totalImageSearches, 0u);
    EXPECT_EQ(stats.cacheHits, 0u);
    EXPECT_EQ(stats.cacheMisses, 0u);
}

TEST(PerfManagerTest, CacheStatsAfterClear) {
    auto& mgr = PerformanceManager::instance();
    mgr.clearCache();
    EXPECT_EQ(mgr.getCacheSize(), 0u);
}

TEST(PerfManagerTest, PreloadNonexistentIncrementsMisses) {
    auto& mgr = PerformanceManager::instance();
    mgr.clearCache();
    mgr.resetStats();
    mgr.preloadImage("/nonexistent/path/image.png");
    EXPECT_EQ(mgr.getCacheMisses(), 1u);
    EXPECT_EQ(mgr.getCacheSize(), 0u);
}

TEST(PerfManagerTest, EvictExpiredOnEmptyCache) {
    auto& mgr = PerformanceManager::instance();
    mgr.clearCache();
    EXPECT_NO_THROW(mgr.evictExpired());
}

TEST(PerfManagerTest, FindMultipleImagesEmptyList) {
    auto& mgr = PerformanceManager::instance();
    auto results = mgr.findMultipleImages({}, Rect(0, 0, 100, 100), 0.8);
    EXPECT_TRUE(results.empty());
}

TEST(PerfManagerTest, ParallelFindColorsReturnsEmpty) {
    auto& mgr = PerformanceManager::instance();
    auto results = mgr.parallelFindColors(Color(255, 0, 0), Rect(0, 0, 10, 10), 10, 1);
    EXPECT_TRUE(results.empty());
}

TEST(PerfManagerTest, ParallelFindMultipleColorsReturnsResults) {
    auto& mgr = PerformanceManager::instance();
    std::vector<Color> colors = {Color(255, 0, 0), Color(0, 255, 0)};
    auto results = mgr.parallelFindMultipleColors(colors, Rect(0, 0, 10, 10), 10, 1);
    // May or may not find colors depending on screen content, but should not crash
    EXPECT_NO_THROW(mgr.parallelFindMultipleColors(colors, Rect(0, 0, 10, 10), 10, 1));
}

TEST(PerfManagerTest, FastFindImageNonexistentReturnsFalse) {
    auto& mgr = PerformanceManager::instance();
    Point result;
    EXPECT_FALSE(mgr.fastFindImage("/nonexistent/image.png", Rect(0, 0, 100, 100), 0.8, result));
}

// ========== PerformanceStats advanced ==========

TEST(PerfStatsTest, AllFieldsAccessible) {
    PerformanceManager::Stats stats;
    stats.totalCaptures = 10;
    stats.totalColorSearches = 20;
    stats.totalImageSearches = 30;
    stats.cacheHits = 40;
    stats.cacheMisses = 50;
    stats.avgCaptureTime = 1.5;
    stats.avgColorSearchTime = 2.5;
    stats.avgImageSearchTime = 3.5;
    EXPECT_EQ(stats.totalCaptures, 10u);
    EXPECT_EQ(stats.totalColorSearches, 20u);
    EXPECT_EQ(stats.totalImageSearches, 30u);
    EXPECT_EQ(stats.cacheHits, 40u);
    EXPECT_EQ(stats.cacheMisses, 50u);
    EXPECT_DOUBLE_EQ(stats.avgCaptureTime, 1.5);
    EXPECT_DOUBLE_EQ(stats.avgColorSearchTime, 2.5);
    EXPECT_DOUBLE_EQ(stats.avgImageSearchTime, 3.5);
}

// ========== Image Cache Hit Path ==========

#ifdef WINGMAN_ENABLE_VISION

TEST(PerfManagerTest, GetCachedImageCacheHit) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.maxCacheSize = 10;
    mgr.setConfig(cfg);
    mgr.clearCache();
    mgr.resetStats();

    // Create a temporary test image
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_perf_test";
    std::filesystem::create_directories(tempDir);
    std::string imgPath = (tempDir / "test_cache.png").string();

    cv::Mat testImg(32, 32, CV_8UC3, cv::Scalar(100, 150, 200));
    cv::imwrite(imgPath, testImg);

    // First load: cache miss
    auto img1 = mgr.getCachedImage(imgPath);
    EXPECT_FALSE(img1.empty());
    EXPECT_EQ(mgr.getCacheMisses(), 1u);
    EXPECT_EQ(mgr.getCacheSize(), 1u);

    // Second load: cache hit
    auto img2 = mgr.getCachedImage(imgPath);
    EXPECT_FALSE(img2.empty());
    EXPECT_EQ(mgr.getCacheHits(), 1u);
    EXPECT_EQ(mgr.getCacheSize(), 1u);

    // Cleanup
    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(tempDir);
}

TEST(PerfManagerTest, GetCachedImageDisabledCache) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = false;
    mgr.setConfig(cfg);
    mgr.resetStats();

    // Create a temporary test image
    auto tempDir = std::filesystem::temp_directory_path() / "wingman_perf_test2";
    std::filesystem::create_directories(tempDir);
    std::string imgPath = (tempDir / "test_nocache.png").string();

    cv::Mat testImg(16, 16, CV_8UC3, cv::Scalar(50, 100, 150));
    cv::imwrite(imgPath, testImg);

    auto img = mgr.getCachedImage(imgPath);
    EXPECT_FALSE(img.empty());
    EXPECT_EQ(mgr.getCacheMisses(), 1u);
    EXPECT_EQ(mgr.getCacheSize(), 0u);  // Not cached when disabled

    mgr.setConfig(origCfg);
    std::filesystem::remove_all(tempDir);
}

TEST(PerfManagerTest, GetCachedImageEviction) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.maxCacheSize = 2;
    mgr.setConfig(cfg);
    mgr.clearCache();

    auto tempDir = std::filesystem::temp_directory_path() / "wingman_perf_test3";
    std::filesystem::create_directories(tempDir);

    // Create 3 images to exceed cache size
    for (int i = 0; i < 3; ++i) {
        std::string path = (tempDir / ("evict_" + std::to_string(i) + ".png")).string();
        cv::Mat img(16, 16, CV_8UC3, cv::Scalar(i * 50, 100, 150));
        cv::imwrite(path, img);
        mgr.getCachedImage(path);
    }

    // Cache should have evicted to stay at or below maxCacheSize
    EXPECT_LE(mgr.getCacheSize(), 2u);

    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(tempDir);
}

TEST(PerfManagerTest, PreloadImageCachesSuccessfully) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.maxCacheSize = 10;
    mgr.setConfig(cfg);
    mgr.clearCache();

    auto tempDir = std::filesystem::temp_directory_path() / "wingman_perf_test4";
    std::filesystem::create_directories(tempDir);
    std::string imgPath = (tempDir / "preload.png").string();

    cv::Mat testImg(16, 16, CV_8UC3, cv::Scalar(200, 100, 50));
    cv::imwrite(imgPath, testImg);

    mgr.preloadImage(imgPath);
    EXPECT_EQ(mgr.getCacheSize(), 1u);

    // Second preload should hit cache
    mgr.preloadImage(imgPath);
    EXPECT_EQ(mgr.getCacheSize(), 1u);

    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(tempDir);
}

// ========== FastFindImage with real image ==========

TEST(PerfManagerTest, FastFindImageWithRealImage) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.maxCacheSize = 10;
    mgr.setConfig(cfg);
    mgr.clearCache();

    auto tempDir = std::filesystem::temp_directory_path() / "wingman_perf_test5";
    std::filesystem::create_directories(tempDir);
    std::string imgPath = (tempDir / "find_test.png").string();

    // Create a distinctive image
    cv::Mat testImg(32, 32, CV_8UC3, cv::Scalar(200, 100, 50));
    cv::imwrite(imgPath, testImg);

    Point result;
    // Search for image in a region - may or may not find it on screen
    EXPECT_NO_THROW(mgr.fastFindImage(imgPath, Rect(0, 0, 100, 100), 0.8, result));

    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(tempDir);
}

TEST(PerfManagerTest, FindMultipleImagesWithRealImages) {
    auto& mgr = PerformanceManager::instance();
    auto origCfg = mgr.getConfig();

    PerformanceConfig cfg;
    cfg.enableImageCache = true;
    cfg.maxCacheSize = 10;
    mgr.setConfig(cfg);
    mgr.clearCache();

    auto tempDir = std::filesystem::temp_directory_path() / "wingman_perf_test6";
    std::filesystem::create_directories(tempDir);

    // Create multiple test images
    std::vector<std::string> paths;
    for (int i = 0; i < 3; ++i) {
        std::string path = (tempDir / ("multi_" + std::to_string(i) + ".png")).string();
        cv::Mat img(16, 16, CV_8UC3, cv::Scalar(i * 80, 100, 50));
        cv::imwrite(path, img);
        paths.push_back(path);
    }

    auto results = mgr.findMultipleImages(paths, Rect(0, 0, 200, 200), 0.8);
    // Should not crash regardless of whether images are found on screen
    EXPECT_NO_THROW(mgr.findMultipleImages(paths, Rect(0, 0, 200, 200), 0.8));

    mgr.clearCache();
    mgr.setConfig(origCfg);
    std::filesystem::remove_all(tempDir);
}

#endif // WINGMAN_ENABLE_VISION
