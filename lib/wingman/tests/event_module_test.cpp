#include <gtest/gtest.h>

#include "wingman/event.hpp"
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
using wingman::EventHub;
using wingman::EventMessage;

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

// ========== Callback Invocation Tests ==========

TEST(EventModuleTest, OnCallbackInvokedByEmit) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	int callbackCount = 0;
	auto callback = [&callbackCount](const std::vector<ScriptValue>& args) -> ScriptValue {
		callbackCount++;
		return ScriptValue::null();
	};

	// Register callback
	auto subId = (*onFunc)({
		ScriptValue::fromString("test.invoke"),
		ScriptValue::fromCallable(callback)
	});
	ASSERT_TRUE(subId.isInt());

	// Emit event to trigger callback
	(*emitFunc)({ScriptValue::fromString("test.invoke")});
	EXPECT_EQ(callbackCount, 1);

	// Emit again - should fire again (not once)
	(*emitFunc)({ScriptValue::fromString("test.invoke")});
	EXPECT_EQ(callbackCount, 2);

	hub.clear();
}

TEST(EventModuleTest, OnceCallbackAutoRemoved) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onceFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "once") { onceFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onceFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	int callbackCount = 0;
	auto callback = [&callbackCount](const std::vector<ScriptValue>&) -> ScriptValue {
		callbackCount++;
		return ScriptValue::null();
	};

	// Register once callback
	(*onceFunc)({
		ScriptValue::fromString("test.once.invoke"),
		ScriptValue::fromCallable(callback)
	});

	// Emit - should fire once
	(*emitFunc)({ScriptValue::fromString("test.once.invoke")});
	EXPECT_EQ(callbackCount, 1);

	// Emit again - should NOT fire
	(*emitFunc)({ScriptValue::fromString("test.once.invoke")});
	EXPECT_EQ(callbackCount, 1);

	hub.clear();
}

TEST(EventModuleTest, OnCallbackReceivesPayload) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	bool callbackCalled = false;
	auto callback = [&callbackCalled](const std::vector<ScriptValue>& args) -> ScriptValue {
		callbackCalled = true;
		if (!args.empty() && args[0].isObject()) {
			auto* type = args[0].get("type");
			EXPECT_NE(type, nullptr);
		}
		return ScriptValue::null();
	};

	(*onFunc)({
		ScriptValue::fromString("test.payload.cb"),
		ScriptValue::fromCallable(callback)
	});

	// Emit with payload
	(*emitFunc)({
		ScriptValue::fromString("test.payload.cb"),
		ScriptValue::fromObject({{"key", ScriptValue::fromString("value")}})
	});
	EXPECT_TRUE(callbackCalled);

	hub.clear();
}

TEST(EventModuleTest, EmitWithAllMetaFields) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.full.meta"),
		ScriptValue::fromObject({{"data", ScriptValue::fromInt(42)}}),
		ScriptValue::fromObject({
			{"source", ScriptValue::fromString("test-source")},
			{"correlationId", ScriptValue::fromString("corr-123")},
			{"priority", ScriptValue::fromInt(5)}
		})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithPartialMeta) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.partial.meta"),
		ScriptValue::fromObject({{"x", ScriptValue::fromInt(1)}}),
		ScriptValue::fromObject({
			{"source", ScriptValue::fromString("src")}
		})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, MessageWithOnlyType) {
	auto mod = createEventModule();
	const ScriptFunction* msgFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "message") { msgFunc = &fn.func; break; }
	}
	ASSERT_NE(msgFunc, nullptr);

	auto result = (*msgFunc)({ScriptValue::fromString("simple.msg")});
	EXPECT_TRUE(result.isObject());
	auto* type = result.get("type");
	ASSERT_NE(type, nullptr);
	EXPECT_EQ(type->asString(), "simple.msg");

	auto* source = result.get("source");
	ASSERT_NE(source, nullptr);
	EXPECT_EQ(source->asString(), "");

	auto* timestamp = result.get("timestamp");
	ASSERT_NE(timestamp, nullptr);
}

TEST(EventModuleTest, MessageWithPayloadOnly) {
	auto mod = createEventModule();
	const ScriptFunction* msgFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "message") { msgFunc = &fn.func; break; }
	}
	ASSERT_NE(msgFunc, nullptr);

	auto result = (*msgFunc)({
		ScriptValue::fromString("msg.with.payload"),
		ScriptValue::fromObject({{"count", ScriptValue::fromInt(99)}})
	});
	EXPECT_TRUE(result.isObject());
	auto* payload = result.get("payload");
	ASSERT_NE(payload, nullptr);
	EXPECT_TRUE(payload->isObject());
}

TEST(EventModuleTest, OffBySubscriptionId) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* offFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "off") { offFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(offFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	int count = 0;
	auto callback = [&count](const std::vector<ScriptValue>&) -> ScriptValue {
		count++;
		return ScriptValue::null();
	};

	auto subId = (*onFunc)({
		ScriptValue::fromString("test.off.byid"),
		ScriptValue::fromCallable(callback)
	});
	ASSERT_TRUE(subId.isInt());

	// Emit to verify callback works
	(*emitFunc)({ScriptValue::fromString("test.off.byid")});
	EXPECT_EQ(count, 1);

	// Unsubscribe by ID
	(*offFunc)({subId});

	// Emit again - callback should not fire
	(*emitFunc)({ScriptValue::fromString("test.off.byid")});
	EXPECT_EQ(count, 1);

	hub.clear();
}

// ========== toJson/fromJson Coverage Tests ==========

TEST(EventModuleTest, EmitWithArrayPayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.array"),
		ScriptValue::fromArray({
			ScriptValue::fromInt(1),
			ScriptValue::fromString("two"),
			ScriptValue::fromBool(true)
		})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithNestedObjectPayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.nested"),
		ScriptValue::fromObject({
			{"inner", ScriptValue::fromObject({
				{"x", ScriptValue::fromInt(10)},
				{"y", ScriptValue::fromInt(20)}
			})},
			{"flag", ScriptValue::fromBool(false)},
			{"ratio", ScriptValue::fromFloat(3.14)}
		})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithFloatPayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.float"),
		ScriptValue::fromFloat(2.718)
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithBoolPayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.bool"),
		ScriptValue::fromBool(true)
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithNullPayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	auto result = (*emitFunc)({
		ScriptValue::fromString("test.null"),
		ScriptValue::null()
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, EmitWithCallablePayload) {
	auto mod = createEventModule();
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "emit") { emitFunc = &fn.func; break; }
	}
	ASSERT_NE(emitFunc, nullptr);

	// Callable should serialize to null in JSON
	auto result = (*emitFunc)({
		ScriptValue::fromString("test.callable"),
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::null();
		})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST(EventModuleTest, OnCallbackReceivesEventObject) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	bool callbackCalled = false;
	std::string receivedType;
	auto callback = [&callbackCalled, &receivedType](const std::vector<ScriptValue>& args) -> ScriptValue {
		callbackCalled = true;
		if (!args.empty() && args[0].isObject()) {
			auto* type = args[0].get("type");
			if (type && type->isString()) {
				receivedType = type->asString();
			}
		}
		return ScriptValue::null();
	};

	(*onFunc)({
		ScriptValue::fromString("test.event.obj"),
		ScriptValue::fromCallable(callback)
	});

	(*emitFunc)({ScriptValue::fromString("test.event.obj")});
	EXPECT_TRUE(callbackCalled);
	EXPECT_EQ(receivedType, "test.event.obj");

	hub.clear();
}

TEST(EventModuleTest, OnCallbackReceivesPayloadData) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	bool payloadCorrect = false;
	auto callback = [&payloadCorrect](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (!args.empty() && args[0].isObject()) {
			auto* payload = args[0].get("payload");
			if (payload && payload->isObject()) {
				auto* val = payload->get("key");
				if (val && val->isString() && val->asString() == "value") {
					payloadCorrect = true;
				}
			}
		}
		return ScriptValue::null();
	};

	(*onFunc)({
		ScriptValue::fromString("test.payload.data"),
		ScriptValue::fromCallable(callback)
	});

	(*emitFunc)({
		ScriptValue::fromString("test.payload.data"),
		ScriptValue::fromObject({{"key", ScriptValue::fromString("value")}})
	});
	EXPECT_TRUE(payloadCorrect);

	hub.clear();
}

TEST(EventModuleTest, MessageWithArrayPayload) {
	auto mod = createEventModule();
	const ScriptFunction* msgFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "message") { msgFunc = &fn.func; break; }
	}
	ASSERT_NE(msgFunc, nullptr);

	auto result = (*msgFunc)({
		ScriptValue::fromString("msg.arr"),
		ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)})
	});
	EXPECT_TRUE(result.isObject());
	auto* payload = result.get("payload");
	ASSERT_NE(payload, nullptr);
	EXPECT_TRUE(payload->isArray());
}

TEST(EventModuleTest, MessageWithMetaPriority) {
	auto mod = createEventModule();
	const ScriptFunction* msgFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "message") { msgFunc = &fn.func; break; }
	}
	ASSERT_NE(msgFunc, nullptr);

	auto result = (*msgFunc)({
		ScriptValue::fromString("msg.priority"),
		ScriptValue::fromObject({{"x", ScriptValue::fromInt(1)}}),
		ScriptValue::fromObject({
			{"source", ScriptValue::fromString("src")},
			{"priority", ScriptValue::fromInt(5)}
		})
	});
	EXPECT_TRUE(result.isObject());
	auto* source = result.get("source");
	ASSERT_NE(source, nullptr);
	EXPECT_EQ(source->asString(), "src");

	auto* priority = result.get("priority");
	ASSERT_NE(priority, nullptr);
	EXPECT_EQ(priority->asInt(), 5);
}

// ========== Exception handling coverage ==========

TEST(EventModuleTest, OnCallbackExceptionIsCaught) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	// Register a callback that throws std::runtime_error
	auto throwingCallback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		throw std::runtime_error("test exception in on callback");
	};

	(*onFunc)({
		ScriptValue::fromString("test.on.exception"),
		ScriptValue::fromCallable(throwingCallback)
	});

	// Emit should not crash even though callback throws
	EXPECT_NO_THROW((*emitFunc)({ScriptValue::fromString("test.on.exception")}));

	hub.clear();
}

TEST(EventModuleTest, OnceCallbackExceptionIsCaught) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onceFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "once") { onceFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onceFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	// Register a once callback that throws a non-std exception
	auto throwingCallback = [](const std::vector<ScriptValue>&) -> ScriptValue {
		throw 42; // non-std exception
	};

	(*onceFunc)({
		ScriptValue::fromString("test.once.exception"),
		ScriptValue::fromCallable(throwingCallback)
	});

	// Emit should not crash even though callback throws
	EXPECT_NO_THROW((*emitFunc)({ScriptValue::fromString("test.once.exception")}));

	hub.clear();
}

TEST(EventModuleTest, FromJsonUnsignedInt) {
	auto& hub = EventHub::instance();
	hub.clear();

	auto mod = createEventModule();
	const ScriptFunction* onFunc = nullptr;
	const ScriptFunction* emitFunc = nullptr;
	for (const auto& fn : mod.functions) {
		if (fn.name == "on") { onFunc = &fn.func; }
		if (fn.name == "emit") { emitFunc = &fn.func; }
	}
	ASSERT_NE(onFunc, nullptr);
	ASSERT_NE(emitFunc, nullptr);

	bool received = false;
	auto callback = [&received](const std::vector<ScriptValue>& args) -> ScriptValue {
		received = true;
		return ScriptValue::null();
	};

	(*onFunc)({
		ScriptValue::fromString("test.unsigned"),
		ScriptValue::fromCallable(callback)
	});

	// Emit with payload containing unsigned int (large positive number)
	(*emitFunc)({
		ScriptValue::fromString("test.unsigned"),
		ScriptValue::fromObject({{"big", ScriptValue::fromInt(999999999999LL)}})
	});
	EXPECT_TRUE(received);

	hub.clear();
}
