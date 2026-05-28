#include <gtest/gtest.h>
#include "wingman/trigger.hpp"
#include "wingman/trigger_engine.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <thread>
#include <chrono>

using namespace wingman;

class TriggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 使用默认logger而不是创建新的
        manager = std::make_unique<TriggerManager>();
    }

    void TearDown() override {
        manager->stop();
        manager.reset();
    }

    std::unique_ptr<TriggerManager> manager;
};

// ========== TriggerManager 基础功能 ==========

TEST_F(TriggerTest, AddTrigger) {
    TriggerConfig config;
    config.name = "test_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;
    config.oneShot = false;

    size_t id = manager->add(config);

    EXPECT_GT(id, 0);
    EXPECT_EQ(manager->getTriggerCount(), 1);
    EXPECT_TRUE(manager->hasTrigger(id));
}

TEST_F(TriggerTest, RemoveTrigger) {
    TriggerConfig config;
    config.name = "to_remove";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    size_t id = manager->add(config);
    EXPECT_EQ(manager->getTriggerCount(), 1);

    manager->remove(id);
    EXPECT_EQ(manager->getTriggerCount(), 0);
    EXPECT_FALSE(manager->hasTrigger(id));
}

TEST_F(TriggerTest, EnableDisableTrigger) {
    TriggerConfig config;
    config.name = "toggle_test";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    size_t id = manager->add(config);

    EXPECT_TRUE(manager->isRunning(id));

    manager->disable(id);
    EXPECT_FALSE(manager->isRunning(id));

    manager->enable(id);
    EXPECT_TRUE(manager->isRunning(id));
}

TEST_F(TriggerTest, UpdateTrigger) {
    TriggerConfig config;
    config.name = "update_test";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    size_t id = manager->add(config);

    TriggerConfig newConfig = config;
    newConfig.name = "updated_trigger";
    newConfig.cooldown = 2000;

    EXPECT_TRUE(manager->update(id, newConfig));

    auto retrieved = manager->getTriggerConfig(id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "updated_trigger");
    EXPECT_EQ(retrieved->cooldown, 2000);
}

TEST_F(TriggerTest, UpdateNonExistentTrigger) {
    TriggerConfig config;
    config.name = "test";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.enabled = true;

    EXPECT_FALSE(manager->update(99999, config));
}

// ========== 触发器配置 ==========

TEST_F(TriggerTest, GetTriggerConfig) {
    TriggerConfig config;
    config.name = "config_test";
    config.condition.type = TriggerType::ColorFound;
    config.condition.value = "0xFF0000";
    config.condition.region = {10, 20, 100, 100};
    config.condition.tolerance = 10;
    config.cooldown = 500;
    config.enabled = true;
    config.oneShot = true;

    TriggerActionData action;
    action.type = BasicTriggerAction::Log;
    action.value = "Test log message";
    config.actions.push_back(action);

    size_t id = manager->add(config);

    auto retrieved = manager->getTriggerConfig(id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "config_test");
    EXPECT_EQ(retrieved->condition.type, TriggerType::ColorFound);
    EXPECT_EQ(retrieved->condition.value, "0xFF0000");
    EXPECT_EQ(retrieved->condition.region.x, 10);
    EXPECT_EQ(retrieved->condition.region.y, 20);
    EXPECT_EQ(retrieved->condition.region.width, 100);
    EXPECT_EQ(retrieved->condition.region.height, 100);
    EXPECT_EQ(retrieved->condition.tolerance, 10);
    EXPECT_TRUE(retrieved->oneShot);
    EXPECT_EQ(retrieved->actions.size(), 1);
    EXPECT_EQ(retrieved->actions[0].type, BasicTriggerAction::Log);
}

TEST_F(TriggerTest, GetAllTriggerConfigs) {
    manager->add([] {
        TriggerConfig c;
        c.name = "trigger1";
        c.condition.type = TriggerType::TimeElapsed;
        c.condition.interval = 100;
        c.condition.enabled = true;
        c.cooldown = 1000;
        c.enabled = true;
        return c;
    }());

    manager->add([] {
        TriggerConfig c;
        c.name = "trigger2";
        c.condition.type = TriggerType::TimeElapsed;
        c.condition.interval = 200;
        c.condition.enabled = true;
        c.cooldown = 2000;
        c.enabled = true;
        return c;
    }());

    auto configs = manager->getAllTriggerConfigs();
    EXPECT_EQ(configs.size(), 2);
}

// ========== 触发器实例 ==========

TEST_F(TriggerTest, GetAllTriggerInstances) {
    TriggerConfig config;
    config.name = "instance_test";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    size_t id = manager->add(config);

    auto instances = manager->getAllTriggerInstances();
    ASSERT_EQ(instances.size(), 1);
    EXPECT_EQ(instances[0].id, id);
    EXPECT_GT(instances[0].startTime, 0);
    EXPECT_EQ(instances[0].lastTriggerTime, 0);
    EXPECT_FALSE(instances[0].triggered);
}

// ========== 启动停止 ==========

TEST_F(TriggerTest, StartStopManager) {
    EXPECT_FALSE(manager->isRunning(1));  // No triggers yet

    TriggerConfig config;
    config.name = "start_test";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    size_t id = manager->add(config);

    manager->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    manager->stop();
    EXPECT_TRUE(manager->hasTrigger(id));
}

// ========== 多个触发器 ==========

TEST_F(TriggerTest, MultipleTriggers) {
    for (int i = 0; i < 5; ++i) {
        TriggerConfig config;
        config.name = "trigger_" + std::to_string(i);
        config.condition.type = TriggerType::TimeElapsed;
        config.condition.interval = 100 * (i + 1);
        config.condition.enabled = true;
        config.cooldown = 1000;
        config.enabled = true;
        manager->add(config);
    }

    EXPECT_EQ(manager->getTriggerCount(), 5);

    auto configs = manager->getAllTriggerConfigs();
    EXPECT_EQ(configs.size(), 5);

    auto instances = manager->getAllTriggerInstances();
    EXPECT_EQ(instances.size(), 5);
}

// ========== 触发器动作类型 ==========

TEST_F(TriggerTest, AllActionTypesExist) {
    // 验证所有动作类型可以正确设置
    std::vector<BasicTriggerAction> types = {
        BasicTriggerAction::RunScript,
        BasicTriggerAction::Click,
        BasicTriggerAction::KeyPress,
        BasicTriggerAction::Type,
        BasicTriggerAction::StopScript,
        BasicTriggerAction::PauseScript,
        BasicTriggerAction::ShowMessage,
        BasicTriggerAction::PlayAudio,
        BasicTriggerAction::Log
    };

    for (auto type : types) {
        TriggerActionData action;
        action.type = type;
        action.value = "test_value";
        action.x = 100;
        action.y = 200;
        action.delay = 50;

        TriggerConfig config;
        config.name = "action_test";
        config.condition.type = TriggerType::TimeElapsed;
        config.condition.interval = 100;
        config.condition.enabled = true;
        config.actions.push_back(action);
        config.cooldown = 1000;
        config.enabled = true;

        size_t id = manager->add(config);
        EXPECT_GT(id, 0);

        auto retrieved = manager->getTriggerConfig(id);
        ASSERT_TRUE(retrieved.has_value());
        EXPECT_EQ(retrieved->actions[0].type, type);
    }
}

// ========== TriggerEngine 测试 ==========

class TriggerEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<TriggerEngine>();
    }

    void TearDown() override {
        engine->stop();
        engine.reset();
    }

    std::unique_ptr<TriggerEngine> engine;
};

TEST_F(TriggerEngineTest, InitialState) {
    EXPECT_FALSE(engine->isRunning());
    EXPECT_NE(engine->getManager(), nullptr);

    auto stats = engine->getStats();
    EXPECT_EQ(stats.totalTriggers, 0);
}

TEST_F(TriggerEngineTest, StartStop) {
    EXPECT_TRUE(engine->start());
    EXPECT_TRUE(engine->isRunning());

    engine->stop();
    EXPECT_FALSE(engine->isRunning());
}

TEST_F(TriggerEngineTest, StartWhenAlreadyRunning) {
    engine->start();
    EXPECT_TRUE(engine->start());  // Should be idempotent
    EXPECT_TRUE(engine->isRunning());

    engine->stop();
}

TEST_F(TriggerEngineTest, GetStats) {
    TriggerConfig config;
    config.name = "stats_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    auto* mgr = engine->getManager();
    mgr->add(config);

    auto stats = engine->getStats();
    EXPECT_EQ(stats.totalTriggers, 1);
}

TEST_F(TriggerEngineTest, EnableDisableByName) {
    auto* mgr = engine->getManager();

    TriggerConfig config;
    config.name = "test_trigger";
    config.condition.type = TriggerType::TimeElapsed;
    config.condition.interval = 100;
    config.condition.enabled = true;
    config.cooldown = 1000;
    config.enabled = true;

    mgr->add(config);

    // Engine doesn't track names directly, so this tests the interface
    EXPECT_FALSE(engine->enableTrigger("non_existent"));
    EXPECT_FALSE(engine->disableTrigger("non_existent"));
}

TEST_F(TriggerEngineTest, LoadFromLuaNonexistentFile) {
    EXPECT_FALSE(engine->loadFromLua("/nonexistent/trigger_config.lua"));
}

TEST_F(TriggerEngineTest, LoadFromYAMLReturnsFalse) {
    EXPECT_FALSE(engine->loadFromYAML("/nonexistent/config.yaml"));
}

TEST_F(TriggerEngineTest, StopWhenNotRunningDoesNotCrash) {
    EXPECT_NO_THROW(engine->stop());
}

TEST_F(TriggerEngineTest, DoubleStopDoesNotCrash) {
    engine->start();
    EXPECT_NO_THROW(engine->stop());
    EXPECT_NO_THROW(engine->stop());
}

TEST_F(TriggerEngineTest, GetStatsEmpty) {
    auto stats = engine->getStats();
    EXPECT_EQ(stats.totalTriggers, 0);
    EXPECT_EQ(stats.enabledTriggers, 0);
    EXPECT_EQ(stats.totalTriggered, 0);
}

TEST_F(TriggerEngineTest, EnableDisableWithNoTriggers) {
    EXPECT_FALSE(engine->enableTrigger("any"));
    EXPECT_FALSE(engine->disableTrigger("any"));
}
