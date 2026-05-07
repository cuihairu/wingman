#include "wingman/lua/lua_engine.hpp"
#include <spdlog/spdlog.h>

namespace wingman::lua {

// ========== LuaEngine 实现 ==========

class LuaEngine::Impl {
public:
    lua_State* L = nullptr;
};

LuaEngine::LuaEngine() : impl_(std::make_unique<Impl>()) {}

LuaEngine::~LuaEngine() {
    shutdown();
}

bool LuaEngine::initialize() {
    impl_->L = luaL_newstate();
    if (!impl_->L) {
        return false;
    }
    openLibs();
    return true;
}

void LuaEngine::shutdown() {
    if (impl_->L) {
        lua_close(impl_->L);
        impl_->L = nullptr;
    }
}

bool LuaEngine::executeFile(const std::string& path) {
    if (!impl_->L) return false;

    if (luaL_dofile(impl_->L, path.c_str()) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(impl_->L, -1));
        }
        lua_pop(impl_->L, 1);
        return false;
    }
    return true;
}

bool LuaEngine::executeString(const std::string& code) {
    if (!impl_->L) return false;

    if (luaL_dostring(impl_->L, code.c_str()) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(impl_->L, -1));
        }
        lua_pop(impl_->L, 1);
        return false;
    }
    return true;
}

bool LuaEngine::executeBuffer(const char* buffer, size_t size, const std::string& name) {
    if (!impl_->L) return false;

    if (luaL_loadbuffer(impl_->L, buffer, size, name.c_str()) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(impl_->L, -1));
        }
        lua_pop(impl_->L, 1);
        return false;
    }

    if (lua_pcall(impl_->L, 0, 0, 0) != LUA_OK) {
        if (errorCallback_) {
            errorCallback_(lua_tostring(impl_->L, -1));
        }
        lua_pop(impl_->L, 1);
        return false;
    }

    return true;
}

void LuaEngine::openLibs() {
    if (!impl_->L) return;
    luaL_openlibs(impl_->L);
}

int LuaEngine::luaErrorCallback(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    spdlog::error("Lua error: {}", msg ? msg : "unknown error");
    return 0;
}

} // namespace wingman::lua
