#include <gtest/gtest.h>
#include "wingman/smart_trigger.hpp"

using namespace wingman;

class SmartTriggerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// TriggerConditionType Tests
// ============================================================================

TEST_F(SmartTriggerTest, TriggerConditionTypeValues) {
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

// ============================================================================
// TriggerActionType Tests
// ============================================================================

TEST_F(SmartTriggerTest, TriggerActionTypeValues) {
    EXPECT_EQ(static_cast<int>(TriggerActionType::CLICK), 0);
    EXPECT_EQ(static_cast<int>(TriggerActionType::KEY_PRESS), 1);
    EXPECT_EQ(static_cast<int>(TriggerActionType::WAIT), 2);
    EXPECT_EQ(static_cast<int>(TriggerActionType::LUA_SCRIPT), 3);
    EXPECT_EQ(static_cast<int>(TriggerActionType::CUSTOM_CALLBACK), 4);
    EXPECT_EQ(static_cast<int>(TriggerActionType::STOP), 5);
    EXPECT_EQ(static_cast<int>(TriggerActionType::LOG), 6);
}

// ============================================================================
// TriggerCondition Tests
// ============================================================================

TEST_F(SmartTriggerTest, TriggerConditionDefaults) {
    TriggerCondition condition;
    condition.type = TriggerConditionType::COLOR_FOUND;

    EXPECT_EQ(condition.tolerance, 0);
    EXPECT_TRUE(condition.templatePath.empty());
    EXPECT_TRUE(condition.targetText.empty());
    EXPECT_EQ(condition.searchRegion.x, 0);
    EXPECT_EQ(condition.searchRegion.y, 0);
    EXPECT_EQ(condition.searchRegion.width, 0);
    EXPECT_EQ(condition.searchRegion.height, 0);
    EXPECT_EQ(condition.threshold, 0.8);
    EXPECT_FALSE(condition.hasPreviousColor);
}

TEST_F(SmartTriggerTest, TriggerConditionWithColor) {
    TriggerCondition condition;
    condition.type = TriggerConditionType::COLOR_FOUND;
    condition.targetColor = Color(255, 0, 0, 255);
    condition.tolerance = 10;

    EXPECT_EQ(condition.targetColor.r, 255);
    EXPECT_EQ(condition.targetColor.g, 0);
    EXPECT_EQ(condition.tolerance, 10);
}

TEST_F(SmartTriggerTest, TriggerConditionWithImage) {
    TriggerCondition condition;
    condition.type = TriggerConditionType::IMAGE_FOUND;
    condition.templatePath = "test_template.png";
    condition.threshold = 0.9;

    EXPECT_EQ(condition.templatePath, "test_template.png");
    EXPECT_EQ(condition.threshold, 0.9);
}

TEST_F(SmartTriggerTest, TriggerConditionWithText) {
    TriggerCondition condition;
    condition.type = TriggerConditionType::TEXT_FOUND;
    condition.targetText = "Hello World";

    EXPECT_EQ(condition.targetText, "Hello World");
}

TEST_F(SmartTriggerTest, TriggerConditionWithRegion) {
    TriggerCondition condition;
    condition.type = TriggerConditionType::COLOR_FOUND;
    condition.searchRegion = {100, 100, 200, 200};

    EXPECT_EQ(condition.searchRegion.x, 100);
    EXPECT_EQ(condition.searchRegion.y, 100);
    EXPECT_EQ(condition.searchRegion.width, 200);
    EXPECT_EQ(condition.searchRegion.height, 200);
}

// ============================================================================
// TriggerAction Tests
// ============================================================================

TEST_F(TriggerActionTest, TriggerActionDefaults) {
    TriggerAction action;
    EXPECT_EQ(action.clickPosition.x, 0);
    EXPECT_EQ(action.clickPosition.y, 0);
    EXPECT_EQ(action.keyCode, 0);
    EXPECT_EQ(action.waitMs, 0);
    EXPECT_TRUE(action.luaScript.empty());
    EXPECT_TRUE(action.logMessage.empty());
    EXPECT_FALSE(action.callback);
}

TEST_F(TriggerActionTest, TriggerActionClick) {
    TriggerAction action;
    action.type = TriggerActionType::CLICK;
    action.clickPosition = {150, 200};

    EXPECT_EQ(action.clickPosition.x, 150);
    EXPECT_EQ(action.clickPosition.y, 200);
}

TEST_F(TriggerActionTest, TriggerActionKeyPress) {
    TriggerAction action;
    action.type = TriggerActionType::KEY_PRESS;
    action.keyCode = 65; // 'A' key

    EXPECT_EQ(action.keyCode, 65);
}

TEST_F(TriggerActionTest, TriggerActionWait) {
    TriggerAction action;
    action.type = TriggerActionType::WAIT;
    action.waitMs = 1000;

    EXPECT_EQ(action.waitMs, 1000);
}

TEST_F(TriggerActionTest, TriggerActionLuaScript) {
    TriggerAction action;
    action.type = TriggerActionType::LUA_SCRIPT;
    action.luaScript = "print('Hello from Lua')";

    EXPECT_EQ(action.luaScript, "print('Hello from Lua')");
}

TEST_F(TriggerActionTest, TriggerActionLog) {
    TriggerAction action;
    action.type = TriggerActionType::LOG;
    action.logMessage = "Test log message";

    EXPECT_EQ(action.logMessage, "Test log message");
}

TEST_F(TriggerActionTest, TriggerActionCallback) {
    bool called = false;
    TriggerAction action;
    action.type = TriggerActionType::CUSTOM_CALLBACK;
    action.callback = [&called]() { called = true; };

    EXPECT_TRUE(action.callback);
    action.callback();
    EXPECT_TRUE(called);
}

// ============================================================================
// SmartTrigger Tests
// ============================================================================

TEST_F(SmartTriggerTest, SmartTriggerConstruction) {
    SmartTrigger trigger("test_trigger");
    EXPECT_EQ(trigger.getName(), "test_trigger");
    EXPECT_FALSE(trigger.isRunning());
    EXPECT_EQ(trigger.getTriggerCount(), 0);
}

TEST_F(SmartTriggerTest, SmartTriggerAddCondition) {
    SmartTrigger trigger("test_trigger");

    TriggerCondition condition;
    condition.type = TriggerConditionType::COLOR_FOUND;
    condition.targetColor = Color(255, 0, 0, 255);

    EXPECT_NO_THROW(trigger.addCondition(condition));
}

TEST_F(SmartTriggerTest, SmartTriggerAddAction) {
    SmartTrigger trigger("test_trigger");

    TriggerAction action;
    action.type = TriggerActionType::LOG;
    action.logMessage = "Test";

    EXPECT_NO_THROW(trigger.addAction(action));
}

TEST_F(SmartTriggerTest, SmartTriggerSetCheckInterval) {
    SmartTrigger trigger("test_trigger");
    EXPECT_NO_THROW(trigger.setCheckInterval(200));
}

TEST_F(SmartTriggerTest, SmartTriggerSetMaxTriggers) {
    SmartTrigger trigger("test_trigger");
    EXPECT_NO_THROW(trigger.setMaxTriggers(10));
}

TEST_F(SmartTriggerTest, SmartTriggerStartStop) {
    SmartTrigger trigger("test_trigger");
    EXPECT_NO_THROW(trigger.start());
    // Give it a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(trigger.stop());
}

TEST_F(SmartTriggerTest, SmartTriggerResetTriggerCount) {
    SmartTrigger trigger("test_trigger");
    trigger.resetTriggerCount();
    EXPECT_EQ(trigger.getTriggerCount(), 0);
}

// ============================================================================
// SmartTriggerManager Tests
// ============================================================================

TEST_F(SmartTriggerTest, SmartTriggerManagerInstance) {
    auto& manager1 = SmartTriggerManager::instance();
    auto& manager2 = SmartTriggerManager::instance();
    EXPECT_EQ(&manager1, &manager2);
}

TEST_F(SmartTriggerTest, SmartTriggerManagerCreateTrigger) {
    auto& manager = SmartTriggerManager::instance();
    auto trigger = manager.createTrigger("manager_test");
    EXPECT_NE(trigger, nullptr);
    EXPECT_EQ(trigger->getName(), "manager_test");
}

TEST_F(SmartTriggerTest, SmartTriggerManagerGetTrigger) {
    auto& manager = SmartTriggerManager::instance();
    manager.createTrigger("get_test");
    auto trigger = manager.getTrigger("get_test");
    EXPECT_NE(trigger, nullptr);

    auto missing = manager.getTrigger("nonexistent");
    EXPECT_EQ(missing, nullptr);
}

TEST_F(SmartTriggerTest, SmartTriggerManagerRemoveTrigger) {
    auto& manager = SmartTriggerManager::instance();
    manager.createTrigger("remove_test");
    EXPECT_NO_THROW(manager.removeTrigger("remove_test"));
    EXPECT_EQ(manager.getTrigger("remove_test"), nullptr);
}

TEST_F(SmartTriggerTest, SmartTriggerManagerGetAllTriggers) {
    auto& manager = SmartTriggerManager::instance();
    manager.createTrigger("trigger1");
    manager.createTrigger("trigger2");

    auto triggers = manager.getAllTriggers();
    EXPECT_GE(triggers.size(), 2);
}

TEST_F(SmartTriggerTest, SmartTriggerManagerStartStopAll) {
    auto& manager = SmartTriggerManager::instance();
    manager.createTrigger("start_stop_test");

    EXPECT_NO_THROW(manager.startAll());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(manager.stopAll());
}
