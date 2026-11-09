#include "event_module_test_common.hpp"

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
