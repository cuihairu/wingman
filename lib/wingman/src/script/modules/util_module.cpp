#include "wingman/script/iscript_engine.hpp"
#include "wingman/input.hpp"
#include <chrono>
#include <iostream>

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createUtilModule() {
	ModuleDescriptor mod;
	mod.name = "util";

	mod.functions.push_back({"sleep", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Input::delay(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "ms:int -> nil"});

	mod.functions.push_back({"getTime", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto now = std::chrono::system_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()).count();
		return ScriptValue::fromInt(ms);
	}, "() -> int"});

	mod.functions.push_back({"log", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::cout << "[Wingman] " << args[0].asString() << "\n";
		return ScriptValue::null();
	}, "message:string -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
