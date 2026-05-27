#include <gtest/gtest.h>
#include "wingman/trigger.hpp"
#include "wingman/trigger_engine.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/performance.hpp"
#include "wingman/screen.hpp"

using namespace wingman;

// ========== TriggerType 枚举 ==========

TEST(TriggerEnumsTest, TriggerTypeValues) {
    EXPECT_EQ(static_cast<int>(TriggerType::ColorFound), 0);
    EXPECT_EQ(static_cast<int>(TriggerType::PixelChanged), 10);
}

TEST(TriggerEnumsTest, BasicTriggerActionValues) {
    EXPECT_NO_THROW(BasicTriggerAction a = BasicTriggerAction::RunScript);
    EXPECT_NO_THROW(BasicTriggerAction a = BasicTriggerAction::Log);
}

// ========== TriggerConfig 结构体 ==========

TEST(TriggerConfigTest, DefaultValues) {
    TriggerConfig cfg;
    EXPECT_TRUE(cfg.name.empty());
    EXPECT_TRUE(cfg.actions.empty());
    EXPECT_FALSE(cfg.oneShot);
    EXPECT_EQ(cfg.cooldown, 0);
    EXPECT_TRUE(cfg.enabled);
}

// ========== BasicTriggerCondition ==========

TEST(BasicTriggerConditionTest, DefaultValues) {
    BasicTriggerCondition cond;
    EXPECT_TRUE(cond.value.empty());
    EXPECT_EQ(cond.tolerance, 0);
    EXPECT_EQ(cond.interval, 0);
    EXPECT_TRUE(cond.enabled);
}

// ========== TriggerActionData ==========

TEST(TriggerActionDataTest, DefaultValues) {
    TriggerActionData data;
    EXPECT_TRUE(data.value.empty());
    EXPECT_EQ(data.x, 0);
    EXPECT_EQ(data.y, 0);
    EXPECT_EQ(data.delay, 0);
}

// ========== TriggerInstance ==========

TEST(TriggerInstanceTest, DefaultValues) {
    TriggerInstance inst;
    EXPECT_EQ(inst.id, 0u);
    EXPECT_EQ(inst.startTime, 0u);
    EXPECT_EQ(inst.lastTriggerTime, 0u);
    EXPECT_FALSE(inst.triggered);
}

// ========== PerformanceConfig ==========

TEST(PerformanceConfigTest, DefaultValues) {
    PerformanceConfig cfg;
    EXPECT_TRUE(cfg.enableImageCache);
    EXPECT_EQ(cfg.maxCacheSize, 50u);
    EXPECT_TRUE(cfg.enableParallelProcessing);
    EXPECT_EQ(cfg.numThreads, 0);
    EXPECT_FALSE(cfg.useColorReduction);
    EXPECT_EQ(cfg.colorReductionBits, 5);
    EXPECT_TRUE(cfg.useImagePyramids);
    EXPECT_EQ(cfg.minPyramidLevel, 2);
}

// ========== PerformanceManager::Stats ==========

TEST(PerformanceStatsTest, DefaultValues) {
    PerformanceManager::Stats stats;
    EXPECT_EQ(stats.totalCaptures, 0u);
    EXPECT_EQ(stats.totalColorSearches, 0u);
    EXPECT_EQ(stats.totalImageSearches, 0u);
    EXPECT_EQ(stats.cacheHits, 0u);
    EXPECT_EQ(stats.cacheMisses, 0u);
    EXPECT_DOUBLE_EQ(stats.avgCaptureTime, 0);
    EXPECT_DOUBLE_EQ(stats.avgColorSearchTime, 0);
    EXPECT_DOUBLE_EQ(stats.avgImageSearchTime, 0);
}

// ========== PerformanceManager 单例 ==========

TEST(PerformanceManagerTest, SingletonInstance) {
    auto& mgr1 = PerformanceManager::instance();
    auto& mgr2 = PerformanceManager::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST(PerformanceManagerTest, SetGetConfig) {
    auto& mgr = PerformanceManager::instance();
    PerformanceConfig cfg;
    cfg.enableImageCache = false;
    cfg.maxCacheSize = 100;
    mgr.setConfig(cfg);

    const auto& retrieved = mgr.getConfig();
    EXPECT_FALSE(retrieved.enableImageCache);
    EXPECT_EQ(retrieved.maxCacheSize, 100u);

    // Restore defaults
    mgr.setConfig(PerformanceConfig{});
}

TEST(PerformanceManagerTest, ClearCacheDoesNotCrash) {
    auto& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.clearCache());
}

TEST(PerformanceManagerTest, GetCacheStats) {
    auto& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.getCacheSize());
    EXPECT_NO_THROW(mgr.getCacheHits());
    EXPECT_NO_THROW(mgr.getCacheMisses());
}

TEST(PerformanceManagerTest, EvictExpiredDoesNotCrash) {
    auto& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.evictExpired());
}

TEST(PerformanceManagerTest, ResetStatsDoesNotCrash) {
    auto& mgr = PerformanceManager::instance();
    EXPECT_NO_THROW(mgr.resetStats());
    auto stats = mgr.getStats();
    EXPECT_EQ(stats.totalCaptures, 0u);
}

// ========== CachedImage ==========

TEST(CachedImageTest, DefaultValues) {
    CachedImage img;
    EXPECT_EQ(img.accessCount, 0u);
    EXPECT_EQ(img.lastAccess, 0u);
    EXPECT_TRUE(img.path.empty());
}

// ========== SmartTriggerCondition / Action 配置 ==========

TEST(SmartTriggerConditionTest, DefaultValues) {
    TriggerCondition cond;
    EXPECT_EQ(cond.tolerance, 0);
    EXPECT_DOUBLE_EQ(cond.threshold, 0.8);
    EXPECT_FALSE(cond.hasPreviousColor);
}

TEST(SmartTriggerActionTest, DefaultValues) {
    TriggerAction action;
    EXPECT_EQ(action.keyCode, 0);
    EXPECT_EQ(action.waitMs, 0);
    EXPECT_TRUE(action.luaScript.empty());
    EXPECT_TRUE(action.logMessage.empty());
}

// ========== SmartTrigger 基础 ==========

TEST(SmartTriggerTest, CreateAndGetName) {
    SmartTrigger trigger("test_trigger");
    EXPECT_EQ(trigger.getName(), "test_trigger");
    EXPECT_FALSE(trigger.isRunning());
    EXPECT_EQ(trigger.getTriggerCount(), 0);
}

TEST(SmartTriggerTest, SetCheckInterval) {
    SmartTrigger trigger("interval_test");
    EXPECT_NO_THROW(trigger.setCheckInterval(500));
}

TEST(SmartTriggerTest, SetMaxTriggers) {
    SmartTrigger trigger("max_test");
    EXPECT_NO_THROW(trigger.setMaxTriggers(10));
}

TEST(SmartTriggerTest, ResetTriggerCount) {
    SmartTrigger trigger("reset_test");
    EXPECT_NO_THROW(trigger.resetTriggerCount());
    EXPECT_EQ(trigger.getTriggerCount(), 0);
}

TEST(SmartTriggerTest, StopWithoutStartDoesNotCrash) {
    SmartTrigger trigger("stop_test");
    EXPECT_NO_THROW(trigger.stop());
}

// ========== SmartTriggerManager ==========

TEST(SmartTriggerManagerTest, CreateAndGetTrigger) {
    auto& mgr = SmartTriggerManager::instance();
    auto trigger = mgr.createTrigger("unique_test_trigger_xyz");
    ASSERT_NE(trigger, nullptr);
    EXPECT_EQ(trigger->getName(), "unique_test_trigger_xyz");

    auto retrieved = mgr.getTrigger("unique_test_trigger_xyz");
    EXPECT_EQ(retrieved, trigger);

    mgr.removeTrigger("unique_test_trigger_xyz");
    EXPECT_EQ(mgr.getTrigger("unique_test_trigger_xyz"), nullptr);
}

TEST(SmartTriggerManagerTest, GetNonexistentTrigger) {
    auto& mgr = SmartTriggerManager::instance();
    EXPECT_EQ(mgr.getTrigger("nonexistent_trigger_xyz"), nullptr);
}

TEST(SmartTriggerManagerTest, GetAllTriggers) {
    auto& mgr = SmartTriggerManager::instance();
    auto all = mgr.getAllTriggers();
    EXPECT_NO_THROW(mgr.getAllTriggers());
}

TEST(SmartTriggerManagerTest, StopAllDoesNotCrash) {
    auto& mgr = SmartTriggerManager::instance();
    EXPECT_NO_THROW(mgr.stopAll());
}

// ========== TriggerEngine::Stats ==========

TEST(TriggerEngineStatsTest, DefaultValues) {
    TriggerEngine::Stats stats;
    EXPECT_EQ(stats.totalTriggers, 0u);
    EXPECT_EQ(stats.enabledTriggers, 0u);
    EXPECT_EQ(stats.totalTriggered, 0u);
}
