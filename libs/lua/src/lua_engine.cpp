#include "wingman/lua/lua_engine.hpp"
#include <spdlog/spdlog.h>

#ifdef WINGMAN_ENABLE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif

namespace wingman::lua {

// ========== LuaEngine 实现 ==========

LuaEngine::LuaEngine() = default;

LuaEngine::~LuaEngine() {
    shutdown();
}

#ifdef WINGMAN_ENABLE_LUA

bool LuaEngine::initialize() {
    L_ = luaL_newstate();
    if (!L_) {
        return false;
    }
    openLibs();
    return true;
}

void LuaEngine::shutdown() {
    if (L_) {
        lua_close(L_);
        L_ = nullptr;
    }
}

bool LuaEngine::executeFile(const std::string& path) {
    if (!L_) return false;

    if (luaL_dofile(L_, path.c_str()) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(L_, -1));
        }
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool LuaEngine::executeString(const std::string& code) {
    if (!L_) return false;

    if (luaL_dostring(L_, code.c_str()) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(L_, -1));
        }
        lua_pop(L_, 1);
        return false;
    }
    return true;
}

bool LuaEngine::executeBuffer(const char* buffer, size_t size, const std::string& name) {
    if (!L_) return false;

    if (luaL_loadbuffer(L_, buffer, size, name.c_str()) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(L_, -1));
        }
        lua_pop(L_, 1);
        return false;
    }

    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(L_, -1));
        }
        lua_pop(L_, 1);
        return false;
    }

    return true;
}

void LuaEngine::openLibs() {
    if (!L_) return;
    luaL_openlibs(L_);
}

void LuaEngine::setGlobal(const std::string& name, const std::string& value) {
    if (!L_) return;
    lua_pushstring(L_, value.c_str());
    lua_setglobal(L_, name.c_str());
}

void LuaEngine::setGlobal(const std::string& name, int value) {
    if (!L_) return;
    lua_pushinteger(L_, value);
    lua_setglobal(L_, name.c_str());
}

void LuaEngine::setGlobal(const std::string& name, double value) {
    if (!L_) return;
    lua_pushnumber(L_, value);
    lua_setglobal(L_, name.c_str());
}

void LuaEngine::registerFunction(const std::string& name, lua_CFunction func) {
    if (!L_) return;
    lua_register(L_, name.c_str(), func);
}

bool LuaEngine::loadModule(const std::string& /*name*/, const std::string& path) {
    if (!L_) return false;
    return executeFile(path);
}

int LuaEngine::luaErrorCallback(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    spdlog::error("Lua error: {}", msg ? msg : "unknown error");
    return 0;
}

#else // WINGMAN_ENABLE_LUA

bool LuaEngine::initialize() {
    spdlog::warn("Lua support is disabled (WINGMAN_ENABLE_LUA=OFF)");
    return false;
}

void LuaEngine::shutdown() {}

bool LuaEngine::executeFile(const std::string&) {
    spdlog::warn("Lua support is disabled");
    return false;
}

bool LuaEngine::executeString(const std::string&) {
    spdlog::warn("Lua support is disabled");
    return false;
}

bool LuaEngine::executeBuffer(const char*, size_t, const std::string&) {
    spdlog::warn("Lua support is disabled");
    return false;
}

void LuaEngine::openLibs() {}

void LuaEngine::setGlobal(const std::string&, const std::string&) {}
void LuaEngine::setGlobal(const std::string&, int) {}
void LuaEngine::setGlobal(const std::string&, double) {}

void LuaEngine::registerFunction(const std::string&, lua_CFunction) {}

bool LuaEngine::loadModule(const std::string&, const std::string&) {
    spdlog::warn("Lua support is disabled");
    return false;
}

int LuaEngine::luaErrorCallback(lua_State*) {
    spdlog::error("Lua error: Lua support is disabled");
    return 0;
}

#endif // WINGMAN_ENABLE_LUA

} // namespace wingman::lua
