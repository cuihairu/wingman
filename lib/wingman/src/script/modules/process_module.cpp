#include "wingman/script/iscript_engine.hpp"
#include "wingman/process.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createProcessModule() {
	ModuleDescriptor mod;
	mod.name = "process";

	mod.functions.push_back({"find", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		ProcessId pid = Process::find(args[0].asString());
		if (pid) {
			return ScriptValue::fromArray({ScriptValue::fromInt(pid), ScriptValue::fromBool(true)});
		}
		return ScriptValue::fromArray({ScriptValue::null(), ScriptValue::fromBool(false)});
	}, "name:string -> pid:int, found:bool"});

	mod.functions.push_back({"start", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string path = args[0].asString();
		std::string cmdArgs = args.size() > 1 ? args[1].asString() : "";
		std::string workingDir = args.size() > 2 ? args[2].asString() : "";
		ProcessId pid = Process::start(path, cmdArgs, workingDir);
		return ScriptValue::fromInt(pid);
	}, "path:string, args:string?, workdir:string? -> pid:int"});

	mod.functions.push_back({"wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		ProcessId pid = args[0].asInt();
		int timeout = args.size() > 1 ? args[1].asInt(0) : 0;
		return ScriptValue::fromBool(Process::wait(pid, timeout));
	}, "pid:int, timeout:int? -> bool"});

	mod.functions.push_back({"terminate", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		ProcessId pid = args[0].asInt();
		bool force = args.size() > 1 ? args[1].asBool() : false;
		return ScriptValue::fromBool(Process::terminate(pid, force));
	}, "pid:int, force:bool? -> bool"});

	mod.functions.push_back({"exists", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(Process::exists(args[0].asInt()));
	}, "pid:int -> bool"});

	mod.functions.push_back({"waitFor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args[0].asString();
		int timeout = args.size() > 1 ? args[1].asInt(5000) : 5000;
		return ScriptValue::fromBool(Process::waitFor(name, timeout));
	}, "name:string, timeout:int? -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
