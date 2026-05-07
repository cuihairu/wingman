#include "wingman/lua/lua_engine.hpp"
#include "wingman/debug/emmy_adapter.hpp"
#include <spdlog/spdlog.h>

namespace wingman::lua {

// ========== 调试适配器实现 ==========

// 这个文件将集成 EmmyLua Debugger
// 具体实现需要 EmmyLua 核心库支持

// 临时实现：占位符
// 实际使用时需要链接 EmmyLua 库

void initEmmyDebugger(lua_State* L, int port) {
    spdlog::info("EmmyLua Debugger not yet linked. Port: {}", port);
    // TODO: 集成 EmmyLua 核心库
    // local dbg = require('emmy_core')
    // dbg.tcpListen('0.0.0.0', port)
}

} // namespace wingman::lua
