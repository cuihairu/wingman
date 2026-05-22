#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace wingman {
namespace script {

// 语言无关的值类型（tagged union）
struct ScriptValue {
	enum Type { Null, Bool, Int, Float, String, Array, Object };

	Type type = Null;

	bool boolVal = false;
	int64_t intVal = 0;
	double floatVal = 0.0;
	std::string strVal;
	std::vector<ScriptValue> arrayVal;
	std::unordered_map<std::string, ScriptValue> objectVal;

	ScriptValue() = default;

	// 工厂方法
	static ScriptValue null() { return {}; }
	static ScriptValue fromBool(bool v) { ScriptValue sv; sv.type = Bool; sv.boolVal = v; return sv; }
	static ScriptValue fromInt(int64_t v) { ScriptValue sv; sv.type = Int; sv.intVal = v; return sv; }
	static ScriptValue fromFloat(double v) { ScriptValue sv; sv.type = Float; sv.floatVal = v; return sv; }
	static ScriptValue fromString(const std::string& v) { ScriptValue sv; sv.type = String; sv.strVal = v; return sv; }
	static ScriptValue fromString(std::string&& v) { ScriptValue sv; sv.type = String; sv.strVal = std::move(v); return sv; }
	static ScriptValue fromArray(std::vector<ScriptValue>&& v) { ScriptValue sv; sv.type = Array; sv.arrayVal = std::move(v); return sv; }
	static ScriptValue fromObject(std::unordered_map<std::string, ScriptValue>&& v) { ScriptValue sv; sv.type = Object; sv.objectVal = std::move(v); return sv; }

	// 便捷访问
	bool isNull() const { return type == Null; }
	bool isBool() const { return type == Bool; }
	bool isInt() const { return type == Int; }
	bool isFloat() const { return type == Float; }
	bool isString() const { return type == String; }
	bool isArray() const { return type == Array; }
	bool isObject() const { return type == Object; }

	bool asBool(bool def = false) const { return type == Bool ? boolVal : def; }
	int64_t asInt(int64_t def = 0) const { return type == Int ? intVal : def; }
	double asFloat(double def = 0.0) const { return type == Float ? floatVal : (type == Int ? static_cast<double>(intVal) : def); }
	const std::string& asString(const std::string& def = "") const { return type == String ? strVal : def; }

	// Object 访问
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

	// Array 访问
	const ScriptValue& at(size_t idx) const {
		static const ScriptValue s_null;
		return idx < arrayVal.size() ? arrayVal[idx] : s_null;
	}
	size_t size() const { return type == Array ? arrayVal.size() : 0; }
};

// 脚本函数签名
using ScriptFunction = std::function<ScriptValue(const std::vector<ScriptValue>&)>;

// 模块描述符（语言无关）
struct ModuleDescriptor {
	struct FunctionEntry {
		std::string name;
		ScriptFunction func;
		std::string signature; // 文档用途，如 "x:int, y:int -> bool"
	};

	std::string name;
	std::vector<FunctionEntry> functions;
};

// 引擎配置
struct EngineConfig {
	bool sandboxed = true;
	uint64_t memoryLimit = 100 * 1024 * 1024;   // 100MB
	uint64_t instructionLimit = 1000000;          // 1M instructions
	uint64_t timeLimitMs = 30000;                 // 30 seconds
	std::unordered_map<std::string, std::string> env;
};

// 脚本引擎纯虚接口
class IScriptEngine {
public:
	virtual ~IScriptEngine() = default;

	// 生命周期
	virtual bool initialize(const EngineConfig& config = {}) = 0;
	virtual void shutdown() = 0;

	// 执行
	virtual bool executeFile(const std::string& path) = 0;
	virtual bool executeString(const std::string& code) = 0;

	// 函数调用
	virtual bool callFunction(const std::string& name,
	                          const std::vector<ScriptValue>& args,
	                          ScriptValue& result) = 0;

	// 模块注册
	virtual void registerModule(const ModuleDescriptor& module) = 0;

	// 全局变量
	virtual void setGlobal(const std::string& name, const ScriptValue& value) = 0;
	virtual ScriptValue getGlobal(const std::string& name) = 0;

	// 错误处理
	virtual std::string getLastError() const = 0;

	// 引擎标识
	virtual std::string getLanguageName() const = 0;
	virtual std::vector<std::string> getSupportedExtensions() const = 0;

	// 沙箱控制
	virtual void enableSandbox(const EngineConfig& config) = 0;
	virtual void disableSandbox() = 0;
};

} // namespace script
} // namespace wingman
