#include <gtest/gtest.h>
#include "wingman/performance.hpp"

using namespace wingman;

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// PerformanceManager Tests
// ============================================================================

TEST_F(PerformanceTest, GetInstance) {
    PerformanceManager& mgr = PerformanceManager::instance();
    SUCCEED();
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
    mgr.setConfig(config);

    PerformanceConfig retrieved = mgr.getConfig();
    EXPECT_EQ(retrieved.maxCacheSize, 100);
    EXPECT_FALSE(retrieved.enableImageCache);
}

TEST_F(PerformanceTest, GetStats) {
    PerformanceManager& mgr = PerformanceManager::instance();
    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_GE(stats.totalCaptures, 0);
    EXPECT_GE(stats.cacheHits, 0);
    EXPECT_GE(stats.cacheMisses, 0);
}

TEST_F(PerformanceTest, ResetStats) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.resetStats();
    PerformanceManager::Stats stats = mgr.getStats();
    EXPECT_EQ(stats.totalCaptures, 0);
    EXPECT_EQ(stats.cacheHits, 0);
    EXPECT_EQ(stats.cacheMisses, 0);
}

TEST_F(PerformanceTest, ClearCache) {
    PerformanceManager& mgr = PerformanceManager::instance();
    mgr.clearCache();
    SUCCEED();
}
