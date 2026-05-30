#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createEventModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

TEST(EventModuleTest, HasExpectedFunctions) {
	auto mod = createEventModule();

	EXPECT_EQ(mod.name, "event");
	ASSERT_GE(mod.functions.size(), 5u);

	bool hasEmit = false;
	bool hasOff = false;
	bool hasClear = false;
	bool hasOn = false;
	bool hasOnce = false;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") hasEmit = true;
		if (fn.name == "off") hasOff = true;
		if (fn.name == "clear") hasClear = true;
		if (fn.name == "on") hasOn = true;
		if (fn.name == "once") hasOnce = true;
	}

	EXPECT_TRUE(hasEmit);
	EXPECT_TRUE(hasOff);
	EXPECT_TRUE(hasClear);
	EXPECT_TRUE(hasOn);
	EXPECT_TRUE(hasOnce);
}

TEST(EventModuleTest, OnReturnsSubscriptionId) {
	auto mod = createEventModule();

	// Find the 'on' function
	const ScriptFunction* onFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") {
			onFunc = &fn.func;
			break;
		}
	}
	ASSERT_NE(onFunc, nullptr);

	// Test that on accepts a callback and returns a subscription ID
	bool callbackCalled = false;
	auto callback = [&callbackCalled](const std::vector<ScriptValue>& args) -> ScriptValue {
		callbackCalled = true;
		return ScriptValue::null();
	};

	std::vector<ScriptValue> args = {
		ScriptValue::fromString("test.event"),
		ScriptValue::fromCallable(callback),
		ScriptValue::fromString("test-sub")
	};

	ScriptValue result = (*onFunc)(args);
	EXPECT_TRUE(result.isInt());
	EXPECT_GT(result.asInt(), 0);
}

TEST(EventModuleTest, OnceReturnsSubscriptionId) {
	auto mod = createEventModule();

	// Find the 'once' function
	const ScriptFunction* onceFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "once") {
			onceFunc = &fn.func;
			break;
		}
	}
	ASSERT_NE(onceFunc, nullptr);

	// Test that once accepts a callback and returns a subscription ID
	auto callback = [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::null();
	};

	std::vector<ScriptValue> args = {
		ScriptValue::fromString("test.once.event"),
		ScriptValue::fromCallable(callback)
	};

	ScriptValue result = (*onceFunc)(args);
	EXPECT_TRUE(result.isInt());
	EXPECT_GT(result.asInt(), 0);
}
