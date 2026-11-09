#include "fsm_module_test_common.hpp"

// ========== Guard/Action/Callback Tests ==========

TEST_F(FsmModuleTest, DispatchWithGuardAllowsTransition) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto state = findFsmFunction(mod, "state");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("guard_allow"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	bool onExitCalled = false;
	bool onEnterCalled = false;
	state({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::null(),
		ScriptValue::fromCallable([&onExitCalled](const std::vector<ScriptValue>&) -> ScriptValue {
			onExitCalled = true;
			return ScriptValue::null();
		})
	});

	state({
		machineId,
		ScriptValue::fromString("running"),
		ScriptValue::fromCallable([&onEnterCalled](const std::vector<ScriptValue>&) -> ScriptValue {
			onEnterCalled = true;
			return ScriptValue::null();
		}),
		ScriptValue::null()
	});

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("go"),
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromBool(true);
		})
	});

	auto result = dispatch({machineId, ScriptValue::fromString("go")});
	EXPECT_TRUE(result.asBool());
	EXPECT_EQ(current({machineId}).asString(), "running");
	EXPECT_TRUE(onExitCalled);
	EXPECT_TRUE(onEnterCalled);
}

TEST_F(FsmModuleTest, DispatchWithGuardRejectsTransition) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("guard_reject"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("go"),
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromBool(false);
		})
	});

	auto result = dispatch({machineId, ScriptValue::fromString("go")});
	EXPECT_FALSE(result.asBool());
	EXPECT_EQ(current({machineId}).asString(), "idle");
}

TEST_F(FsmModuleTest, DispatchWithActionCallback) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("action_test"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	bool actionCalled = false;
	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("go"),
		ScriptValue::null(),
		ScriptValue::fromCallable([&actionCalled](const std::vector<ScriptValue>&) -> ScriptValue {
			actionCalled = true;
			return ScriptValue::null();
		})
	});

	dispatch({machineId, ScriptValue::fromString("go")});
	EXPECT_TRUE(actionCalled);
}

TEST_F(FsmModuleTest, DispatchWrongFromStateReturnsFalse) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("wrong_from"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("running"),
		ScriptValue::fromString("stopped"),
		ScriptValue::fromString("stop")
	});

	auto result = dispatch({machineId, ScriptValue::fromString("stop")});
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchWithPayloadAndEvent) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("payload_dispatch"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	bool actionReceivedPayload = false;
	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("activate"),
		ScriptValue::null(),
		ScriptValue::fromCallable([&actionReceivedPayload](const std::vector<ScriptValue>& args) -> ScriptValue {
			if (!args.empty() && args[0].isObject()) {
				auto* payload = args[0].get("payload");
				if (payload && payload->isObject()) {
					actionReceivedPayload = true;
				}
			}
			return ScriptValue::null();
		})
	});

	dispatch({
		machineId,
		ScriptValue::fromString("activate"),
		ScriptValue::fromObject({{"speed", ScriptValue::fromInt(100)}})
	});
	EXPECT_TRUE(actionReceivedPayload);
	EXPECT_EQ(current({machineId}).asString(), "active");
}

TEST_F(FsmModuleTest, MultipleTransitionsChain) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("chain"),
		ScriptValue::fromString("a")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("a"),
		ScriptValue::fromString("b"),
		ScriptValue::fromString("next")
	});
	transition({
		machineId,
		ScriptValue::fromString("b"),
		ScriptValue::fromString("c"),
		ScriptValue::fromString("next")
	});

	dispatch({machineId, ScriptValue::fromString("next")});
	EXPECT_EQ(current({machineId}).asString(), "b");

	dispatch({machineId, ScriptValue::fromString("next")});
	EXPECT_EQ(current({machineId}).asString(), "c");
}

TEST_F(FsmModuleTest, StateWithCallbacks) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto state = findFsmFunction(mod, "state");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("cb_state"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	bool enterCalled = false;
	bool exitCalled = false;

	state({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromCallable([&enterCalled](const std::vector<ScriptValue>&) -> ScriptValue {
			enterCalled = true;
			return ScriptValue::null();
		}),
		ScriptValue::fromCallable([&exitCalled](const std::vector<ScriptValue>&) -> ScriptValue {
			exitCalled = true;
			return ScriptValue::null();
		})
	});

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("done"),
		ScriptValue::fromString("finish")
	});

	dispatch({machineId, ScriptValue::fromString("finish")});
	EXPECT_TRUE(exitCalled);
}

TEST_F(FsmModuleTest, StateNonexistentArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto state = findFsmFunction(mod, "state");

	auto result = state({ScriptValue::fromString("only_one")});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, TransitionInsufficientArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto transition = findFsmFunction(mod, "transition");

	auto result = transition({
		ScriptValue::fromString("id"),
		ScriptValue::fromString("from")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

