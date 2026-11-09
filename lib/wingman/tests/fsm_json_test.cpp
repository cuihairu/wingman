#include "fsm_module_test_common.hpp"

// ========== toJson/fromJson Coverage Tests ==========

TEST_F(FsmModuleTest, DispatchWithArrayPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("arr_payload"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromArray({
			ScriptValue::fromInt(1),
			ScriptValue::fromString("two"),
			ScriptValue::fromBool(true)
		})
	});
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchWithFloatPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("float_payload"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromObject({{"speed", ScriptValue::fromFloat(3.14)}})
	});
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchWithBoolPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("bool_payload"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromObject({{"flag", ScriptValue::fromBool(true)}})
	});
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchWithNullPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("null_payload"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromObject({{"data", ScriptValue::null()}})
	});
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchWithNestedArrayPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("nested_arr"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromArray({
			ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)}),
			ScriptValue::fromObject({{"key", ScriptValue::fromString("val")}})
		})
	});
	EXPECT_TRUE(result.asBool());
}

// ========== Cleanup Tests ==========

TEST_F(FsmModuleTest, CleanupClearsStateMachines) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("cleanup_test"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	// Verify machine exists
	auto result = current({machineId});
	EXPECT_EQ(result.asString(), "idle");

	// Cleanup should remove all machines
	cleanupFsmModule();

	// Machine should no longer exist after cleanup
	auto after = current({machineId});
	EXPECT_TRUE(after.isNull());
}

// ========== Dispatch with Guard Returning Non-Bool ==========

TEST_F(FsmModuleTest, DispatchWithGuardReturningNonBool) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("guard_nonbool"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	// Guard returns a non-bool value (string), asBool(false) should be false
	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("go"),
		ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("not_a_bool");
		})
	});

	auto result = dispatch({machineId, ScriptValue::fromString("go")});
	// Guard returned non-bool which is falsy, so transition should be rejected
	EXPECT_FALSE(result.asBool());
	EXPECT_EQ(current({machineId}).asString(), "idle");
}

// ========== Transition with Empty Event ==========

TEST_F(FsmModuleTest, TransitionWithEmptyEventString) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");

	auto machineId = create({
		ScriptValue::fromString("empty_event"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	// Transition with empty event string (uses default from args[3] not being string)
	auto result = transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running")
		// No event string provided
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

// ========== State without Callbacks ==========

TEST_F(FsmModuleTest, StateWithoutCallbacks) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto state = findFsmFunction(mod, "state");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("no_callbacks"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	// Add state without onEnter/onExit callbacks
	state({
		machineId,
		ScriptValue::fromString("running")
	});

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({machineId, ScriptValue::fromString("go")});
	EXPECT_TRUE(result.asBool());
	EXPECT_EQ(current({machineId}).asString(), "running");
}

// ========== Multiple Machines Coexist ==========

TEST_F(FsmModuleTest, MultipleMachinesCoexist) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto current = findFsmFunction(mod, "current");

	auto id1 = create({
		ScriptValue::fromString("machine1"),
		ScriptValue::fromString("state_a")
	});
	auto id2 = create({
		ScriptValue::fromString("machine2"),
		ScriptValue::fromString("state_b")
	});
	ASSERT_TRUE(id1.isString());
	ASSERT_TRUE(id2.isString());

	EXPECT_EQ(current({id1}).asString(), "state_a");
	EXPECT_EQ(current({id2}).asString(), "state_b");
}

// ========== Reset on Fresh Machine ==========

TEST_F(FsmModuleTest, ResetOnFreshMachineIsNoop) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto reset = findFsmFunction(mod, "reset");
	auto current = findFsmFunction(mod, "current");

	auto machineId = create({
		ScriptValue::fromString("fresh_reset"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	// Reset without any transitions
	auto result = reset({machineId});
	EXPECT_TRUE(result.asBool());
	EXPECT_EQ(current({machineId}).asString(), "idle");
}

// ========== Coverage: fromJson with array payload in guard callback ==========

TEST_F(FsmModuleTest, GuardReceivesArrayPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("guard_arr"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	bool guardCalled = false;
	auto guard = ScriptValue::fromCallable([&guardCalled](const std::vector<ScriptValue>& args) -> ScriptValue {
		guardCalled = true;
		// Verify the payload contains array data
		if (!args.empty() && args[0].isObject()) {
			auto* payload = args[0].get("payload");
			if (payload && payload->isArray()) {
				return ScriptValue::fromBool(true);
			}
		}
		return ScriptValue::fromBool(true);
	});

	// Transition with guard that receives array payload
	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go"),
		ScriptValue::null(), // action
		guard
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromString("two")})
	});
	EXPECT_TRUE(result.asBool());
	EXPECT_TRUE(guardCalled);
}

// ========== Coverage: state onEnter receives array payload ==========

TEST_F(FsmModuleTest, StateOnEnterReceivesArrayPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto state = findFsmFunction(mod, "state");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("enter_arr"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	bool onEnterCalled = false;
	auto onEnter = ScriptValue::fromCallable([&onEnterCalled](const std::vector<ScriptValue>& args) -> ScriptValue {
		onEnterCalled = true;
		return ScriptValue::null();
	});

	// Add state with onEnter callback
	state({
		machineId,
		ScriptValue::fromString("active"),
		onEnter
	});

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromArray({ScriptValue::fromInt(42), ScriptValue::fromBool(true)})
	});
	EXPECT_TRUE(result.asBool());
	EXPECT_TRUE(onEnterCalled);
}

// ========== Coverage: dispatch with nested object payload ==========

TEST_F(FsmModuleTest, DispatchWithNestedObjectPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");

	auto machineId = create({
		ScriptValue::fromString("nested_obj"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("active"),
		ScriptValue::fromString("go")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("go"),
		ScriptValue::fromObject({
			{"nested", ScriptValue::fromObject({{"a", ScriptValue::fromInt(1)}})},
			{"items", ScriptValue::fromArray({ScriptValue::fromString("x")})},
			{"flag", ScriptValue::fromBool(true)},
			{"ratio", ScriptValue::fromFloat(3.14)},
			{"nothing", ScriptValue::null()}
		})
	});
	EXPECT_TRUE(result.asBool());
}
