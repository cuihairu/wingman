#pragma once

#include <sol/sol.hpp>
#include "wingman/script/iscript_engine.hpp"

namespace wingman {
namespace lua {

// ScriptValue → sol::object
sol::object toLuaObject(sol::state_view& lua, const script::ScriptValue& value);

// sol::object → ScriptValue
script::ScriptValue toScriptValue(const sol::object& obj);

// sol::table → ScriptValue（智能区分数组/对象）
script::ScriptValue tableToScriptValue(const sol::table& tbl);

} // namespace lua
} // namespace wingman
