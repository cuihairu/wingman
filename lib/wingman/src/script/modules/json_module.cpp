#include "wingman/script/iscript_engine.hpp"
#include "wingman/json.hpp"

namespace wingman {
namespace script {
namespace modules {

static ScriptValue jsonToScriptValue(const JsonValue& jv);
static JsonValue scriptToJson(const ScriptValue& sv);

static ScriptValue jsonToScriptValue(const JsonValue& jv) {
	switch (jv.type()) {
	case JsonType::Null: return ScriptValue::null();
	case JsonType::Boolean: return ScriptValue::fromBool(jv.asBool());
	case JsonType::Number: {
		// 尝试作为整数返回，若非整数则返回浮点
		double d = jv.asDouble();
		int64_t i = jv.asInt64();
		if (d == static_cast<double>(i) && i > -1000000000 && i < 1000000000) {
			return ScriptValue::fromInt(i);
		}
		return ScriptValue::fromFloat(d);
	}
	case JsonType::String: return ScriptValue::fromString(jv.asString());
	case JsonType::Array: {
		std::vector<ScriptValue> arr;
		for (size_t i = 0; i < jv.size(); ++i) {
			arr.push_back(jsonToScriptValue(jv.at(i)));
		}
		return ScriptValue::fromArray(std::move(arr));
	}
	case JsonType::Object: {
		std::unordered_map<std::string, ScriptValue> obj;
		auto keys = jv.keys();
		for (const auto& k : keys) {
			obj[k] = jsonToScriptValue(jv.get(k));
		}
		return ScriptValue::fromObject(std::move(obj));
	}
	default: return ScriptValue::null();
	}
}

static JsonValue scriptToJson(const ScriptValue& sv) {
	switch (sv.type) {
	case ScriptValue::Null: return JsonValue();
	case ScriptValue::Bool: return JsonValue(sv.boolVal);
	case ScriptValue::Int: return JsonValue(sv.intVal);
	case ScriptValue::Float: return JsonValue(sv.floatVal);
	case ScriptValue::String: return JsonValue(sv.strVal);
	case ScriptValue::Array: {
		JsonValue arr = JsonValue::array();
		for (const auto& v : sv.arrayVal) arr.push(scriptToJson(v));
		return arr;
	}
	case ScriptValue::Object: {
		JsonValue obj = JsonValue::object();
		for (const auto& [k, v] : sv.objectVal) obj.set(k, scriptToJson(v));
		return obj;
	}
	default: return JsonValue();
	}
}

ModuleDescriptor createJsonModule() {
	ModuleDescriptor mod;
	mod.name = "json";

	mod.functions.push_back({"decode", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string str = args[0].asString();
		JsonValue result = JsonValue::parse(str);
		return jsonToScriptValue(result);
	}, "jsonString:string -> value"});

	mod.functions.push_back({"encode", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		JsonValue jv = scriptToJson(args[0]);
		int indent = args.size() > 1 ? static_cast<int>(args[1].asInt(-1)) : -1;
		return ScriptValue::fromString(jv.dump(indent));
	}, "value, indent:int? -> string"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
