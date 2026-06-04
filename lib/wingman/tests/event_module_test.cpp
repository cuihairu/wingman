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

TEST(EventModuleTest, EmitReturnsBool) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({ScriptValue::fromString("test.emit")});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithPayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.emit.payload"),
		ScriptValue::fromObject({{"key", ScriptValue::fromString("value")}})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithMeta) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.emit.meta"),
		ScriptValue::fromObject({{"data", ScriptValue::fromInt(42)}}),
		ScriptValue::fromObject({
			{"source", ScriptValue::fromString("test")},
			{"correlationId", ScriptValue::fromString("abc")},
			{"priority", ScriptValue::fromInt(1)}
		})
	});
	EXPECT_TRUE(result.isBool());
}

TEST(EventModuleTest, EmitEmptyArgsReturnsFalse) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(EventModuleTest, OffWithSubscriptionId) {
	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* offFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "off") { offFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(offFunc, nullptr);

	auto callback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	};
	auto subId = (*onFunc)({
		ScriptValue::fromString("test.off.event"),
		ScriptValue::fromCallable(callback)
	});
	EXPECT_TRUE(subId.isInt());

	auto result = (*offFunc)({subId});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, OffWithStringType) {
	auto mod = createEventModule();
	const ScriptFunction* offFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "off") { offFunc = &fn.func; break; }
	}
	ASSERT_NE(offFunc, nullptr);

	auto result = (*offFunc)({ScriptValue::fromString("test.off.type")});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, OffEmptyArgsReturnsFalse) {
	auto mod = createEventModule();
	const ScriptFunction* offFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "off") { offFunc = &fn.func; break; }
	}
	ASSERT_NE(offFunc, nullptr);

	auto result = (*offFunc)({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(EventModuleTest, ClearReturnsNull) {
	auto mod = createEventModule();
	const ScriptFunction* clearFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "clear") { clearFunc = &fn.func; break; }
	}
	ASSERT_NE(clearFunc, nullptr);

	auto result = (*clearFunc)({});
	EXPECT_TRUE(result.isNull());
}

TEST(EventModuleTest, MessageCreatesEventObject) {
	auto mod = createEventModule();
	const ScriptFunction* msgFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "message") { msgFunc = &fn.func; break; }
	}
	ASSERT_NE(msgFunc, nullptr);

	auto result = (*msgFunc)({ScriptValue::fromString("test.msg")});
	EXPECT_TRUE(result.isObject());
	auto* type = result.get("type");
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->asString(), "test.msg");
}

TEST(EventModuleTest, MessageWithPayloadAndMeta) {
	auto mod = createEventModule();
	const ScriptFunction* msgFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "message") { msgFunc = &fn.func; break; }
	}
	ASSERT_NE(msgFunc, nullptr);

	auto result = (*msgFunc)({
		ScriptValue::fromString("test.msg.full"),
		ScriptValue::fromObject({{"data", ScriptValue::fromInt(123)}}),
		ScriptValue::fromObject({
			{"source", ScriptValue::fromString("src")},
			{"correlationId", ScriptValue::fromString("corr")},
			{"priority", ScriptValue::fromInt(2)}
		})
	});
	EXPECT_TRUE(result.isObject());
}

TEST(EventModuleTest, OnWithNonCallableReturnsFalse) {
	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; break; }
	}
	ASSERT_NE(onFunc, nullptr);

	auto result = (*onFunc)({
		ScriptValue::fromString("test.event"),
		ScriptValue::fromString("not a callable")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(EventModuleTest, OnEmptyArgsReturnsFalse) {
	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; break; }
	}
	ASSERT_NE(onFunc, nullptr);

	auto result = (*onFunc)({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(EventModuleTest, OnceWithNonCallableReturnsFalse) {
	auto mod = createEventModule();
	const ScriptFunction* onceFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "once") { onceFunc = &fn.func; break; }
	}
	ASSERT_NE(onceFunc, nullptr);

	auto result = (*onceFunc)({
		ScriptValue::fromString("test.event"),
		ScriptValue::fromInt(42)
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST(EventModuleTest, OnceEmptyArgsReturnsFalse) {
	auto mod = createEventModule();
	const ScriptFunction* onceFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "once") { onceFunc = &fn.func; break; }
	}
	ASSERT_NE(onceFunc, nullptr);

	auto result = (*onceFunc)({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}
