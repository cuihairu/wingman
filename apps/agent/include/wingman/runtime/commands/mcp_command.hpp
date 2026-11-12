#pragma once

#include <string>
#include <vector>

namespace wingman::runtime::commands {

/// 以 stdio 传输跑 MCP server（newline-delimited JSON-RPC 2.0），读写
/// std::cin/std::cout，EOF 返回。tail 参数必须为空（校验在 cli 分发层）。
/// stdout 只准 JSON-RPC 帧——日志已由 main 在 mcp 模式重定向 stderr。
int mcpCommand(const std::vector<std::string>& tail);

} // namespace wingman::runtime::commands
