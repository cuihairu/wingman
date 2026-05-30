#include "wingman/script/iscript_engine.hpp"
#include "wingman/verification.hpp"

namespace wingman {
namespace script {
namespace modules {

// Module-level shared VerificationManager instance
static VerificationManager& getVerificationManager() {
	static VerificationManager instance;
	return instance;
}

ModuleDescriptor createVerificationModule() {
	ModuleDescriptor mod;
	mod.name = "verification";

	mod.functions.push_back({"totp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		TOTPConfig config;
		config.type = TOTPType::Google;
		config.digits = 6;
		config.secret = args[0].asString();
		if (args.size() > 1) config.digits = static_cast<int>(args[1].asInt(6));
		if (args.size() > 2) config.period = static_cast<int>(args[2].asInt(30));
		return ScriptValue::fromString(getVerificationManager().generateTOTP(config));
	}, "secret:string, digits:int?, period:int? -> string"});

	mod.functions.push_back({"steamGuard", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromString(getVerificationManager().generateSteamGuard(args[0].asString()));
	}, "secret:string -> string"});

	mod.functions.push_back({"saveTOTP", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		TOTPConfig config;
		config.type = TOTPType::Google;
		config.digits = 6;
		config.secret = args[1].asString();
		if (args.size() > 2) config.digits = static_cast<int>(args[2].asInt(6));
		if (args.size() > 3) config.period = static_cast<int>(args[3].asInt(30));
		return ScriptValue::fromBool(getVerificationManager().saveTOTP(args[0].asString(), config));
	}, "account:string, secret:string, digits:int?, period:int? -> bool"});

	mod.functions.push_back({"loadTOTP", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto configOpt = getVerificationManager().loadTOTP(args[0].asString());
		if (configOpt) return ScriptValue::fromString(getVerificationManager().generateTOTP(*configOpt));
		return ScriptValue::null();
	}, "account:string -> string?"});

	mod.functions.push_back({"listTOTP", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto accounts = getVerificationManager().listTOTPAccounts();
		std::vector<ScriptValue> arr;
		for (const auto& a : accounts) arr.push_back(ScriptValue::fromString(a));
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {string}"});

	mod.functions.push_back({"removeTOTP", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		return ScriptValue::fromBool(getVerificationManager().removeTOTP(args[0].asString()));
	}, "account:string -> bool"});

	mod.functions.push_back({"getRemaining", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		TOTPConfig config;
		config.type = TOTPType::Google;
		config.digits = 6;
		config.secret = args[0].asString();
		if (args.size() > 1) config.period = static_cast<int>(args[1].asInt(30));
		return ScriptValue::fromInt(getVerificationManager().getRemainingSeconds(config));
	}, "secret:string, period:int? -> int"});

	mod.functions.push_back({"verify", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string account = args[0].asString();
		std::string code = args[1].asString();
		int window = args.size() > 2 ? static_cast<int>(args[2].asInt(1)) : 1;
		return ScriptValue::fromBool(getVerificationManager().verifyTOTP(account, code, window));
	}, "account:string, code:string, window:int? -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
