#include <gtest/gtest.h>
#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/trigger.hpp"
#include "wingman/platform/mock_input.hpp"
#include <nlohmann/json.hpp>

using namespace wingman;
using namespace wingman::rpc;
using json = nlohmann::json;

// trigger_handler.cpp 第九批覆盖率收口（2026-09-23）：rpc_test.cpp 的 ListTriggers
// 只 list 过 ColorFound + Click 单一组合，triggerTypeToString/actionTypeToString
// 的其余枚举 case 全部零覆盖。本文件通过 list 全枚举 condition/action 类型 +
// 非法枚举值兜底分支，收齐两个序列化函数的全部 case 与 switch 后兜底 return。

namespace {

class TriggerListSerializationTest : public ::testing::Test {
protected:
	void SetUp() override {
		registerTriggerHandlers(dispatcher, manager);
	}

	json call(const std::string& method, const json& params = json::object()) {
		json req = {{"method", method}, {"id", "1"}, {"params", params}};
		return json::parse(dispatcher.dispatch(req.dump()));
	}

	json addTrigger(const std::string& name, const json& condition, json actions) {
		json params = {{"config", {
			{"name", name},
			{"condition", condition},
			{"actions", std::move(actions)}
		}}};
		return call("trigger.add", params);
	}

	RpcDispatcher dispatcher;
	TriggerManager manager{std::make_shared<platform::mock::MockInput>()};
};

} // anonymous namespace

// 全部 11 种 condition 类型的字符串序列化（triggerTypeToString 各 case）
TEST_F(TriggerListSerializationTest, ListSerializesAllConditionTypes) {
	const std::vector<std::string> types = {
		"ColorFound", "ColorLost", "ImageFound", "ImageLost", "WindowOpened",
		"WindowClosed", "ProcessStarted", "ProcessStopped", "TimeElapsed",
		"HotkeyPressed", "PixelChanged"
	};
	for (size_t i = 0; i < types.size(); ++i) {
		auto resp = addTrigger("cond-" + types[i],
			{{"type", types[i]}, {"value", "#FF0000"}},
			json::array({{{"type", "Log"}}}));
		ASSERT_TRUE(resp["data"]["success"].get<bool>()) << types[i];
	}

	auto list = call("trigger.list");
	const auto& triggers = list["data"]["result"]["triggers"];
	ASSERT_EQ(triggers.size(), types.size());
	for (const auto& t : triggers) {
		EXPECT_EQ(t["type"].get<std::string>(), t["name"].get<std::string>().substr(5))
			<< t["name"].get<std::string>();
	}
}

// 全部 10 种 action 类型的字符串序列化（actionTypeToString 各 case）
TEST_F(TriggerListSerializationTest, ListSerializesAllActionTypes) {
	const std::vector<std::string> actions = {
		"RunScript", "Click", "KeyPress", "Type", "StopScript",
		"PauseScript", "ShowMessage", "PlayAudio", "Log", "Delay"
	};
	json actionArr = json::array();
	for (const auto& a : actions) {
		actionArr.push_back({{"type", a}, {"value", "v-" + a}, {"x", 1}, {"y", 2}, {"delay", 3}});
	}
	auto resp = addTrigger("all-actions", {{"type", "ColorFound"}}, std::move(actionArr));
	ASSERT_TRUE(resp["data"]["success"].get<bool>());

	auto list = call("trigger.list");
	const auto& triggers = list["data"]["result"]["triggers"];
	ASSERT_EQ(triggers.size(), 1u);
	const auto& gotActions = triggers[0]["actions"];
	ASSERT_EQ(gotActions.size(), actions.size());
	for (size_t i = 0; i < actions.size(); ++i) {
		EXPECT_EQ(gotActions[i]["type"].get<std::string>(), actions[i]);
		EXPECT_EQ(gotActions[i]["value"].get<std::string>(), "v-" + actions[i]);
	}
}

// 非法枚举值入库后序列化兜底：数值 999 经 static_cast 入库，list 时
// switch 无匹配走各自兜底 return（"ColorFound" / "Log"），宽容不崩溃
TEST_F(TriggerListSerializationTest, ListFallsBackOnInvalidEnumValues) {
	json params = {{"config", {
		{"name", "bogus-enum"},
		{"condition", {{"type", 999}}},
		{"actions", json::array({{{"type", 999}}})}
	}}};
	auto resp = call("trigger.add", params);
	ASSERT_TRUE(resp["data"]["success"].get<bool>());

	auto list = call("trigger.list");
	const auto& triggers = list["data"]["result"]["triggers"];
	ASSERT_EQ(triggers.size(), 1u);
	EXPECT_EQ(triggers[0]["type"].get<std::string>(), "ColorFound");
	ASSERT_EQ(triggers[0]["actions"].size(), 1u);
	EXPECT_EQ(triggers[0]["actions"][0]["type"].get<std::string>(), "Log");
}
