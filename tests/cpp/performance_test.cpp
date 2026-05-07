#include <gtest/gtest.h>
#include "wingman/performance.hpp"

using namespace wingman;

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// PerformanceConfig Tests
// ============================================================================

TEST_F(PerformanceTest, PerformanceConfigDefaults) {
    PerformanceConfig config;
    EXPECT_TRUE(config.enableImageCache);
    EXPECT_EQ(config.maxCacheSize, 50);
    EXPECT_TRUE(config.enableParallelProcessing);
    EXPECT_EQ(config.numThreads, 0); // 0 means auto
}

TEST_F(PerformanceTest, PerformanceConfigWithValues) {
    PerformanceConfig config;
    config.enableImageCache = false;
    config.maxCacheSize = 100;
    config.enableParallelProcessing = false;
    config.numThreads = 4;

    EXPECT_FALSE(config.enableImageCache);
    EXPECT_EQ(config.maxCacheSize, 100);
    EXPECT_FALSE(config.enableParallelProcessing);
    EXPECT_EQ(config.numThreads, 4);
}

// ============================================================================
// PerformanceManager Tests
// ============================================================================

TEST_F(PerformanceTest, GetInstance) {
    PerformanceManager& mgr1 = PerformanceManager::instance();
    PerformanceManager& mgr2 = PerformanceManager::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(PerformanceTest, DefaultConfig) {
    PerformanceManager& mgr = PerformanceManager::instance();
    PerformanceConfig config = mgr.getConfig();
    EXPECT_TRUE(config.enableImageCache);
    EXPECT_EQ(config.maxCacheSize, 50);
    EXPECT_TRUE(config.enableParallelProcessing);
}

TEST_F(PerformanceTest, SetConfig) {
    PerformanceManager& mgr = PerformanceManager::instance();
    PerformanceConfig config;
    config.maxCacheSize = 100;
    config.enableImageCache = false;
    config.numThreads = 2;
    mgr.setConfig(config);

    PerformanceConfig retrieved = mgr.getConfig();
    EXPECT_EQ(retrieved.maxCacheSize, 100);
    EXPECT_FALSE(retrieved.enableImageCache);
    EXPECT_EQ(retrieved.numThreads, 2);
}

TEST_F(PerformanceTest, GetStats) {
    PerformanceManager& mgr = PerformanceManager::instance();
    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_GE(stats.totalCaptures, 0);
    EXPECT_GE(stats.cacheHits, 0);
    EXPECT_GE(stats.cacheMisses, 0);
    EXPECT_GE(stats.totalCaptureTimeMs, 0);
}

TEST_F(PerformanceTest, ResetStats) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.resetStats();
    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_EQ(stats.totalCaptures, 0);
    EXPECT_EQ(stats.cacheHits, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
    EXPECT_EQ(stats.totalCaptureTimeMs, 0);
}

TEST_F(PerformanceTest, ClearCache) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.clearCache();
    // Should not crash
    SUCCEED();
}

TEST_F(PerformanceTest, RecordCapture) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.recordCapture(50.5);
    mgr.recordCapture(30.0);

    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_EQ(stats.totalCaptures, 2);
    EXPECT_GT(stats.totalCaptureTimeMs, 0);
}

TEST_F(PerformanceTest, RecordCacheHit) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.recordCacheHit();
    mgr.recordCacheHit();

    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_EQ(stats.cacheHits, 2);
}

TEST_F(PerformanceTest, RecordCacheMiss) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.recordCacheMiss();

    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_EQ(stats.cacheMisses, 1);
}

TEST_F(PerformanceTest, GetCacheHitRate) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.resetStats();

    // No activity
    double rate1 = mgr.getCacheHitRate();
    EXPECT_EQ(rate1, 0.0);

    // Some hits and misses
    mgr.recordCacheHit();
    mgr.recordCacheHit();
    mgr.recordCacheHit();
    mgr.recordCacheMiss();
    mgr.recordCacheMiss();

    double rate2 = mgr.getCacheHitRate();
    EXPECT_GT(rate2, 0.0);
    EXPECT_LE(rate2, 1.0);
}

TEST_F(PerformanceTest, GetAverageCaptureTime) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.resetStats();

    // No captures
    double avg1 = mgr.getAverageCaptureTime();
    EXPECT_EQ(avg1, 0.0);

    // Some captures
    mgr.recordCapture(100.0);
    mgr.recordCapture(200.0);

    double avg2 = mgr.getAverageCaptureTime();
    EXPECT_EQ(avg2, 150.0);
}

TEST_F(PerformanceTest, EnableDisableCache) {
    PerformanceManager& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.enableCache(true));
    EXPECT_NO_THROW(mgr.enableCache(false));
}

TEST_F(PerformanceTest, SetCacheSize) {
    PerformanceManager& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.setCacheSize(25));
    EXPECT_NO_THROW(mgr.setCacheSize(100));
}

TEST_F(PerformanceTest, SetNumThreads) {
    PerformanceManager& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.setNumThreads(2));
    EXPECT_NO_THROW(mgr.setNumThreads(0)); // Auto
}
