#include "wingman/script/iscript_engine.hpp"
#include "wingman/config.hpp"

namespace wingman {
namespace script {
namespace modules {

// Module-level shared ConfigManager instance
static ConfigManager& getConfigManager() {
	static ConfigManager instance;
	return instance;
}

ModuleDescriptor createConfigModule() {
	ModuleDescriptor mod;
	mod.name = "config";

	mod.functions.push_back({"get", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto val = getConfigManager().get(args[0].asString());
		if (val) return ScriptValue::fromString(*val);
		return ScriptValue::null();
	}, "key:string -> string?"});

	mod.functions.push_back({"set", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		getConfigManager().set(args[0].asString(), args[1].asString());
		return ScriptValue::null();
	}, "key:string, value:string -> nil"});

	mod.functions.push_back({"remove", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(getConfigManager().remove(args[0].asString()));
	}, "key:string -> bool"});

	mod.functions.push_back({"save", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(getConfigManager().save());
	}, "() -> bool"});

	mod.functions.push_back({"load", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(getConfigManager().load());
	}, "() -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
