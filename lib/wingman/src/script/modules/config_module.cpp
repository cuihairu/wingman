#include "wingman/script/iscript_engine.hpp"
#include "wingman/config.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <string>

namespace wingman {
namespace script {
namespace modules {

// Module-level shared ConfigManager instance
// WINGMAN_CONFIG_DIR 可重定向配置目录（测试用，避免写穿仓库真实配置；运行时未设置时保持默认）
static ConfigManager& getConfigManager() {
	static ConfigManager instance([] {
		if (const char* dir = std::getenv("WINGMAN_CONFIG_DIR")) {
			if (*dir != '\0') return std::string(dir);
		}
		return std::string("config");
	}());
	return instance;
}

static std::string unwrapConfigString(const std::string& value) {
	try {
		auto json = nlohmann::json::parse(value);
		if (json.is_string()) {
			return json.get<std::string>();
		}
	} catch (...) {
	}
	return value;
}

ModuleDescriptor createConfigModule() {
	ModuleDescriptor mod;
	mod.name = "config";

	mod.functions.push_back({"get", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto val = getConfigManager().get(args[0].asString());
		if (val) return ScriptValue::fromString(unwrapConfigString(*val));
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
