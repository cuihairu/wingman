// Lua HTTP bindings with full context support
// Allows users to define HTTP routes in Lua with ctx pattern

#include "wingman/lua_http.hpp"
#include "lua.hpp"
#include <sol/sol.hpp>
#include <spdlog/spdlog.h>

namespace wingman::lua {

// Register HttpContext type in Lua
void register_http_context(sol::state& lua) {
    sol::usertype<LuaHttpContext> ctx_type = lua.new_usertype<LuaHttpContext>(
        "HttpContext",
        sol::no_constructor);

    // Request methods
    ctx_type["getBody"] = &LuaHttpContext::getBody;
    ctx_type["getHeader"] = &LuaHttpContext::getHeader;
    ctx_type["getMethod"] = &LuaHttpContext::getMethod;
    ctx_type["getURL"] = &LuaHttpContext::getURL;
    ctx_type["getQuery"] = &LuaHttpContext::getQuery;
    ctx_type["getJSON"] = &LuaHttpContext::getJSON;

    // Response methods
    ctx_type["setStatus"] = &LuaHttpContext::setStatus;
    ctx_type["setHeader"] = &LuaHttpContext::setHeader;
    ctx_type["setJSON"] = &LuaHttpContext::setJSON;
    ctx_type["setString"] = &LuaHttpContext::setString;

    // Session methods
    ctx_type["setSession"] = &LuaHttpContext::setSession;
    ctx_type["getSession"] = &LuaHttpContext::getSession;

    // Storage methods (request-scoped)
    ctx_type["set"] = &LuaHttpContext::set;
    ctx_type["get"] = &LuaHttpContext::get;

    // User methods
    ctx_type["isAuthenticated"] = &LuaHttpContext::isAuthenticated;
    ctx_type["getUsername"] = &LuaHttpContext::getUsername;
    ctx_type["getUserRole"] = &LuaHttpContext::getUserRole;

    // Response helpers
    ctx_type["JSON"] = &LuaHttpContext::JSON;
    ctx_type["Error"] = &LuaHttpContext::Error;
    ctx_type["OK"] = &LuaHttpContext::OK;
}

void register_http_module(sol::state& lua) {
    register_http_context(lua);

    auto http_table = lua.create_table();

    // Response helpers (standalone)
    http_table["response"] = lua.create_table();
    http_table["response"]["json"] = [](const nlohmann::json& data) -> std::string {
        return data.dump();
    };

    lua["wingman_http"] = http_table;
}

// Load and register HTTP routes from Lua script
bool load_http_routes(sol::state& lua, const std::string& scriptPath, LuaHTTPServer& server) {
    try {
        auto* lua_ptr = &lua;

        // Register route registration functions with ctx support
        lua["http_get"] = [&server, lua_ptr](const std::string& path, sol::function handler) {
            server.get(path, [handler, lua_ptr](LuaHttpContext& ctx) {
                try {
                    auto result = handler(ctx);
                    if (!result.valid()) {
                        sol::error err = result;
                        ctx.Error(err.what());
                    }
                } catch (const std::exception& e) {
                    ctx.Error(e.what());
                }
            });
        };

        lua["http_post"] = [&server, lua_ptr](const std::string& path, sol::function handler) {
            server.post(path, [handler, lua_ptr](LuaHttpContext& ctx) {
                try {
                    auto result = handler(ctx);
                    if (!result.valid()) {
                        sol::error err = result;
                        ctx.Error(err.what());
                    }
                } catch (const std::exception& e) {
                    ctx.Error(e.what());
                }
            });
        };

        lua["http_put"] = [&server, lua_ptr](const std::string& path, sol::function handler) {
            server.put(path, [handler, lua_ptr](LuaHttpContext& ctx) {
                try {
                    auto result = handler(ctx);
                    if (!result.valid()) {
                        sol::error err = result;
                        ctx.Error(err.what());
                    }
                } catch (const std::exception& e) {
                    ctx.Error(e.what());
                }
            });
        };

        lua["http_delete"] = [&server, lua_ptr](const std::string& path, sol::function handler) {
            server.delete_(path, [handler, lua_ptr](LuaHttpContext& ctx) {
                try {
                    auto result = handler(ctx);
                    if (!result.valid()) {
                        sol::error err = result;
                        ctx.Error(err.what());
                    }
                } catch (const std::exception& e) {
                    ctx.Error(e.what());
                }
            });
        };

        // Middleware registration
        lua["http_use"] = [&server, lua_ptr](sol::function middleware) {
            server.use([middleware, lua_ptr](LuaHttpContext& ctx) -> bool {
                try {
                    auto result = middleware(ctx);
                    return result.valid();
                } catch (...) {
                    return false;
                }
            });
        };

        // Load the script
        sol::protected_function_result result = lua.safe_script_file(scriptPath);
        if (!result.valid()) {
            sol::error err = result;
            spdlog::error("Failed to load HTTP routes from {}: {}", scriptPath, err.what());
            return false;
        }

        spdlog::info("Loaded HTTP routes from {}", scriptPath);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error loading HTTP routes: {}", e.what());
        return false;
    }
}

} // namespace wingman::lua
