#include "event_module_test_common.hpp"

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
