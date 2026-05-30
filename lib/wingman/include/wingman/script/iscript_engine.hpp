#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace wingman {
namespace script {

// Language-independent value type (tagged union)
struct ScriptValue {
	enum Type { Null, Bool, Int, Float, String, Array, Object, Callable };

	Type type = Null;

	bool boolVal = false;
	int64_t intVal = 0;
	double floatVal = 0.0;
	std::string strVal;
	std::vector<ScriptValue> arrayVal;
	std::unordered_map<std::string, ScriptValue> objectVal;

	// Callable support: holds a function that can be called from C++
	using CallableFunc = std::function<ScriptValue(const std::vector<ScriptValue>&)>;
	CallableFunc callableVal;

	ScriptValue() = default;

	// Factory methods
	static ScriptValue null() { return {}; }
	static ScriptValue fromBool(bool v) { ScriptValue sv; sv.type = Bool; sv.boolVal = v; return sv; }
	static ScriptValue fromInt(int64_t v) { ScriptValue sv; sv.type = Int; sv.intVal = v; return sv; }
	static ScriptValue fromFloat(double v) { ScriptValue sv; sv.type = Float; sv.floatVal = v; return sv; }
	static ScriptValue fromString(const std::string& v) { ScriptValue sv; sv.type = String; sv.strVal = v; return sv; }
	static ScriptValue fromString(std::string&& v) { ScriptValue sv; sv.type = String; sv.strVal = std::move(v); return sv; }
	static ScriptValue fromArray(std::vector<ScriptValue>&& v) { ScriptValue sv; sv.type = Array; sv.arrayVal = std::move(v); return sv; }
	static ScriptValue fromObject(std::unordered_map<std::string, ScriptValue>&& v) { ScriptValue sv; sv.type = Object; sv.objectVal = std::move(v); return sv; }
	static ScriptValue fromCallable(CallableFunc v) { ScriptValue sv; sv.type = Callable; sv.callableVal = std::move(v); return sv; }

	// Convenience access
	bool isNull() const { return type == Null; }
	bool isBool() const { return type == Bool; }
	bool isInt() const { return type == Int; }
	bool isFloat() const { return type == Float; }
	bool isString() const { return type == String; }
	bool isArray() const { return type == Array; }
	bool isObject() const { return type == Object; }
	bool isCallable() const { return type == Callable; }

	bool asBool(bool def = false) const { return type == Bool ? boolVal : def; }
	int64_t asInt(int64_t def = 0) const { return type == Int ? intVal : def; }
	double asFloat(double def = 0.0) const { return type == Float ? floatVal : (type == Int ? static_cast<double>(intVal) : def); }
	const std::string& asString(const std::string& def = "") const { return type == String ? strVal : def; }

	// Object access
	const ScriptValue* get(const std::string& key) const {
		if (type != Object) return nullptr;
		auto it = objectVal.find(key);
		return it != objectVal.end() ? &it->second : nullptr;
	}

	ScriptValue get(const std::string& key, const ScriptValue& def) const {
		if (type != Object) return def;
		auto it = objectVal.find(key);
		return it != objectVal.end() ? it->second : def;
	}

	// Array access
	const ScriptValue& at(size_t idx) const {
		static const ScriptValue s_null;
		return idx < arrayVal.size() ? arrayVal[idx] : s_null;
	}
	size_t size() const { return type == Array ? arrayVal.size() : 0; }

	// Callable invocation
	ScriptValue call(const std::vector<ScriptValue>& args = {}) const {
		if (type == Callable && callableVal) {
			return callableVal(args);
		}
		return ScriptValue::null();
	}
};

// Script function signature
using ScriptFunction = std::function<ScriptValue(const std::vector<ScriptValue>&)>;

// Module descriptor (language-independent)
struct ModuleDescriptor {
	struct FunctionEntry {
		std::string name;
		ScriptFunction func;
		std::string signature; // For documentation purposes, e.g. "x:int, y:int -> bool"

		ScriptValue operator()(const std::vector<ScriptValue>& args) const {
			return func ? func(args) : ScriptValue::null();
		}
	};

	std::string name;
	std::vector<FunctionEntry> functions;
};

// Engine configuration
struct EngineConfig {
	bool sandboxed = false;
	uint64_t memoryLimit = 100 * 1024 * 1024;   // 100MB
	uint64_t instructionLimit = 1000000;          // 1M instructions
	uint64_t timeLimitMs = 30000;                 // 30 seconds
	std::unordered_map<std::string, std::string> env;
};

// Pure virtual script engine interface
class IScriptEngine {
public:
	virtual ~IScriptEngine() = default;

	// Lifecycle
	virtual bool initialize(const EngineConfig& config = {}) = 0;
	virtual void shutdown() = 0;

	// Execution
	virtual bool executeFile(const std::string& path) = 0;
	virtual bool executeString(const std::string& code) = 0;

	// Function call
	virtual bool callFunction(const std::string& name,
	                          const std::vector<ScriptValue>& args,
	                          ScriptValue& result) = 0;

	// Module registration
	virtual void registerModule(const ModuleDescriptor& module) = 0;

	// Global variables
	virtual void setGlobal(const std::string& name, const ScriptValue& value) = 0;
	virtual ScriptValue getGlobal(const std::string& name) = 0;

	// Error handling
	virtual std::string getLastError() const = 0;

	// Engine identification
	virtual std::string getLanguageName() const = 0;
	virtual std::vector<std::string> getSupportedExtensions() const = 0;

	// Sandbox control
	virtual void enableSandbox(const EngineConfig& config) = 0;
	virtual void disableSandbox() = 0;
};

} // namespace script
} // namespace wingman
