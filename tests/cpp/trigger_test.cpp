#include <gtest/gtest.h>
#include "wingman/trigger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace wingman;

class TriggerTest : public ::testing::Test {
protected:
    std::shared_ptr<spdlog::logger> logger;
    std::unique_ptr<TriggerManager> manager;

    void SetUp() override {
        // Use a unique logger name for each test to avoid conflicts
        std::string logger_name = "trigger_test_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        logger = spdlog::stdout_color_mt(logger_name);
        logger->set_level(spdlog::level::debug);
        manager = std::make_unique<TriggerManager>(logger);
    }

    void TearDown() override {
        manager.reset();
    }
};

// ============================================================================
// TriggerManager Tests
// ============================================================================

TEST_F(TriggerTest, CreateManager) {
    EXPECT_NE(manager, nullptr);
}

TEST_F(TriggerTest, AddTrigger) {
    TriggerConfig config;
    config.name = "test_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.enabled = true;

    size_t id = manager->add(config);
    EXPECT_GT(id, 0);
}

TEST_F(TriggerTest, EnableDisableTrigger) {
    TriggerConfig config;
    config.name = "test_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.enabled = false;

    size_t id = manager->add(config);
    EXPECT_FALSE(manager->isRunning(id));

    manager->enable(id);
    EXPECT_TRUE(manager->isRunning(id));

    manager->disable(id);
    EXPECT_FALSE(manager->isRunning(id));
}

TEST_F(TriggerTest, RemoveTrigger) {
    TriggerConfig config;
    config.name = "test_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.enabled = true;

    size_t id = manager->add(config);
    manager->remove(id);
    EXPECT_FALSE(manager->isRunning(id));
}

TEST_F(TriggerTest, SetLuaState) {
    lua_State* L = luaL_newstate();
    ASSERT_NE(L, nullptr);
    manager->setLuaState(L);
    lua_close(L);
}

TEST_F(TriggerTest, LogAction) {
    // Test that log action works
    TriggerConfig config;
    config.name = "log_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 50;
    config.condition.enabled = true;
    config.enabled = true;
    config.oneShot = true;

    TriggerActionData action;
    action.type = TriggerAction::Log;
    action.value = "Test log message";
    config.actions.push_back(action);

    manager->add(config);
    manager->start();

    // Wait for trigger to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    manager->stop();
    SUCCEED();
}
