#include <gtest/gtest.h>
#include "wingman/smart_trigger.hpp"

using namespace wingman;

// ========== SmartTrigger 基础测试 ==========

TEST(SmartTriggerTest, ConstructionAndName) {
    SmartTrigger t("MyTrigger");
    EXPECT_EQ(t.getName(), "MyTrigger");
}

TEST(SmartTriggerTest, IsRunningFalseInitially) {
    SmartTrigger t("test");
    EXPECT_FALSE(t.isRunning());
}

TEST(SmartTriggerTest, TriggerCountStartsAtZero) {
    SmartTrigger t("test");
    EXPECT_EQ(t.getTriggerCount(), 0);
}

TEST(SmartTriggerTest, ResetTriggerCount) {
    SmartTrigger t("test");
    // triggerCount starts at 0; reset should keep it at 0
    t.resetTriggerCount();
    EXPECT_EQ(t.getTriggerCount(), 0);
}

TEST(SmartTriggerTest, AddConditionDoesNotCrash) {
    SmartTrigger t("test");
    TriggerCondition cond;
    cond.type = TriggerConditionType::COLOR_FOUND;
    EXPECT_NO_THROW(t.addCondition(cond));
}

TEST(SmartTriggerTest, AddActionDoesNotCrash) {
    SmartTrigger t("test");
    TriggerAction action;
    action.type = TriggerActionType::LOG;
    action.logMessage = "hello";
    EXPECT_NO_THROW(t.addAction(action));
}

TEST(SmartTriggerTest, SetCheckIntervalAndMaxTriggers) {
    SmartTrigger t("test");
    EXPECT_NO_THROW(t.setCheckInterval(500));
    EXPECT_NO_THROW(t.setMaxTriggers(10));
}

TEST(SmartTriggerTest, StartFailsWithNoConditions) {
    SmartTrigger t("test");
    // start() returns false when conditions_ is empty
    EXPECT_FALSE(t.start());
    EXPECT_FALSE(t.isRunning());
}

TEST(SmartTriggerTest, StartStopImmediately) {
    SmartTrigger t("test");
    // Add a condition so start() can proceed (watchLoop will call checkConditions
    // which needs Vision — it may crash, but stop() should still work)
    TriggerCondition cond;
    cond.type = TriggerConditionType::COLOR_FOUND;
    t.addCondition(cond);

    // start() will launch a thread that calls checkConditions -> Vision::findColor
    // This may fail in a test environment, so we test via EXPECT_NO_THROW
    EXPECT_NO_THROW(t.start());
    // Immediately stop to clean up
    EXPECT_NO_THROW(t.stop());
    EXPECT_FALSE(t.isRunning());
}

TEST(SmartTriggerTest, DestructorCallsStop) {
    // Ensure no crash or hang when SmartTrigger is destroyed
    EXPECT_NO_THROW({
        SmartTrigger t("dtor_test");
        TriggerCondition cond;
        cond.type = TriggerConditionType::COLOR_FOUND;
        t.addCondition(cond);
        t.start();
        // destructor should call stop() internally
    });
}

// ========== SmartTriggerManager 测试 ==========

TEST(SmartTriggerManagerTest, InstanceReturnsSingleton) {
    auto& a = SmartTriggerManager::instance();
    auto& b = SmartTriggerManager::instance();
    EXPECT_EQ(&a, &b);
}

TEST(SmartTriggerManagerTest, CreateAndGetTrigger) {
    auto& mgr = SmartTriggerManager::instance();

    auto created = mgr.createTrigger("test_create");
    ASSERT_NE(created, nullptr);
    EXPECT_EQ(created->getName(), "test_create");

    auto retrieved = mgr.getTrigger("test_create");
    EXPECT_EQ(retrieved, created);

    // Cleanup
    mgr.removeTrigger("test_create");
}

TEST(SmartTriggerManagerTest, GetNonExistentTriggerReturnsNull) {
    auto& mgr = SmartTriggerManager::instance();
    auto result = mgr.getTrigger("non_existent_trigger_xyz");
    EXPECT_EQ(result, nullptr);
}

TEST(SmartTriggerManagerTest, RemoveTriggerDoesNotCrash) {
    auto& mgr = SmartTriggerManager::instance();
    // Removing a non-existent trigger should be safe
    EXPECT_NO_THROW(mgr.removeTrigger("no_such_trigger"));
}

TEST(SmartTriggerManagerTest, CreateDuplicateTriggerReturnsExisting) {
    auto& mgr = SmartTriggerManager::instance();

    auto first = mgr.createTrigger("dup_test");
    auto second = mgr.createTrigger("dup_test");
    EXPECT_EQ(first, second);

    mgr.removeTrigger("dup_test");
}

TEST(SmartTriggerManagerTest, GetAllTriggers) {
    auto& mgr = SmartTriggerManager::instance();

    // Cleanup any leftovers first
    mgr.removeTrigger("all_a");
    mgr.removeTrigger("all_b");

    mgr.createTrigger("all_a");
    mgr.createTrigger("all_b");

    auto all = mgr.getAllTriggers();
    EXPECT_EQ(all.size(), 2u);

    mgr.removeTrigger("all_a");
    mgr.removeTrigger("all_b");

    all = mgr.getAllTriggers();
    EXPECT_TRUE(all.empty());
}

TEST(SmartTriggerManagerTest, StartAllStopAllWithNoTriggers) {
    auto& mgr = SmartTriggerManager::instance();
    // Should be safe even with no triggers
    EXPECT_NO_THROW(mgr.startAll());
    EXPECT_NO_THROW(mgr.stopAll());
}
