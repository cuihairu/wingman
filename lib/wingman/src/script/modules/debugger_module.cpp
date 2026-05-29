#include "wingman/script/iscript_engine.hpp"

// debugger header not yet implemented, providing stub functions

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createDebuggerModule() {
	ModuleDescriptor mod;
	mod.name = "debugger";

	mod.functions.push_back({"start", [](const std::vector<ScriptValue>&) -> ScriptValue {
		// stub: debugger not yet implemented
		return ScriptValue::fromBool(false);
	}, "() -> bool"});

	mod.functions.push_back({"stop", [](const std::vector<ScriptValue>&) -> ScriptValue {
		// stub: debugger not yet implemented
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"breakpoint", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		// Return identifier; actual breakpoints are set by IDE
		return ScriptValue::fromString(args[0].asString() + ":" + std::to_string(args[1].asInt()));
	}, "file:string, line:int -> string"});

	mod.functions.push_back({"breakHere", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromString("DEBUG_BREAK_HERE");
	}, "() -> string"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
