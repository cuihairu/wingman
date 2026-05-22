#include "wingman/script/iscript_engine.hpp"
#include "wingman/security.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createSecurityModule() {
	ModuleDescriptor mod;
	mod.name = "security";

	mod.functions.push_back({"getRandomDelay", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(SecurityManager::instance().getRandomDelay());
	}, "() -> int"});

	mod.functions.push_back({"getRandomOffset", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto [x, y] = SecurityManager::instance().getRandomOffset();
		return ScriptValue::fromArray({ScriptValue::fromFloat(x), ScriptValue::fromFloat(y)});
	}, "() -> float, float"});

	mod.functions.push_back({"isDebuggerPresent", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(SecurityManager::instance().isDebuggerPresent());
	}, "() -> bool"});

	mod.functions.push_back({"isRunningInVM", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(SecurityManager::instance().isRunningInVM());
	}, "() -> bool"});

	mod.functions.push_back({"verifyIntegrity", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(SecurityManager::instance().verifyIntegrity());
	}, "() -> bool"});

	mod.functions.push_back({"hashString", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(SecurityManager::hashString(args[0].asString()));
	}, "str:string -> string"});

	mod.functions.push_back({"encryptString", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(SecurityManager::encryptString(args[0].asString(), args[1].asString()));
	}, "str:string, key:string -> string"});

	mod.functions.push_back({"decryptString", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(SecurityManager::decryptString(args[0].asString(), args[1].asString()));
	}, "str:string, key:string -> string"});

	mod.functions.push_back({"generateRandomString", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(SecurityManager::generateRandomString(static_cast<int>(args[0].asInt())));
	}, "length:int -> string"});

	mod.functions.push_back({"filterSensitive", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(SecurityManager::filterSensitive(args[0].asString()));
	}, "str:string -> string"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
