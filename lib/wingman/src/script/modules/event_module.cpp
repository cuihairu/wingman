#include "wingman/event.hpp"
#include "wingman/script/iscript_engine.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace wingman {
namespace script {
namespace modules {

namespace {

nlohmann::json toJson(const ScriptValue& value) {
	switch (value.type) {
	case ScriptValue::Null:
		return nullptr;
	case ScriptValue::Bool:
		return value.boolVal;
	case ScriptValue::Int:
		return value.intVal;
	case ScriptValue::Float:
		return value.floatVal;
	case ScriptValue::String:
		return value.strVal;
	case ScriptValue::Array: {
		nlohmann::json arr = nlohmann::json::array();
		for (const auto& item : value.arrayVal) {
			arr.push_back(toJson(item));
		}
		return arr;
	}
	case ScriptValue::Object: {
		nlohmann::json obj = nlohmann::json::object();
		for (const auto& [key, item] : value.objectVal) {
			obj[key] = toJson(item);
		}
		return obj;
	}
	case ScriptValue::Callable:
		return nullptr;
	}
	return nullptr;
}

ScriptValue fromJson(const nlohmann::json& value) {
	if (value.is_null()) return ScriptValue::null();
	if (value.is_boolean()) return ScriptValue::fromBool(value.get<bool>());
	if (value.is_number_integer()) return ScriptValue::fromInt(value.get<int64_t>());
	if (value.is_number_unsigned()) return ScriptValue::fromInt(static_cast<int64_t>(value.get<uint64_t>()));
	if (value.is_number_float()) return ScriptValue::fromFloat(value.get<double>());
	if (value.is_string()) return ScriptValue::fromString(value.get<std::string>());
	if (value.is_array()) {
		std::vector<ScriptValue> arr;
		arr.reserve(value.size());
		for (const auto& item : value) {
			arr.push_back(fromJson(item));
		}
		return ScriptValue::fromArray(std::move(arr));
	}
	if (value.is_object()) {
		std::unordered_map<std::string, ScriptValue> obj;
		for (auto it = value.begin(); it != value.end(); ++it) {
			obj[it.key()] = fromJson(it.value());
		}
		return ScriptValue::fromObject(std::move(obj));
	}
	return ScriptValue::null();
}

ScriptValue fromEventMessage(const EventMessage& msg) {
	return ScriptValue::fromObject({
		{"type", ScriptValue::fromString(msg.type)},
		{"source", ScriptValue::fromString(msg.source)},
		{"correlationId", ScriptValue::fromString(msg.correlationId)},
		{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))},
		{"priority", ScriptValue::fromInt(msg.priority)},
		{"payload", fromJson(msg.payload)}
	});
}

} // namespace

ModuleDescriptor createEventModule() {
	ModuleDescriptor mod;
	mod.name = "event";

	// on(type: string, callback: function, name?: string) -> subscriptionId
	mod.functions.push_back({"on", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string type = args[0].asString();
		if (!args[1].isCallable()) return ScriptValue::fromBool(false);

		ScriptValue::CallableFunc callback = args[1].callableVal;
		std::string name = args.size() > 2 && args[2].isString() ? args[2].asString() : "";

		// Wrap the callback to convert EventMessage to ScriptValue
		uint64_t id = EventHub::instance().subscribe(type, [callback, type](const EventMessage& msg) {
			std::vector<ScriptValue> cbArgs;
			cbArgs.push_back(ScriptValue::fromObject({
				{"type", ScriptValue::fromString(msg.type)},
				{"source", ScriptValue::fromString(msg.source)},
				{"correlationId", ScriptValue::fromString(msg.correlationId)},
				{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))},
				{"priority", ScriptValue::fromInt(msg.priority)},
				{"payload", fromJson(msg.payload)}
			}));
			// Wrap callback in try-catch to prevent exception propagation
			try {
				callback(cbArgs);
			} catch (const std::exception& e) {
				spdlog::error("[event] on callback exception for '{}': {}", type, e.what());
			} catch (...) {
				spdlog::error("[event] on callback unknown exception for '{}'", type);
			}
		}, name, false);

		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "type:string, callback:function, name?:string -> subscriptionId:int"});

	// once(type: string, callback: function) -> subscriptionId
	mod.functions.push_back({"once", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);

		std::string type = args[0].asString();
		if (!args[1].isCallable()) return ScriptValue::fromBool(false);

		ScriptValue::CallableFunc callback = args[1].callableVal;

		uint64_t id = EventHub::instance().subscribe(type, [callback, type](const EventMessage& msg) {
			std::vector<ScriptValue> cbArgs;
			cbArgs.push_back(ScriptValue::fromObject({
				{"type", ScriptValue::fromString(msg.type)},
				{"source", ScriptValue::fromString(msg.source)},
				{"correlationId", ScriptValue::fromString(msg.correlationId)},
				{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(msg.timestamp))},
				{"priority", ScriptValue::fromInt(msg.priority)},
				{"payload", fromJson(msg.payload)}
			}));
			// Wrap callback in try-catch to prevent exception propagation
			try {
				callback(cbArgs);
			} catch (const std::exception& e) {
				spdlog::error("[event] on callback exception for '{}': {}", type, e.what());
			} catch (...) {
				spdlog::error("[event] on callback unknown exception for '{}'", type);
			}
		}, "", true);

		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "type:string, callback:function -> subscriptionId:int"});

	mod.functions.push_back({"emit", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);

		std::string type = args[0].asString();
		if (type.empty()) {
			spdlog::warn("[event] emit: event type cannot be empty");
			return ScriptValue::fromBool(false);
		}

		nlohmann::json payload = args.size() > 1 ? toJson(args[1]) : nlohmann::json::object();
		std::string source;
		std::string correlationId;
		int priority = 0;

		if (args.size() > 2 && args[2].isObject()) {
			if (auto* value = args[2].get("source")) source = value->asString();
			if (auto* value = args[2].get("correlationId")) correlationId = value->asString();
			if (auto* value = args[2].get("priority")) priority = static_cast<int>(value->asInt());
		}

		EventHub::instance().emit(type, payload, source, correlationId, priority);
		return ScriptValue::fromBool(true);
	}, "type:string, payload?:object, meta?:{source?,correlationId?,priority?} -> bool"});

	mod.functions.push_back({"off", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		if (args[0].isString()) {
			EventHub::instance().unsubscribe(args[0].asString());
		} else {
			EventHub::instance().unsubscribe(static_cast<uint64_t>(args[0].asInt()));
		}
		return ScriptValue::fromBool(true);
	}, "subscription:int|string -> bool"});

	mod.functions.push_back({"clear", [](const std::vector<ScriptValue>&) -> ScriptValue {
		EventHub::instance().clear();
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"message", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		EventMessage msg;
		if (!args.empty()) msg.type = args[0].asString();
		if (args.size() > 1) msg.payload = toJson(args[1]);
		if (args.size() > 2 && args[2].isObject()) {
			if (auto* value = args[2].get("source")) msg.source = value->asString();
			if (auto* value = args[2].get("correlationId")) msg.correlationId = value->asString();
			if (auto* value = args[2].get("priority")) msg.priority = static_cast<int>(value->asInt());
		}
		return fromEventMessage(msg);
	}, "type:string, payload?:object, meta?:object -> event"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
