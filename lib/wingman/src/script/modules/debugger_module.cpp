#include "wingman/script/iscript_engine.hpp"

// debugger header not yet implemented, providing stub functions

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createDebuggerModule() {
	ModuleDescriptor mod;
	mod.name = "debugger";

	mod.functions.push_back({"start", [](const std::vector<ScriptValue>&) -> ScriptValue {
		// stub: debugger 尚未实现
		return ScriptValue::fromBool(false);
	}, "() -> bool"});

	mod.functions.push_back({"stop", [](const std::vector<ScriptValue>&) -> ScriptValue {
		// stub: debugger 尚未实现
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"breakpoint", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		// 返回标识符，实际断点由 IDE 设置
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
