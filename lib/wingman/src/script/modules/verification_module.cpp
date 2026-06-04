#include "wingman/script/iscript_engine.hpp"
#include "wingman/verification.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createVerificationModule() {
	ModuleDescriptor mod;
	mod.name = "verification";

	mod.functions.push_back({"totp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string secret = args[0].asString();
		int digits = args.size() > 1 ? static_cast<int>(args[1].asInt(6)) : 6;
		int period = args.size() > 2 ? static_cast<int>(args[2].asInt(30)) : 30;
		return ScriptValue::fromString(generateTOTP(secret, digits, period));
	}, "secret:string, digits:int?, period:int? -> string"});

	mod.functions.push_back({"steamGuard", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(generateSteamGuard(args[0].asString()));
	}, "secret:string -> string"});

	mod.functions.push_back({"verify", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string secret = args[0].asString();
		std::string code = args[1].asString();
		int digits = args.size() > 2 ? static_cast<int>(args[2].asInt(6)) : 6;
		int period = args.size() > 3 ? static_cast<int>(args[3].asInt(30)) : 30;
		int window = args.size() > 4 ? static_cast<int>(args[4].asInt(1)) : 1;
		return ScriptValue::fromBool(verifyTOTP(secret, code, digits, period, window));
	}, "secret:string, code:string, digits:int?, period:int?, window:int? -> bool"});

	mod.functions.push_back({"remaining", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int period = args.empty() ? 30 : static_cast<int>(args[0].asInt(30));
		return ScriptValue::fromInt(getRemainingSeconds(period));
	}, "period:int? -> int"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
