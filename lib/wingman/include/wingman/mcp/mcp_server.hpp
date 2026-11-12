#pragma once

// MCP (Model Context Protocol) stdio server：newline-delimited JSON-RPC 2.0。
// 供 wingman-agent mcp 子命令拉起，AI 客户端（Claude Code / Cursor / Hermes）
// 本地 stdio 直连。stdout 只准出现 JSON-RPC 帧——日志一律走 stderr（接线处
// 重配 spdlog sink）。协议事实经本地 @modelcontextprotocol/sdk types.js 核对：
// tool 执行失败回 result.isError=true（LLM 需看见错误自行纠正），找不到
// tool / 结构坏请求才是协议级 error；版本协商 = 回显受支持请求版本，否则
// 回退服务端最新。

#include <nlohmann/json.hpp>

#include "wingman/mcp/mcp_tools.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace wingman::mcp {

// 单帧处理：请求 → 响应帧。notification / 无 id 帧返回 nullopt（不回帧）。
// 供 run() 主循环与单测直调。
using MessageHandler = std::optional<nlohmann::json>(const nlohmann::json& request);

class McpServer {
public:
    // toolsContext：tool 后端注入缝（测试 mock）；缺省 nullptr 字段 =
    // 生产单例（uia() 门面 / defaultSharedInput() / 局部 ScriptManager）。
    McpServer(std::string serverName, std::string serverVersion,
              ToolContext toolsContext = {});

    // 单帧分发（见 MessageHandler 注释）。坏 method / 缺 method → -32600；
    // 未知 method → -32601；tools/call 未知 tool → -32602。
    std::optional<nlohmann::json> handleMessage(const nlohmann::json& request);

    // stdio 主循环：逐行读（容忍尾部 \r，跳过空行零输出），坏 JSON 行回
    // -32700（id:null），响应单行输出并逐行 flush；EOF 返回。单线程——
    // tool 实现依赖此前提（元素句柄注册表无锁）。
    void run(std::istream& in, std::ostream& out);

private:
    nlohmann::json handleInitialize(const nlohmann::json& request);
    nlohmann::json handleToolsList();
    std::optional<nlohmann::json> handleToolsCall(const nlohmann::json& request);

    std::string serverName_;
    std::string serverVersion_;
    ToolContext toolsContext_;
};

// tools/list 的 7 个 tool 描述（name/description/inputSchema）。tools 用与
// server 解耦的独立函数：测试可单独断言目录面。
std::vector<nlohmann::json> toolCatalog();

} // namespace wingman::mcp
