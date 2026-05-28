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

// ========== 多条件与多动作 ==========

TEST(SmartTriggerTest, MultipleConditionsAdded) {
    SmartTrigger t("multi_cond");
    TriggerCondition c1;
    c1.type = TriggerConditionType::COLOR_FOUND;
    TriggerCondition c2;
    c2.type = TriggerConditionType::IMAGE_FOUND;
    c2.templatePath = "template.png";
    TriggerCondition c3;
    c3.type = TriggerConditionType::TEXT_FOUND;
    c3.targetText = "hello";

    EXPECT_NO_THROW(t.addCondition(c1));
    EXPECT_NO_THROW(t.addCondition(c2));
    EXPECT_NO_THROW(t.addCondition(c3));

    // Should still fail to start because conditions depend on Vision/OCR
    // which may not be available, but the object is valid
    EXPECT_EQ(t.getName(), "multi_cond");
    EXPECT_FALSE(t.isRunning());
}

TEST(SmartTriggerTest, MultipleActionsAdded) {
    SmartTrigger t("multi_action");
    TriggerAction a1;
    a1.type = TriggerActionType::LOG;
    a1.logMessage = "first";
    TriggerAction a2;
    a2.type = TriggerActionType::WAIT;
    a2.waitMs = 0;
    TriggerAction a3;
    a3.type = TriggerActionType::KEY_PRESS;
    a3.keyCode = 65;
    TriggerAction a4;
    a4.type = TriggerActionType::STOP;

    EXPECT_NO_THROW(t.addAction(a1));
    EXPECT_NO_THROW(t.addAction(a2));
    EXPECT_NO_THROW(t.addAction(a3));
    EXPECT_NO_THROW(t.addAction(a4));

    EXPECT_EQ(t.getName(), "multi_action");
    EXPECT_EQ(t.getTriggerCount(), 0);
}

// ========== TriggerCondition 字段 ==========

TEST(TriggerConditionTest, DefaultFieldValues) {
    TriggerCondition cond{};
    EXPECT_EQ(cond.tolerance, 0);
    EXPECT_EQ(cond.threshold, 0.8);
    EXPECT_FALSE(cond.hasPreviousColor);
    EXPECT_TRUE(cond.templatePath.empty());
    EXPECT_TRUE(cond.targetText.empty());
}

TEST(TriggerConditionTest, FieldAssignment) {
    TriggerCondition cond;
    cond.type = TriggerConditionType::COLOR_NOT_FOUND;
    cond.tolerance = 50;
    cond.templatePath = "/path/to/img.png";
    cond.targetText = "match_me";
    cond.threshold = 0.95;
    cond.searchRegion = {10, 20, 200, 300};
    cond.hasPreviousColor = true;

    EXPECT_EQ(cond.type, TriggerConditionType::COLOR_NOT_FOUND);
    EXPECT_EQ(cond.tolerance, 50);
    EXPECT_EQ(cond.templatePath, "/path/to/img.png");
    EXPECT_EQ(cond.targetText, "match_me");
    EXPECT_DOUBLE_EQ(cond.threshold, 0.95);
    EXPECT_EQ(cond.searchRegion.x, 10);
    EXPECT_EQ(cond.searchRegion.y, 20);
    EXPECT_TRUE(cond.hasPreviousColor);
}

TEST(TriggerConditionTest, AllConditionTypesAreValid) {
    // Ensure all enum values can be assigned without crashing
    TriggerCondition cond;
    EXPECT_NO_THROW(cond.type = TriggerConditionType::COLOR_FOUND);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::COLOR_NOT_FOUND);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::IMAGE_FOUND);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::IMAGE_NOT_FOUND);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::TEXT_FOUND);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::TEXT_NOT_FOUND);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::EDGE_DETECTED);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::COLOR_CHANGED);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::OCR_CONTAINS);
    EXPECT_NO_THROW(cond.type = TriggerConditionType::OCR_EQUALS);
}

// ========== TriggerAction 字段 ==========

TEST(TriggerActionTest, DefaultFieldValues) {
    TriggerAction action{};
    EXPECT_EQ(action.clickPosition.x, 0);
    EXPECT_EQ(action.clickPosition.y, 0);
    EXPECT_EQ(action.keyCode, 0);
    EXPECT_EQ(action.waitMs, 0);
    EXPECT_TRUE(action.luaScript.empty());
    EXPECT_TRUE(action.logMessage.empty());
    EXPECT_FALSE(action.callback);
}

TEST(TriggerActionTest, FieldAssignment) {
    TriggerAction action;
    action.type = TriggerActionType::CLICK;
    action.clickPosition = {100, 200};
    action.keyCode = 13;
    action.waitMs = 500;
    action.luaScript = "print('hi')";
    action.logMessage = "action fired";

    EXPECT_EQ(action.type, TriggerActionType::CLICK);
    EXPECT_EQ(action.clickPosition.x, 100);
    EXPECT_EQ(action.clickPosition.y, 200);
    EXPECT_EQ(action.keyCode, 13);
    EXPECT_EQ(action.waitMs, 500);
    EXPECT_EQ(action.luaScript, "print('hi')");
    EXPECT_EQ(action.logMessage, "action fired");
}

TEST(TriggerActionTest, CallbackInvocation) {
    TriggerAction action;
    action.type = TriggerActionType::CUSTOM_CALLBACK;
    bool wasCalled = false;
    action.callback = [&wasCalled]() { wasCalled = true; };
    ASSERT_TRUE(action.callback);
    action.callback();
    EXPECT_TRUE(wasCalled);
}

// ========== SmartTriggerManager 多触发器 ==========

TEST(SmartTriggerManagerTest, MultipleTriggersCount) {
    auto& mgr = SmartTriggerManager::instance();

    mgr.removeTrigger("count_a");
    mgr.removeTrigger("count_b");
    mgr.removeTrigger("count_c");

    mgr.createTrigger("count_a");
    mgr.createTrigger("count_b");
    mgr.createTrigger("count_c");

    auto all = mgr.getAllTriggers();
    EXPECT_EQ(all.size(), 3u);

    mgr.removeTrigger("count_a");
    mgr.removeTrigger("count_b");
    mgr.removeTrigger("count_c");

    all = mgr.getAllTriggers();
    EXPECT_TRUE(all.empty());
}

TEST(SmartTriggerManagerTest, RemoveTriggerDecreasesCount) {
    auto& mgr = SmartTriggerManager::instance();

    mgr.removeTrigger("dec_a");
    mgr.removeTrigger("dec_b");

    mgr.createTrigger("dec_a");
    mgr.createTrigger("dec_b");
    EXPECT_EQ(mgr.getAllTriggers().size(), 2u);

    mgr.removeTrigger("dec_a");
    EXPECT_EQ(mgr.getAllTriggers().size(), 1u);

    auto remaining = mgr.getTrigger("dec_b");
    ASSERT_NE(remaining, nullptr);
    EXPECT_EQ(remaining->getName(), "dec_b");

    mgr.removeTrigger("dec_b");
}

// ========== SmartTrigger setMaxTriggers 边界 ==========

TEST(SmartTriggerTest, SetMaxTriggersToOneDoesNotCrash) {
    SmartTrigger t("max_one");
    TriggerCondition cond;
    cond.type = TriggerConditionType::COLOR_FOUND;
    t.addCondition(cond);
    TriggerAction action;
    action.type = TriggerActionType::LOG;
    action.logMessage = "hit";
    t.addAction(action);
    t.setMaxTriggers(1);
    EXPECT_NO_THROW(t.start());
    EXPECT_NO_THROW(t.stop());
    EXPECT_FALSE(t.isRunning());
}

// ========== SmartTrigger 析构后清理 ==========

TEST(SmartTriggerTest, DestructorAfterStartCleansUp) {
    {
        SmartTrigger t("dtor_cleanup");
        TriggerCondition cond;
        cond.type = TriggerConditionType::COLOR_FOUND;
        t.addCondition(cond);
        t.start();
        // t is running; destructor should call stop() and join thread
    }
    // If we reach here without hanging, destructor cleanup worked
    SUCCEED();
}

// ========== SmartTriggerManager 重复创建返回同一实例 ==========

TEST(SmartTriggerManagerTest, CreateSameNameReturnsSameSharedPtr) {
    auto& mgr = SmartTriggerManager::instance();

    auto first = mgr.createTrigger("same_ptr_test");
    auto second = mgr.createTrigger("same_ptr_test");
    EXPECT_EQ(first.get(), second.get());
    EXPECT_EQ(first.use_count(), second.use_count());

    mgr.removeTrigger("same_ptr_test");
}

// ========== SmartTrigger stop 在未启动时安全 ==========

TEST(SmartTriggerTest, StopWhenNotRunningIsSafe) {
    SmartTrigger t("stop_idle");
    EXPECT_NO_THROW(t.stop());
    EXPECT_NO_THROW(t.stop());  // double stop
    EXPECT_FALSE(t.isRunning());
}
