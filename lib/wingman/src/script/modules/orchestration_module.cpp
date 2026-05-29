#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createOrchestrationModule() {
	ModuleDescriptor mod;
	mod.name = "orchestration";

	// Orchestration module depends on server component, providing stub implementation
	mod.functions.push_back({"submit_workflow", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "workflow:{...} -> workflowId:string?"});

	mod.functions.push_back({"cancel_workflow", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(false);
	}, "workflowId:string -> bool"});

	mod.functions.push_back({"get_workflow", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "workflowId:string -> workflow?"});

	mod.functions.push_back({"get_all_workflows", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromArray({});
	}, "() -> {workflow}"});

	return mod;
}

ModuleDescriptor createTeamModule() {
	ModuleDescriptor mod;
	mod.name = "team";

	mod.functions.push_back({"register", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "username:string? -> clientId:string?"});

	mod.functions.push_back({"join", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(false);
	}, "teamId:string -> bool"});

	mod.functions.push_back({"leave", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(false);
	}, "() -> bool"});

	mod.functions.push_back({"send", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(false);
	}, "action:string, data? -> bool"});

	mod.functions.push_back({"poll", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromArray({});
	}, "() -> {message}"});

	mod.functions.push_back({"members", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromArray({});
	}, "() -> {member}"});

	mod.functions.push_back({"info", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject({});
	}, "() -> {team_id,my_id,...}"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
