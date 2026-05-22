#include "wingman/lua/lua_marshal.hpp"

namespace wingman {
namespace lua {

sol::object toLuaObject(sol::state_view& lua, const script::ScriptValue& value) {
	switch (value.type) {
	case script::ScriptValue::Null:
		return sol::make_object(lua, sol::nil);
	case script::ScriptValue::Bool:
		return sol::make_object(lua, value.boolVal);
	case script::ScriptValue::Int:
		return sol::make_object(lua, value.intVal);
	case script::ScriptValue::Float:
		return sol::make_object(lua, value.floatVal);
	case script::ScriptValue::String:
		return sol::make_object(lua, value.strVal);
	case script::ScriptValue::Array: {
		sol::table tbl = lua.create_table(static_cast<int>(value.arrayVal.size()), 0);
		int i = 1;
		for (const auto& elem : value.arrayVal) {
			tbl[i++] = toLuaObject(lua, elem);
		}
		return tbl;
	}
	case script::ScriptValue::Object: {
		sol::table tbl = lua.create_table(0, static_cast<int>(value.objectVal.size()));
		for (const auto& [k, v] : value.objectVal) {
			tbl[k] = toLuaObject(lua, v);
		}
		return tbl;
	}
	}
	return sol::make_object(lua, sol::nil);
}

script::ScriptValue toScriptValue(const sol::object& obj) {
	if (!obj.valid()) {
		return script::ScriptValue::null();
	}

	switch (obj.get_type()) {
	case sol::type::nil:
		return script::ScriptValue::null();
	case sol::type::boolean:
		return script::ScriptValue::fromBool(obj.as<bool>());
	case sol::type::number: {
		// 通过 push 到栈顶后用 lua_isinteger 检查
		lua_State* L = obj.lua_state();
		obj.push(L);
		bool isInt = lua_isinteger(L, -1) != 0;
		if (isInt) {
			int64_t val = lua_tointeger(L, -1);
			lua_pop(L, 1);
			return script::ScriptValue::fromInt(val);
		}
		double val = lua_tonumber(L, -1);
		lua_pop(L, 1);
		return script::ScriptValue::fromFloat(val);
	}
	case sol::type::string:
		return script::ScriptValue::fromString(obj.as<std::string>());
	case sol::type::table:
		return tableToScriptValue(obj.as<sol::table>());
	default:
		return script::ScriptValue::null();
	}
}

script::ScriptValue tableToScriptValue(const sol::table& tbl) {
	// 区分数组和对象：检查是否有从 1 开始的连续整数键
	bool isArray = true;
	int maxIndex = 0;

	for (const auto& [key, _] : tbl) {
		if (key.get_type() == sol::type::number) {
			auto maybe_k = key.as<sol::optional<int>>();
			if (maybe_k && *maybe_k > 0) {
				if (*maybe_k > maxIndex) maxIndex = *maybe_k;
			} else {
				isArray = false;
				break;
			}
		} else {
			isArray = false;
			break;
		}
	}

	if (isArray && maxIndex > 0) {
		std::vector<script::ScriptValue> arr(maxIndex);
		for (const auto& [key, val] : tbl) {
			int k = key.as<int>();
			if (k >= 1 && k <= maxIndex) {
				arr[k - 1] = toScriptValue(val);
			}
		}
		return script::ScriptValue::fromArray(std::move(arr));
	}

	// 对象
	std::unordered_map<std::string, script::ScriptValue> obj;
	for (const auto& [key, val] : tbl) {
		if (key.get_type() == sol::type::string) {
			obj[key.as<std::string>()] = toScriptValue(val);
		}
	}
	return script::ScriptValue::fromObject(std::move(obj));
}

} // namespace lua
} // namespace wingman
