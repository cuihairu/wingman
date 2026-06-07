#include "wingman/script/iscript_engine.hpp"
#include "wingman/crypt.hpp"
#include <spdlog/spdlog.h>

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createCryptoModule() {
	ModuleDescriptor mod;
	mod.name = "crypto";

	// AES-256-GCM encryption
	mod.functions.push_back({"encryptAES", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			spdlog::error("[Crypto] encryptAES requires (plaintext: string, password: string, salt?: string)");
			return ScriptValue::fromString("");
		}
		std::string plaintext = args[0].asString();
		std::string password = args[1].asString();
		std::string salt = args.size() > 2 && args[2].isString() ? args[2].asString() : "";
		return ScriptValue::fromString(crypt::encryptAES(plaintext, password, salt));
	}, "plaintext:string, password:string, salt:string? -> base64_string"});

	mod.functions.push_back({"decryptAES", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			spdlog::error("[Crypto] decryptAES requires (ciphertext: base64_string, password: string)");
			return ScriptValue::fromString("");
		}
		std::string ciphertext = args[0].asString();
		std::string password = args[1].asString();
		return ScriptValue::fromString(crypt::decryptAES(ciphertext, password));
	}, "ciphertext:base64_string, password:string -> plaintext"});

	// Key derivation
	mod.functions.push_back({"deriveKey", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
			spdlog::error("[Crypto] deriveKey requires (password: string, salt: hex_string, iterations?: int, keyLen?: int)");
			return ScriptValue::fromString("");
		}
		std::string password = args[0].asString();
		std::string salt = args[1].asString();
		int iterations = args.size() > 2 && args[2].isInt() ? static_cast<int>(args[2].asInt()) : 100000;
		size_t keyLen = args.size() > 3 && args[3].isInt() ? static_cast<size_t>(args[3].asInt()) : 32;
		return ScriptValue::fromString(crypt::deriveKey(password, salt, iterations, keyLen));
	}, "password:string, salt:hex_string, iterations?:int, keyLen?:int -> hex_string"});

	// Salt generation
	mod.functions.push_back({"generateSalt", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		size_t length = args.size() > 0 && args[0].isInt() ? static_cast<size_t>(args[0].asInt()) : 16;
		return ScriptValue::fromString(crypt::generateSalt(length));
	}, "length?:int -> hex_string"});

	// Hash functions
	mod.functions.push_back({"sha256", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) {
			return ScriptValue::fromString("");
		}
		return ScriptValue::fromString(crypt::sha256(args[0].asString()));
	}, "data:string -> hex_string"});

	mod.functions.push_back({"sha512", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) {
			return ScriptValue::fromString("");
		}
		return ScriptValue::fromString(crypt::sha512(args[0].asString()));
	}, "data:string -> hex_string"});

	// Base64 encoding/decoding
	mod.functions.push_back({"base64Encode", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) {
			return ScriptValue::fromString("");
		}
		std::string data = args[0].asString();
		std::vector<uint8_t> bytes(data.begin(), data.end());
		return ScriptValue::fromString(crypt::base64Encode(bytes));
	}, "data:string -> base64_string"});

	mod.functions.push_back({"base64Decode", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) {
			return ScriptValue::fromString("");
		}
		auto bytes = crypt::base64Decode(args[0].asString());
		return ScriptValue::fromString(std::string(bytes.begin(), bytes.end()));
	}, "base64_string:string -> data"});

	// Hex encoding/decoding
	mod.functions.push_back({"hexEncode", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) {
			return ScriptValue::fromString("");
		}
		std::string data = args[0].asString();
		std::vector<uint8_t> bytes(data.begin(), data.end());
		return ScriptValue::fromString(crypt::bytesToHex(bytes));
	}, "data:string -> hex_string"});

	mod.functions.push_back({"hexDecode", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isString()) {
			return ScriptValue::fromString("");
		}
		auto bytes = crypt::hexToBytes(args[0].asString());
		return ScriptValue::fromString(std::string(bytes.begin(), bytes.end()));
	}, "hex_string:string -> data"});

	// Random bytes
	mod.functions.push_back({"randomBytes", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		size_t length = args.size() > 0 && args[0].isInt() ? static_cast<size_t>(args[0].asInt()) : 16;
		auto bytes = crypt::randomBytes(length);
		return ScriptValue::fromString(std::string(bytes.begin(), bytes.end()));
	}, "length?:int -> bytes_string"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
