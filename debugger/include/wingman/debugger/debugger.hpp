#pragma once

#include "wingman/debugger/protocol.hpp"
#include "wingman/debugger/breakpoint.hpp"
#include <memory>
#include <functional>
#include <mutex>
#include <optional>
#include <atomic>

// 前向声明 Lua 状态
struct lua_State;

namespace wingman {

// Lua 调试器（简化版）
class LuaDebugger {
public:
    LuaDebugger();
    ~LuaDebugger();

    // 初始化调试器（附加到 Lua 状态）
    bool attach(lua_State* L);
    void detach();

    // 控制命令
    void continueExecution();
    void stepOver();
    void stepIn();
    void pause();

    // 断点管理
    BreakpointManager& breakpoints() { return breakpointManager_; }

    // 状态查询
    DebugState getState() const { return state_.load(); }
    std::vector<StackFrame> getStackTrace();
    std::vector<Variable> getVariables(size_t reference);
    std::vector<Scope> getScopes();

    // 当前停止位置
    struct CurrentPosition {
        std::string file;
        int line;
        bool valid;
    };
    CurrentPosition getCurrentPosition() const;

    // 单例访问
    static LuaDebugger& instance();

    // 检查是否应该暂停（供钩子调用）
    bool checkBreakpoint(const std::string& file, int line);
    bool checkStep();

private:
    lua_State* luaState_;
    std::atomic<DebugState> state_;
    BreakpointManager breakpointManager_;
    std::mutex mutex_;

    // 调试钩子相关
    void installDebugHook();
    void removeDebugHook();

    // 当前调试上下文
    struct DebugContext {
        std::string currentFile;
        int currentLine;
        int stepCount;      // 剩余步数
        bool stepping;      // 是否正在步进模式
    };
    DebugContext context_;

    // Lua 钩子函数（静态）
    static void luaDebugHook(lua_State* L, lua_Debug* ar);

    // 变量查询辅助
    std::vector<Variable> getLocals(lua_State* L, int level = 0);
    std::vector<Variable> getGlobals(lua_State* L);
    static std::string getValueAsString(lua_State* L, int index, bool& isTable);
};

// 便捷函数
bool startDebugger(lua_State* L);
void stopDebugger();
LuaDebugger& getDebugger();

} // namespace wingman
