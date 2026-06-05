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
