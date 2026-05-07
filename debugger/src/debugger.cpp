#include "wingman/debugger/debugger.hpp"
#include <spdlog/spdlog.h>
#include <lua.hpp>
#include <cstring>

namespace wingman {

// 调试器单例实例指针（用于钩子函数访问）
static LuaDebugger* g_debuggerInstance = nullptr;

// ========== LuaDebugger 实现 ==========

LuaDebugger::LuaDebugger()
    : luaState_(nullptr)
    , state_(DebugState::Stopped) {

    context_.currentFile = "";
    context_.currentLine = 0;
    context_.stepCount = 0;
    context_.stepping = false;
}

LuaDebugger::~LuaDebugger() {
    detach();
}

LuaDebugger& LuaDebugger::instance() {
    static LuaDebugger instance;
    return instance;
}

bool LuaDebugger::attach(lua_State* L) {
    if (!L) {
        spdlog::error("[Debugger] Invalid Lua state");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    luaState_ = L;
    state_.store(DebugState::Running);
    g_debuggerInstance = this;

    installDebugHook();

    spdlog::info("[Debugger] Attached to Lua state");
    return true;
}

void LuaDebugger::detach() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (luaState_) {
        removeDebugHook();
        luaState_ = nullptr;
    }

    state_.store(DebugState::Terminated);
    g_debuggerInstance = nullptr;

    spdlog::info("[Debugger] Detached from Lua state");
}

void LuaDebugger::installDebugHook() {
    if (!luaState_) return;

    // 设置钩子：在每行调用
    lua_sethook(luaState_, [](lua_State* L, lua_Debug* ar) {
        if (g_debuggerInstance) {
            g_debuggerInstance->luaDebugHook(L, ar);
        }
    }, LUA_MASKLINE, 0);

    spdlog::debug("[Debugger] Debug hook installed");
}

void LuaDebugger::removeDebugHook() {
    if (!luaState_) return;
    lua_sethook(luaState_, nullptr, 0, 0);
    spdlog::debug("[Debugger] Debug hook removed");
}

// Lua 钩子函数
void LuaDebugger::luaDebugHook(lua_State* L, lua_Debug* ar) {
    lua_getinfo(L, "nSl", ar);

    if (ar->currentline > 0 && ar->source) {
        std::string file = ar->source;
        if (!file.empty() && file[0] == '@') {
            file = file.substr(1);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        context_.currentFile = file;
        context_.currentLine = ar->currentline;

        // 检查断点
        if (checkBreakpoint(file, ar->currentline)) {
            state_.store(DebugState::Stopped);
            return;
        }

        // 检查步进
        if (checkStep()) {
            state_.store(DebugState::Stopped);
        }
    }
}

bool LuaDebugger::checkBreakpoint(const std::string& file, int line) {
    return breakpointManager_.shouldBreak(file, line);
}

bool LuaDebugger::checkStep() {
    if (!context_.stepping) return false;

    if (context_.stepCount <= 0) {
        context_.stepping = false;
        spdlog::debug("[Debugger] Step complete at {}:{}", context_.currentFile, context_.currentLine);
        return true;
    }

    context_.stepCount--;
    return false;
}

void LuaDebugger::continueExecution() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.store(DebugState::Running);
    context_.stepping = false;
    spdlog::debug("[Debugger] Continue execution");
}

void LuaDebugger::stepOver() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.store(DebugState::Running);
    context_.stepping = true;
    context_.stepCount = 1;
    spdlog::debug("[Debugger] Step over");
}

void LuaDebugger::stepIn() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.store(DebugState::Running);
    context_.stepping = true;
    context_.stepCount = 1;
    spdlog::debug("[Debugger] Step in");
}

void LuaDebugger::pause() {
    state_.store(DebugState::Paused);
    context_.stepping = false;
    spdlog::debug("[Debugger] Paused");
}

std::vector<StackFrame> LuaDebugger::getStackTrace() {
    std::vector<StackFrame> frames;

    if (!luaState_) return frames;

    std::lock_guard<std::mutex> lock(mutex_);

    lua_Debug ar;
    int level = 0;

    while (lua_getstack(luaState_, level, &ar)) {
        lua_getinfo(luaState_, "nSl", &ar);

        StackFrame frame;
        frame.id = level;
        frame.name = ar.name ? ar.name : (ar.what ? ar.what : "unknown");
        frame.source = ar.source ? ar.source : "unknown";
        if (!frame.source.empty() && frame.source[0] == '@') {
            frame.source = frame.source.substr(1);
        }
        frame.line = ar.currentline;
        frame.column = 0;

        frames.push_back(frame);
        level++;
    }

    return frames;
}

std::vector<Variable> LuaDebugger::getVariables(size_t reference) {
    std::vector<Variable> vars;

    if (!luaState_) return vars;

    std::lock_guard<std::mutex> lock(mutex_);

    // reference: 1 = Locals, 2 = Globals
    if (reference == 1) {
        vars = getLocals(luaState_);
    } else if (reference == 2) {
        vars = getGlobals(luaState_);
    }

    return vars;
}

std::vector<Scope> LuaDebugger::getScopes() {
    std::vector<Scope> scopes;

    Scope locals;
    locals.name = "Locals";
    locals.variablesReference = 1;
    locals.type = ScopeType::Locals;
    locals.expensive = false;
    scopes.push_back(locals);

    Scope globals;
    globals.name = "Globals";
    globals.variablesReference = 2;
    globals.type = ScopeType::Globals;
    globals.expensive = false;
    scopes.push_back(globals);

    return scopes;
}

LuaDebugger::CurrentPosition LuaDebugger::getCurrentPosition() const {
    CurrentPosition pos;
    pos.file = context_.currentFile;
    pos.line = context_.currentLine;
    pos.valid = !context_.currentFile.empty();
    return pos;
}

// ========== 变量查询辅助函数 ==========

std::vector<Variable> LuaDebugger::getLocals(lua_State* L, int level) {
    std::vector<Variable> vars;

    lua_Debug ar;
    if (!lua_getstack(L, level, &ar)) {
        return vars;
    }

    const char* name;
    int i = 1;
    while ((name = lua_getlocal(L, &ar, i)) != nullptr) {
        bool isTable = false;
        std::string value = getValueAsString(L, -1, isTable);
        std::string type = lua_typename(L, lua_type(L, -1));

        Variable var;
        var.name = name;
        var.value = value;
        var.type = type;
        var.variablesReference = isTable ? 1000 + i : 0;
        var.indexed = isTable;

        vars.push_back(var);
        lua_pop(L, 1);
        i++;
    }

    return vars;
}

std::vector<Variable> LuaDebugger::getGlobals(lua_State* L) {
    std::vector<Variable> vars;

    lua_pushglobaltable(L);
    lua_pushnil(L);

    int count = 0;
    const int maxEntries = 50;  // 限制数量

    while (lua_next(L, -2) != 0 && count < maxEntries) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            bool isTable = false;
            std::string value = getValueAsString(L, -1, isTable);
            std::string type = lua_typename(L, lua_type(L, -1));
            std::string name = lua_tostring(L, -2);

            // 跳过内部变量
            if (name.empty() || name[0] != '_') {
                Variable var;
                var.name = name;
                var.value = value;
                var.type = type;
                var.variablesReference = isTable ? 2000 + count : 0;
                var.indexed = isTable;

                vars.push_back(var);
                count++;
            }
        }
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return vars;
}

std::string LuaDebugger::getValueAsString(lua_State* L, int index, bool& isTable) {
    isTable = false;
    int type = lua_type(L, index);

    switch (type) {
        case LUA_TNIL:
            return "nil";
        case LUA_TBOOLEAN:
            return lua_toboolean(L, index) ? "true" : "false";
        case LUA_TNUMBER: {
            if (lua_isinteger(L, index)) {
                return std::to_string(lua_tointeger(L, index));
            } else {
                return std::to_string(lua_tonumber(L, index));
            }
        }
        case LUA_TSTRING: {
            const char* str = lua_tostring(L, index);
            if (str && strlen(str) > 50) {
                return std::string(str, 50) + "...";
            }
            return str ? str : "";
        }
        case LUA_TTABLE:
            isTable = true;
            return "{...}";
        case LUA_TFUNCTION:
            return "function";
        case LUA_TUSERDATA:
            return "userdata";
        case LUA_TTHREAD:
            return "thread";
        case LUA_TLIGHTUSERDATA:
            return "lightuserdata";
        default:
            return "unknown";
    }
}

// ========== 便捷函数 ==========

bool startDebugger(lua_State* L) {
    return LuaDebugger::instance().attach(L);
}

void stopDebugger() {
    LuaDebugger::instance().detach();
}

LuaDebugger& getDebugger() {
    return LuaDebugger::instance();
}

} // namespace wingman
