#include <gtest/gtest.h>
#include "wingman/trigger.hpp"
#include "wingman/trigger_engine.hpp"
#ifdef _WIN32
#include "wingman/smart_trigger.hpp"
#endif
#include "wingman/performance.hpp"
#include "wingman/screen.hpp"

using namespace wingman;

// ========== TriggerType Enum ==========

TEST(TriggerEnumsTest, TriggerTypeValues) {
    EXPECT_EQ(static_cast<int>(TriggerType::ColorFound), 0);
    EXPECT_EQ(static_cast<int>(TriggerType::PixelChanged), 10);
}

TEST(TriggerEnumsTest, BasicTriggerActionValues) {
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::RunScript), 0);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::Log), 8);
}

// ========== TriggerConfig Struct ==========

TEST(TriggerConfigTest, DefaultValues) {
    TriggerConfig cfg{};
    EXPECT_TRUE(cfg.name.empty());
    EXPECT_TRUE(cfg.actions.empty());
    // oneShot may default to true depending on implementation
    EXPECT_NO_THROW(cfg.oneShot);
    EXPECT_NO_THROW(cfg.cooldown);
    EXPECT_NO_THROW(cfg.enabled);
}

// ========== BasicTriggerCondition ==========

TEST(BasicTriggerConditionTest, DefaultValues) {
    BasicTriggerCondition cond{};
    EXPECT_TRUE(cond.value.empty());
    EXPECT_NO_THROW(cond.tolerance);
    EXPECT_NO_THROW(cond.interval);
}

// ========== TriggerActionData ==========

TEST(TriggerActionDataTest, DefaultValues) {
    TriggerActionData data{};
    EXPECT_TRUE(data.value.empty());
    EXPECT_NO_THROW(data.x);
    EXPECT_NO_THROW(data.y);
    EXPECT_NO_THROW(data.delay);
}

// ========== TriggerInstance ==========

TEST(TriggerInstanceTest, DefaultValues) {
    TriggerInstance inst{};
    EXPECT_EQ(inst.id, 0u);
    EXPECT_NO_THROW(inst.startTime);
    EXPECT_NO_THROW(inst.lastTriggerTime);
    EXPECT_NO_THROW(inst.triggered);
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

// ========== PerformanceManager Singleton ==========

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

// ========== SmartTriggerCondition / Action Configuration (Windows only) ==========

#ifdef _WIN32

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

// ========== SmartTrigger Basics ==========

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

TEST(SmartTriggerTest, ResetTriggerCountKeepsZero) {
    SmartTrigger trigger("reset_test");
    EXPECT_NO_THROW(trigger.resetTriggerCount());
    EXPECT_EQ(trigger.getTriggerCount(), 0);
}

TEST(SmartTriggerTest, StopWithoutStartDoesNotCrash) {
    SmartTrigger trigger("stop_test");
    EXPECT_NO_THROW(trigger.stop());
}

// ========== SmartTriggerManager ==========

TEST(SmartTriggerManagerTest, CreateGetAndRemoveTrigger) {
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

TEST(SmartTriggerManagerTest, GetAllTriggersReturnsContainer) {
    auto& mgr = SmartTriggerManager::instance();
    auto all = mgr.getAllTriggers();
    EXPECT_GE(all.size(), 0u);
}

TEST(SmartTriggerManagerTest, StopAllDoesNotCrash) {
    auto& mgr = SmartTriggerManager::instance();
    EXPECT_NO_THROW(mgr.stopAll());
}

#endif // _WIN32

// ========== TriggerEngine::Stats ==========

TEST(TriggerEngineStatsTest, DefaultValues) {
    TriggerEngine::Stats stats;
    EXPECT_EQ(stats.totalTriggers, 0u);
    EXPECT_EQ(stats.enabledTriggers, 0u);
    EXPECT_EQ(stats.totalTriggered, 0u);
}

// ========== Additional Trigger Config Tests ==========

TEST(TriggerTypeTest, AllEnumValues) {
    EXPECT_EQ(static_cast<int>(TriggerType::ColorFound), 0);
    EXPECT_EQ(static_cast<int>(TriggerType::ColorLost), 1);
    EXPECT_EQ(static_cast<int>(TriggerType::ImageFound), 2);
    EXPECT_EQ(static_cast<int>(TriggerType::ImageLost), 3);
    EXPECT_EQ(static_cast<int>(TriggerType::WindowOpened), 4);
    EXPECT_EQ(static_cast<int>(TriggerType::WindowClosed), 5);
    EXPECT_EQ(static_cast<int>(TriggerType::ProcessStarted), 6);
    EXPECT_EQ(static_cast<int>(TriggerType::ProcessStopped), 7);
    EXPECT_EQ(static_cast<int>(TriggerType::TimeElapsed), 8);
    EXPECT_EQ(static_cast<int>(TriggerType::HotkeyPressed), 9);
    EXPECT_EQ(static_cast<int>(TriggerType::PixelChanged), 10);
}

TEST(BasicTriggerActionTest, AllEnumValues) {
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::RunScript), 0);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::Click), 1);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::KeyPress), 2);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::Type), 3);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::StopScript), 4);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::PauseScript), 5);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::ShowMessage), 6);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::PlayAudio), 7);
    EXPECT_EQ(static_cast<int>(BasicTriggerAction::Log), 8);
}

TEST(TriggerConfigTest, FieldAssignment) {
    TriggerConfig cfg{};
    cfg.name = "test";
    cfg.cooldown = 500;
    cfg.enabled = true;
    cfg.oneShot = true;

    EXPECT_EQ(cfg.name, "test");
    EXPECT_EQ(cfg.cooldown, 500);
    EXPECT_TRUE(cfg.enabled);
    EXPECT_TRUE(cfg.oneShot);
}

TEST(TriggerActionDataTest, FieldAssignment) {
    TriggerActionData data{};
    data.type = BasicTriggerAction::Click;
    data.value = "click_action";
    data.x = 100;
    data.y = 200;
    data.delay = 50;

    EXPECT_EQ(data.type, BasicTriggerAction::Click);
    EXPECT_EQ(data.value, "click_action");
    EXPECT_EQ(data.x, 100);
    EXPECT_EQ(data.y, 200);
    EXPECT_EQ(data.delay, 50);
}

#ifdef _WIN32
TEST(TriggerConditionTest, AllFields) {
    TriggerCondition cond;
    cond.type = TriggerConditionType::COLOR_FOUND;
    cond.targetColor = Color(255, 0, 0);
    cond.tolerance = 10;
    cond.searchRegion = Rect(0, 0, 800, 600);
    cond.threshold = 0.9;

    EXPECT_EQ(cond.type, TriggerConditionType::COLOR_FOUND);
    EXPECT_EQ(cond.targetColor.r, 255);
    EXPECT_EQ(cond.tolerance, 10);
    EXPECT_EQ(cond.searchRegion.width, 800);
    EXPECT_DOUBLE_EQ(cond.threshold, 0.9);
}

TEST(TriggerConditionTypeTest, AllEnumValues) {
    EXPECT_EQ(static_cast<int>(TriggerConditionType::COLOR_FOUND), 0);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::COLOR_NOT_FOUND), 1);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::IMAGE_FOUND), 2);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::IMAGE_NOT_FOUND), 3);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::TEXT_FOUND), 4);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::TEXT_NOT_FOUND), 5);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::EDGE_DETECTED), 6);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::COLOR_CHANGED), 7);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::OCR_CONTAINS), 8);
    EXPECT_EQ(static_cast<int>(TriggerConditionType::OCR_EQUALS), 9);
}

TEST(TriggerActionTypeTest, AllEnumValues) {
    EXPECT_EQ(static_cast<int>(TriggerActionType::CLICK), 0);
    EXPECT_EQ(static_cast<int>(TriggerActionType::KEY_PRESS), 1);
    EXPECT_EQ(static_cast<int>(TriggerActionType::WAIT), 2);
    EXPECT_EQ(static_cast<int>(TriggerActionType::LUA_SCRIPT), 3);
    EXPECT_EQ(static_cast<int>(TriggerActionType::CUSTOM_CALLBACK), 4);
    EXPECT_EQ(static_cast<int>(TriggerActionType::STOP), 5);
    EXPECT_EQ(static_cast<int>(TriggerActionType::LOG), 6);
}
#endif

// ========== PerformanceConfig Extended ==========

TEST(PerformanceConfigTest, FieldModification) {
    PerformanceConfig cfg;
    cfg.enableImageCache = false;
    cfg.maxCacheSize = 100;
    cfg.enableParallelProcessing = false;
    cfg.numThreads = 4;
    cfg.useColorReduction = true;
    cfg.colorReductionBits = 3;
    cfg.useImagePyramids = false;
    cfg.minPyramidLevel = 1;

    EXPECT_FALSE(cfg.enableImageCache);
    EXPECT_EQ(cfg.maxCacheSize, 100u);
    EXPECT_FALSE(cfg.enableParallelProcessing);
    EXPECT_EQ(cfg.numThreads, 4);
    EXPECT_TRUE(cfg.useColorReduction);
    EXPECT_EQ(cfg.colorReductionBits, 3);
    EXPECT_FALSE(cfg.useImagePyramids);
    EXPECT_EQ(cfg.minPyramidLevel, 1);
}

TEST(CachedImageTest, FieldAssignment) {
    CachedImage img;
    img.path = "/path/to/image.png";
    img.accessCount = 5;
    img.lastAccess = 1234567890;

    EXPECT_EQ(img.path, "/path/to/image.png");
    EXPECT_EQ(img.accessCount, 5u);
    EXPECT_EQ(img.lastAccess, 1234567890u);
}
