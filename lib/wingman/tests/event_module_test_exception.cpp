#include "event_module_test_common.hpp"

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
