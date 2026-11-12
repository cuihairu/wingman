#pragma once

// MCP tools 实现层：7 个 tool 的参数校验与能力分发。与协议层解耦——
// McpServer 只按名字查表；单测直接调 callTool（注入 mock 后端）。
//
// 能力降级口径（缺依赖明确报错、不静默切换）：
// - UIA 族（uia_tree/uia_find/uia_click/uia_action）：平台无 UIA 后端
//   （Linux 工厂返回 nullptr）→ isError「not supported」，不假装成功。
// - screen_shot：非 vision 构建（无 OpenCV）→ isError。
// - input_click/script_run：全平台可用（IInput 默认后端 / Lua 引擎工厂）。

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace wingman {

class IUIAManager;
class ScriptManager;
namespace platform {
class IInput;
}

namespace mcp {

// 后端注入缝：缺省 nullptr = 走生产单例（wingman::uia() 门面 /
// defaultSharedInput() / 局部 ScriptManager）。测试注入 mock。
struct ToolContext {
    IUIAManager* uiaManager = nullptr;
    platform::IInput* input = nullptr;
    ScriptManager* scriptManager = nullptr;
};

// 执行一个 tool 调用。已知 tool 返回 CallToolResult 形状：成功
// {content:[...]}；tool 内失败同形状 + isError:true（协议层不转 error）。
// 未知 toolName 返回 nullopt，由协议层转 -32602。
std::optional<nlohmann::json> callTool(const std::string& name,
                                       const nlohmann::json& args,
                                       const ToolContext& ctx);

} // namespace mcp
} // namespace wingman
