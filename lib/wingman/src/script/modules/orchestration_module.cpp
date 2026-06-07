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

} // namespace modules
} // namespace script
} // namespace wingman
