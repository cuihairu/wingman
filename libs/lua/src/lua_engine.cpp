#include "wingman/lua/lua_engine.hpp"
#include <spdlog/spdlog.h>

namespace wingman::lua {

// ========== LuaEngine 实现 ==========

LuaEngine::LuaEngine() = default;

LuaEngine::~LuaEngine() {
    shutdown();
}

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

int LuaEngine::luaErrorCallback(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    spdlog::error("Lua error: {}", msg ? msg : "unknown error");
    return 0;
}

} // namespace wingman::lua
