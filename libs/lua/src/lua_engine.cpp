#include "wingman/lua/lua_engine.hpp"
#include <spdlog/spdlog.h>

namespace wingman::lua {

// ========== LuaEngine 实现 ==========

LuaEngine::LuaEngine() = default;

LuaEngine::~LuaEngine() {
    shutdown();
}

bool LuaEngine::initialize() {
    try {
        lua_.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::io,
            sol::lib::os,
            sol::lib::debug
        );
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(e.what());
        }
        return false;
    }
}

void LuaEngine::shutdown() {
    lua_ = sol::state();
}

bool LuaEngine::executeFile(const std::string& path) {
    if (!lua_.ready()) return false;

    try {
        sol::protected_function_result result = lua_.safe_script_file(path);
        if (!result.valid()) {
            sol::error err = result;
            if (errorCallback_) {
                errorCallback_(err.what());
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(e.what());
        }
        return false;
    }
}

bool LuaEngine::executeString(const std::string& code) {
    if (!lua_.ready()) return false;

    try {
        sol::protected_function_result result = lua_.safe_script(code);
        if (!result.valid()) {
            sol::error err = result;
            if (errorCallback_) {
                errorCallback_(err.what());
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(e.what());
        }
        return false;
    }
}

bool LuaEngine::executeBuffer(const char* buffer, size_t size, const std::string& name) {
    if (!lua_.ready()) return false;

    try {
        sol::protected_function_result result = lua_.safe_script(
            std::string(buffer, size),
            sol::detail::default_chunk_name(),
            name,
            sol::load_mode::text
        );
        if (!result.valid()) {
            sol::error err = result;
            if (errorCallback_) {
                errorCallback_(err.what());
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(e.what());
        }
        return false;
    }
}

bool LuaEngine::loadModule(const std::string& name, const std::string& path) {
    if (!lua_.ready()) return false;
    return executeFile(path);
}

void LuaEngine::registerFunction(const std::string& name, sol::function func) {
    if (!lua_.ready()) return;
    lua_.set(name, func);
}

void LuaEngine::setGlobal(const std::string& name, const std::string& value) {
    if (!lua_.ready()) return;
    lua_[name] = value;
}

void LuaEngine::setGlobal(const std::string& name, int value) {
    if (!lua_.ready()) return;
    lua_[name] = value;
}

void LuaEngine::setGlobal(const std::string& name, double value) {
    if (!lua_.ready()) return;
    lua_[name] = value;
}

} // namespace wingman::lua
