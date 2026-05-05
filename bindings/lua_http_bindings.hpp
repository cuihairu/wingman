#pragma once

#include <sol/sol.hpp>
#include <string>
#include "wingman/lua_http.hpp"

namespace wingman::lua {

// Register HTTP module in Lua state
void register_http_module(sol::state& lua);

// Load and register HTTP routes from Lua script
// Returns true on success, false on error
bool load_http_routes(sol::state& lua, const std::string& scriptPath, LuaHTTPServer& server);

} // namespace wingman::lua
