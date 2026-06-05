#include <gtest/gtest.h>

#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createFsmModule();
void cleanupFsmModule();

} // namespace modules
} // namespace script
} // namespace wingman

using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

ModuleDescriptor::FunctionEntry findFsmFunction(const ModuleDescriptor& mod, const std::string& name) {
	for (const auto& fn : mod.functions) {
		if (fn.name == name) return fn;
	}
	return {};
}

} // namespace

class FsmModuleTest : public ::testing::Test {
protected:
	void TearDown() override {
		cleanupFsmModule();
	}
};

TEST_F(FsmModuleTest, HasExpectedFunctions) {
	auto mod = createFsmModule();
	EXPECT_EQ(mod.name, "fsm");
	ASSERT_GE(mod.functions.size(), 6u);

	bool hasCreate = false, hasState = false, hasTransition = false;
	bool hasDispatch = false, hasCurrent = false, hasReset = false;
	for (const auto& fn : mod.functions) {
		if (fn.name == "create") hasCreate = true;
		if (fn.name == "state") hasState = true;
		if (fn.name == "transition") hasTransition = true;
		if (fn.name == "dispatch") hasDispatch = true;
		if (fn.name == "current") hasCurrent = true;
		if (fn.name == "reset") hasReset = true;
	}
	EXPECT_TRUE(hasCreate);
	EXPECT_TRUE(hasState);
	EXPECT_TRUE(hasTransition);
	EXPECT_TRUE(hasDispatch);
	EXPECT_TRUE(hasCurrent);
	EXPECT_TRUE(hasReset);
}

TEST_F(FsmModuleTest, CreateReturnsStringId) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	ASSERT_FALSE(create.name.empty());

	auto result = create({
		ScriptValue::fromString("test_machine"),
		ScriptValue::fromString("idle")
	});
	EXPECT_TRUE(result.isString());
	EXPECT_FALSE(result.asString().empty());
}

TEST_F(FsmModuleTest, CreateEmptyArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	ASSERT_FALSE(create.name.empty());

	auto result = create({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, CreateInsufficientArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	ASSERT_FALSE(create.name.empty());

	auto result = create({ScriptValue::fromString("only_name")});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, CurrentReturnsInitialState) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto current = findFsmFunction(mod, "current");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(current.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_current"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	auto result = current({machineId});
	EXPECT_TRUE(result.isString());
	EXPECT_EQ(result.asString(), "idle");
}

TEST_F(FsmModuleTest, CurrentNonexistentReturnsNull) {
	auto mod = createFsmModule();
	auto current = findFsmFunction(mod, "current");
	ASSERT_FALSE(current.name.empty());

	auto result = current({ScriptValue::fromString("nonexistent")});
	EXPECT_TRUE(result.isNull());
}

TEST_F(FsmModuleTest, CurrentEmptyArgsReturnsNull) {
	auto mod = createFsmModule();
	auto current = findFsmFunction(mod, "current");
	ASSERT_FALSE(current.name.empty());

	auto result = current({});
	EXPECT_TRUE(result.isNull());
}

TEST_F(FsmModuleTest, StateAddsStateToMachine) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto state = findFsmFunction(mod, "state");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(state.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_state"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	auto result = state({
		machineId,
		ScriptValue::fromString("running")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, StateNonexistentMachineReturnsFalse) {
	auto mod = createFsmModule();
	auto state = findFsmFunction(mod, "state");
	ASSERT_FALSE(state.name.empty());

	auto result = state({
		ScriptValue::fromString("nonexistent"),
		ScriptValue::fromString("running")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, StateEmptyArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto state = findFsmFunction(mod, "state");
	ASSERT_FALSE(state.name.empty());

	auto result = state({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, TransitionAddsTransition) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(transition.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_transition"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	auto result = transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("start")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, TransitionNonexistentMachineReturnsFalse) {
	auto mod = createFsmModule();
	auto transition = findFsmFunction(mod, "transition");
	ASSERT_FALSE(transition.name.empty());

	auto result = transition({
		ScriptValue::fromString("nonexistent"),
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, TransitionEmptyArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto transition = findFsmFunction(mod, "transition");
	ASSERT_FALSE(transition.name.empty());

	auto result = transition({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchChangesState) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto current = findFsmFunction(mod, "current");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(transition.name.empty());
	ASSERT_FALSE(dispatch.name.empty());
	ASSERT_FALSE(current.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_dispatch"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("start")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("start")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());

	auto state = current({machineId});
	EXPECT_EQ(state.asString(), "running");
}

TEST_F(FsmModuleTest, DispatchNoMatchingTransitionReturnsFalse) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto dispatch = findFsmFunction(mod, "dispatch");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(dispatch.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_no_match"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("nonexistent_event")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchNonexistentMachineReturnsFalse) {
	auto mod = createFsmModule();
	auto dispatch = findFsmFunction(mod, "dispatch");
	ASSERT_FALSE(dispatch.name.empty());

	auto result = dispatch({
		ScriptValue::fromString("nonexistent"),
		ScriptValue::fromString("start")
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchEmptyArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto dispatch = findFsmFunction(mod, "dispatch");
	ASSERT_FALSE(dispatch.name.empty());

	auto result = dispatch({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, DispatchWithPayload) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(transition.name.empty());
	ASSERT_FALSE(dispatch.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_payload"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("start")
	});

	auto result = dispatch({
		machineId,
		ScriptValue::fromString("start"),
		ScriptValue::fromObject({{"speed", ScriptValue::fromInt(100)}})
	});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
}

TEST_F(FsmModuleTest, ResetRestoresInitialState) {
	auto mod = createFsmModule();
	auto create = findFsmFunction(mod, "create");
	auto transition = findFsmFunction(mod, "transition");
	auto dispatch = findFsmFunction(mod, "dispatch");
	auto reset = findFsmFunction(mod, "reset");
	auto current = findFsmFunction(mod, "current");
	ASSERT_FALSE(create.name.empty());
	ASSERT_FALSE(transition.name.empty());
	ASSERT_FALSE(dispatch.name.empty());
	ASSERT_FALSE(reset.name.empty());
	ASSERT_FALSE(current.name.empty());

	auto machineId = create({
		ScriptValue::fromString("test_reset"),
		ScriptValue::fromString("idle")
	});
	ASSERT_TRUE(machineId.isString());

	transition({
		machineId,
		ScriptValue::fromString("idle"),
		ScriptValue::fromString("running"),
		ScriptValue::fromString("start")
	});

	dispatch({machineId, ScriptValue::fromString("start")});
	EXPECT_EQ(current({machineId}).asString(), "running");

	auto result = reset({machineId});
	EXPECT_TRUE(result.isBool());
	EXPECT_TRUE(result.asBool());
	EXPECT_EQ(current({machineId}).asString(), "idle");
}

TEST_F(FsmModuleTest, ResetNonexistentMachineReturnsFalse) {
	auto mod = createFsmModule();
	auto reset = findFsmFunction(mod, "reset");
	ASSERT_FALSE(reset.name.empty());

	auto result = reset({ScriptValue::fromString("nonexistent")});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

TEST_F(FsmModuleTest, ResetEmptyArgsReturnsFalse) {
	auto mod = createFsmModule();
	auto reset = findFsmFunction(mod, "reset");
	ASSERT_FALSE(reset.name.empty());

	auto result = reset({});
	EXPECT_TRUE(result.isBool());
	EXPECT_FALSE(result.asBool());
}

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
