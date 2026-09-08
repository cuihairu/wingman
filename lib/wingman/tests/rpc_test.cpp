#include <gtest/gtest.h>
#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/rpc/system_handler.hpp"
#include "wingman/trigger.hpp"
#include "wingman/platform/mock_input.hpp"
#include <nlohmann/json.hpp>

using namespace wingman;
using namespace wingman::rpc;
using json = nlohmann::json;

// ========== RpcDispatcher 基础 ==========

TEST(RpcDispatcherTest, RegisterAndDispatch) {
    RpcDispatcher dispatcher;
    dispatcher.registerHandler("echo", [](const json& params) -> json {
        return params;
    });

    auto response = json::parse(dispatcher.dispatch(R"({"method":"echo","id":"1","params":{"msg":"hi"}})"));
    EXPECT_EQ(response["type"], "response");
    EXPECT_EQ(response["id"], "1");
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["result"]["msg"], "hi");
}

TEST(RpcDispatcherTest, InvalidJsonReturnsError) {
    RpcDispatcher dispatcher;
    auto response = json::parse(dispatcher.dispatch("not a json"));
    EXPECT_EQ(response["type"], "error");
    EXPECT_TRUE(response["error"].get<std::string>().find("Invalid JSON") == 0);
}

TEST(RpcDispatcherTest, UnknownMethodReturnsError) {
    RpcDispatcher dispatcher;
    auto response = json::parse(dispatcher.dispatch(R"({"method":"nope","id":"42"})"));
    EXPECT_EQ(response["type"], "response");
    EXPECT_EQ(response["id"], "42");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"], "Unknown method: nope");
}

TEST(RpcDispatcherTest, HandlerResultWithSuccessUsedDirectly) {
    RpcDispatcher dispatcher;
    dispatcher.registerHandler("custom", [](const json&) -> json {
        return {{"success", false}, {"error", "custom failure"}};
    });

    auto response = json::parse(dispatcher.dispatch(R"({"method":"custom","id":"x"})"));
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"], "custom failure");
}

TEST(RpcDispatcherTest, HandlerThrowingReturnsError) {
    RpcDispatcher dispatcher;
    dispatcher.registerHandler("boom", [](const json&) -> json {
        throw std::runtime_error("handler exploded");
    });

    auto response = json::parse(dispatcher.dispatch(R"({"method":"boom","id":"9"})"));
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"], "handler exploded");
}

TEST(RpcDispatcherTest, RegisterHandlerOverwrites) {
    RpcDispatcher dispatcher;
    dispatcher.registerHandler("m", [](const json&) -> json { return 1; });
    dispatcher.registerHandler("m", [](const json&) -> json { return 2; });

    auto response = json::parse(dispatcher.dispatch(R"({"method":"m"})"));
    EXPECT_EQ(response["data"]["result"], 2);
}

// ========== Trigger Handlers ==========

class TriggerRpcTest : public ::testing::Test {
protected:
    void SetUp() override {
        registerTriggerHandlers(dispatcher, manager);
        registerSystemHandlers(dispatcher, "test-version");
    }

    json call(const std::string& method, const json& params = json::object(), const std::string& id = "1") {
        json req = {{"method", method}, {"id", id}, {"params", params}};
        return json::parse(dispatcher.dispatch(req.dump()));
    }

    size_t addTrigger(const std::string& name, bool enabled = true) {
        json params = {{"config", {
            {"name", name},
            {"enabled", enabled},
            {"condition", {{"type", "ColorFound"}, {"value", "#FF0000"}}},
            {"actions", json::array({{{"type", "Click"}, {"x", 10}, {"y", 20}}})}
        }}};
        auto response = call("trigger.add", params);
        return std::stoull(response["data"]["result"]["id"].get<std::string>());
    }

    RpcDispatcher dispatcher;
    TriggerManager manager{std::make_shared<platform::mock::MockInput>()};
};

TEST_F(TriggerRpcTest, AddTriggerDefaults) {
    auto response = call("trigger.add", {{"config", {{"name", "t1"}}}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    EXPECT_EQ(manager.getTriggerCount(), 1u);
    size_t id = std::stoull(response["data"]["result"]["id"].get<std::string>());
    auto config = manager.getTriggerConfig(id);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->name, "t1");
    EXPECT_EQ(config->condition.type, TriggerType::ColorFound);
}

TEST_F(TriggerRpcTest, AddTriggerFullConfig) {
    size_t id = addTrigger("full");
    auto config = manager.getTriggerConfig(id);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->condition.type, TriggerType::ColorFound);
    EXPECT_EQ(config->condition.value, "#FF0000");
    ASSERT_EQ(config->actions.size(), 1u);
    EXPECT_EQ(config->actions[0].type, BasicTriggerAction::Click);
    EXPECT_EQ(config->actions[0].x, 10);
    EXPECT_EQ(config->actions[0].y, 20);
}

TEST_F(TriggerRpcTest, AddTriggerParamsAsConfig) {
    // 不带 "config" 包装时，params 本身作为配置
    auto response = call("trigger.add", {{"name", "direct"}, {"oneShot", true}, {"cooldown", 5}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    size_t id = std::stoull(response["data"]["result"]["id"].get<std::string>());
    auto config = manager.getTriggerConfig(id);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->name, "direct");
    EXPECT_TRUE(config->oneShot);
    EXPECT_EQ(config->cooldown, 5);
}

TEST_F(TriggerRpcTest, AddTriggerNumericTypeEnum) {
    json params = {{"config", {
        {"name", "numeric"},
        {"condition", {{"type", 4}, {"value", "x"}, {"tolerance", 20}, {"interval", 500}, {"enabled", false},
            {"region", {{"x", 1}, {"y", 2}, {"width", 3}, {"height", 4}}}}},
        {"actions", json::array({{{"type", 1}, {"value", "script"}}})}
    }}};
    auto response = call("trigger.add", params);
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    auto config = manager.getTriggerConfig(std::stoull(response["data"]["result"]["id"].get<std::string>()));
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->condition.type, TriggerType::WindowOpened);
    EXPECT_EQ(config->condition.tolerance, 20);
    EXPECT_EQ(config->condition.interval, 500);
    EXPECT_FALSE(config->condition.enabled);
    EXPECT_EQ(config->condition.region.x, 1);
    EXPECT_EQ(config->condition.region.width, 3);
    EXPECT_EQ(config->actions[0].type, BasicTriggerAction::Click);
    EXPECT_EQ(config->actions[0].value, "script");
}

TEST_F(TriggerRpcTest, AddTriggerAllStringTypes) {
    const std::vector<std::string> types = {
        "ColorFound", "pixel", "ColorLost", "ImageFound", "image", "ImageLost",
        "WindowOpened", "WindowClosed", "ProcessStarted", "ProcessStopped",
        "TimeElapsed", "HotkeyPressed", "PixelChanged", "Unknown"
    };
    for (const auto& t : types) {
        json params = {{"config", {
            {"name", std::string("s-") + t},
            {"condition", {{"type", t}}}
        }}};
        auto response = call("trigger.add", params);
        EXPECT_TRUE(response["data"]["success"].get<bool>()) << "type=" << t;
    }
}

TEST_F(TriggerRpcTest, AddTriggerAllActionTypes) {
    const std::vector<std::string> types = {
        "RunScript", "macro", "Click", "KeyPress", "key", "Type",
        "StopScript", "PauseScript", "ShowMessage", "PlayAudio",
        "Delay", "Whatever"
    };
    for (const auto& t : types) {
        json params = {{"config", {{"name", std::string("a-") + t},
            {"actions", json::array({{{"type", t}, {"value", "v"}, {"delay", 3}}})}
        }}};
        auto response = call("trigger.add", params);
        EXPECT_TRUE(response["data"]["success"].get<bool>()) << "type=" << t;
    }
}

TEST_F(TriggerRpcTest, ActionWithoutTypeDefaultsToLog) {
    json params = {{"config", {
        {"name", "no-action-type"},
        {"actions", json::array({json::object()})}
    }}};
    auto response = call("trigger.add", params);
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    auto config = manager.getTriggerConfig(std::stoull(response["data"]["result"]["id"].get<std::string>()));
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->actions.size(), 1u);
    EXPECT_EQ(config->actions[0].type, BasicTriggerAction::Log);
}

TEST_F(TriggerRpcTest, ListTriggers) {
    size_t id1 = addTrigger("t1", true);
    size_t id2 = addTrigger("t2", false);

    auto response = call("trigger.list");
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    const auto& triggers = response["data"]["result"]["triggers"];
    ASSERT_EQ(triggers.size(), 2u);
    EXPECT_EQ(triggers[0]["id"], std::to_string(id1));
    EXPECT_EQ(triggers[0]["name"], "t1");
    EXPECT_TRUE(triggers[0]["enabled"].get<bool>());
    EXPECT_EQ(triggers[0]["type"], "ColorFound");
    EXPECT_TRUE(triggers[0]["condition"].contains("region"));
    EXPECT_EQ(triggers[0]["condition"]["value"], "#FF0000");
    ASSERT_EQ(triggers[0]["actions"].size(), 1u);
    EXPECT_EQ(triggers[0]["actions"][0]["type"], "Click");
    EXPECT_EQ(triggers[1]["id"], std::to_string(id2));
    EXPECT_FALSE(triggers[1]["enabled"].get<bool>());
}

TEST_F(TriggerRpcTest, RemoveTrigger) {
    size_t id = addTrigger("doomed");
    ASSERT_EQ(manager.getTriggerCount(), 1u);

    auto response = call("trigger.remove", {{"id", std::to_string(id)}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    EXPECT_EQ(manager.getTriggerCount(), 0u);
}

TEST_F(TriggerRpcTest, UpdateTrigger) {
    size_t id = addTrigger("original");
    json params = {{"id", std::to_string(id)}, {"config", {{"name", "updated"}, {"enabled", false}}}};
    auto response = call("trigger.update", params);
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    auto config = manager.getTriggerConfig(id);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->name, "updated");
    EXPECT_FALSE(config->enabled);
}

TEST_F(TriggerRpcTest, UpdateNonexistentTrigger) {
    auto response = call("trigger.update", {{"id", "999"}, {"config", {{"name", "x"}}}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"], "Trigger not found");
}

TEST_F(TriggerRpcTest, ToggleTrigger) {
    size_t id = addTrigger("toggle-me", true);

    auto off = call("trigger.toggle", {{"id", std::to_string(id)}});
    EXPECT_TRUE(off["data"]["success"].get<bool>());
    EXPECT_FALSE(off["data"]["result"]["enabled"].get<bool>());
    EXPECT_FALSE(manager.getTriggerConfig(id)->enabled);

    auto on = call("trigger.toggle", {{"id", std::to_string(id)}});
    EXPECT_TRUE(on["data"]["result"]["enabled"].get<bool>());
    EXPECT_TRUE(manager.getTriggerConfig(id)->enabled);
}

TEST_F(TriggerRpcTest, ToggleNonexistentTrigger) {
    auto response = call("trigger.toggle", {{"id", "1234"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"], "Trigger not found");
}

// ========== System Handlers ==========

TEST_F(TriggerRpcTest, SystemGetStatus) {
    auto response = call("system.getStatus");
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["server"], "wingman");
    EXPECT_EQ(result["version"], "0.1.0");
    EXPECT_TRUE(result.contains("uptime"));
    EXPECT_EQ(result["runningScripts"], 0);
    EXPECT_FALSE(result["paused"].get<bool>());
}

TEST_F(TriggerRpcTest, SystemGetVersion) {
    auto response = call("system.getVersion");
    EXPECT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["server"], "wingman");
    EXPECT_EQ(result["version"], "test-version");
    EXPECT_TRUE(result.contains("buildDate"));
}
