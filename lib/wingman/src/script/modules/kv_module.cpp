#include "wingman/script/iscript_engine.hpp"
#include "wingman/kvstore.hpp"

namespace wingman {
namespace script {
namespace modules {

static KeyValueStore& getKVStore() {
	static KeyValueStore instance;
	return instance;
}

ModuleDescriptor createKvModule() {
	ModuleDescriptor mod;
	mod.name = "kv";

	mod.functions.push_back({"set", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		std::string key = args[0].asString();
		std::string value = args[1].asString();
		KvOptions options;
		if (args.size() > 2 && args[2].isObject()) {
			auto* ttl = args[2].get("ttl");
			if (ttl) options.ttl = ttl->asInt();
			auto* nx = args[2].get("nx");
			if (nx) options.nx = nx->asBool();
			auto* xx = args[2].get("xx");
			if (xx) options.xx = xx->asBool();
		}
		store.set(key, value, options);
		return ScriptValue::null();
	}, "key:string, value:string, options? -> nil"});

	mod.functions.push_back({"get", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromString(store.get(args[0].asString()));
	}, "key:string -> string"});

	mod.functions.push_back({"delete", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		if (args[0].isArray()) {
			for (const auto& k : args[0].arrayVal) store.del(k.asString());
		} else {
			store.del(args[0].asString());
		}
		return ScriptValue::null();
	}, "key:string -> nil"});

	mod.functions.push_back({"exists", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromBool(store.exists(args[0].asString()));
	}, "key:string -> bool"});

	mod.functions.push_back({"expire", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		store.expire(args[0].asString(), args[1].asInt());
		return ScriptValue::null();
	}, "key:string, seconds:int -> nil"});

	mod.functions.push_back({"ttl", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromInt(store.ttl(args[0].asString()));
	}, "key:string -> int"});

	mod.functions.push_back({"incr", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		int64_t delta = args.size() > 1 ? args[1].asInt(1) : 1;
		return ScriptValue::fromInt(store.incr(args[0].asString(), delta));
	}, "key:string, delta:int? -> int"});

	// Hash operations
	mod.functions.push_back({"hset", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		store.hset(args[0].asString(), args[1].asString(), args[2].asString());
		return ScriptValue::null();
	}, "hash:string, field:string, value:string -> nil"});

	mod.functions.push_back({"hget", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromString(store.hget(args[0].asString(), args[1].asString()));
	}, "hash:string, field:string -> string"});

	mod.functions.push_back({"hgetall", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		auto pairs = store.hgetall(args[0].asString());
		std::unordered_map<std::string, ScriptValue> obj;
		for (auto& [k, v] : pairs) obj[k] = ScriptValue::fromString(std::move(v));
		return ScriptValue::fromObject(std::move(obj));
	}, "hash:string -> {field:value}"});

	mod.functions.push_back({"hdel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		store.hdel(args[0].asString(), args[1].asString());
		return ScriptValue::null();
	}, "hash:string, field:string -> nil"});

	// List operations
	mod.functions.push_back({"lpush", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		store.lpush(args[0].asString(), args[1].asString());
		return ScriptValue::null();
	}, "list:string, value:string -> nil"});

	mod.functions.push_back({"rpush", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		store.rpush(args[0].asString(), args[1].asString());
		return ScriptValue::null();
	}, "list:string, value:string -> nil"});

	mod.functions.push_back({"lpop", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromString(store.lpop(args[0].asString()));
	}, "list:string -> string"});

	mod.functions.push_back({"rpop", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromString(store.rpop(args[0].asString()));
	}, "list:string -> string"});

	mod.functions.push_back({"llen", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		return ScriptValue::fromInt(store.llen(args[0].asString()));
	}, "list:string -> int"});

	mod.functions.push_back({"lrange", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& store = getKVStore();
		auto items = store.lrange(args[0].asString(), static_cast<int>(args[1].asInt()), static_cast<int>(args[2].asInt()));
		std::vector<ScriptValue> arr;
		for (auto& s : items) arr.push_back(ScriptValue::fromString(std::move(s)));
		return ScriptValue::fromArray(std::move(arr));
	}, "list:string, start:int, stop:int -> {string}"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
