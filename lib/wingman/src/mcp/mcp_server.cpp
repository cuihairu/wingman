#include "wingman/mcp/mcp_server.hpp"

#include "wingman/mcp/mcp_tools.hpp"

namespace wingman::mcp {

namespace {

// 供回显的已知协议版本（本地 @modelcontextprotocol/sdk 2026.8.31
// SUPPORTED_PROTOCOL_VERSIONS 核对；首项 = 服务端最新，协商失败回退项）。
constexpr const char* kSupportedVersions[] = {
    "2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05",
};

constexpr const char* kLatestVersion = kSupportedVersions[0];

nlohmann::json errorFrame(const nlohmann::json& id, int code,
                          const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {{"code", code}, {"message", message}}},
    };
}

nlohmann::json resultFrame(const nlohmann::json& id, nlohmann::json result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", std::move(result)},
    };
}

// CallToolResult 文本块：tool 返回结构化 JSON 一律包成 text（LLM 消费面统一）。
nlohmann::json textContent(const nlohmann::json& payload) {
    return {{"type", "text"}, {"text", payload.dump()}};
}

nlohmann::json errorResult(const std::string& message) {
    return {
        {"content", nlohmann::json::array({textContent({{"error", message}})})},
        {"isError", true},
    };
}

} // namespace

McpServer::McpServer(std::string serverName, std::string serverVersion,
                     ToolContext toolsContext)
    : serverName_(std::move(serverName)), serverVersion_(std::move(serverVersion)),
      toolsContext_(toolsContext) {}

nlohmann::json McpServer::handleInitialize(const nlohmann::json& request) {
    const auto& params = request.value("params", nlohmann::json::object());
    const auto requested = params.value("protocolVersion", "");
    // 回显受支持版本；未知/缺失回退服务端最新（客户端不认会自行断开，规范语义）。
    std::string negotiated = kLatestVersion;
    for (const auto* v : kSupportedVersions) {
        if (requested == v) {
            negotiated = requested;
            break;
        }
    }
    return {
        {"protocolVersion", negotiated},
        // capabilities 必带：声明 tools 能力，否则客户端不发 tools/list。
        // 空对象要显式 json::object()——裸 {} 会被 nlohmann 序列化成 null，
        // 客户端会读成「未声明 tools 能力」。
        {"capabilities", {{"tools", nlohmann::json::object()}}},
        {"serverInfo", {{"name", serverName_}, {"version", serverVersion_}}},
    };
}

std::vector<nlohmann::json> toolCatalog() {
    // 嵌套大括号聚合初始化与 nlohmann init-list 语义冲突（lambda 调用混入
    // 多层 init-list 后解析崩坏），故逐 tool 命令式构造。
    const auto schema = [](std::vector<std::pair<std::string, nlohmann::json>> properties,
                           std::vector<std::string> required = {}) {
        nlohmann::json props = nlohmann::json::object();
        for (auto& [key, value] : properties) {
            props[key] = std::move(value);
        }
        nlohmann::json s = nlohmann::json::object();
        s["type"] = "object";
        s["properties"] = std::move(props);
        if (!required.empty()) {
            s["required"] = std::move(required);
        }
        return s;
    };
    const auto addTool = [&schema](std::vector<nlohmann::json>& tools,
                                   const char* name, const char* description,
                                   std::vector<std::pair<std::string, nlohmann::json>> properties,
                                   std::vector<std::string> required = {}) {
        nlohmann::json tool = nlohmann::json::object();
        tool["name"] = name;
        tool["description"] = description;
        tool["inputSchema"] = schema(std::move(properties), std::move(required));
        tools.push_back(std::move(tool));
    };

    std::vector<nlohmann::json> tools;
    tools.reserve(7);

    addTool(tools, "wingman_uia_tree",
            "Read the UI Automation element tree (up to 3 levels). Windows only: "
            "on platforms without a UIA backend this returns an explicit error. "
            "window: optional substring of the window title (default: foreground window). "
            "Each element carries a handle usable by wingman_uia_click / wingman_uia_action.",
            {
                {"window", {{"type", "string"}, {"description", "Window title substring (default: foreground)"}}},
            });

    addTool(tools, "wingman_uia_find",
            "Find UI Automation elements by selector. Windows only. Provide at least "
            "one criterion (name/id/className/role/text). Returns matching elements "
            "with handles for subsequent actions.",
            {
                {"name", {{"type", "string"}, {"description", "Substring match on element name"}}},
                {"id", {{"type", "string"}, {"description", "Exact automation id"}}},
                {"className", {{"type", "string"}, {"description", "Exact class name"}}},
                {"role", {{"type", "string"}, {"description", "Role name, e.g. Button, TextBox"}}},
                {"text", {{"type", "string"}, {"description", "Substring match on element text"}}},
            });

    addTool(tools, "wingman_uia_click",
            "Semantic click on a UI Automation element (Invoke pattern; no coordinate "
            "inference). Windows only. Identify the element by handle (from uia_tree/"
            "uia_find) or by name/id. On failure this returns an explicit error - it "
            "never silently falls back to coordinate clicking.",
            {
                {"name", {{"type", "string"}}},
                {"id", {{"type", "string"}}},
                {"handle", {{"type", "integer"}}},
            });

    addTool(tools, "wingman_uia_action",
            "Invoke an action on a UI Automation element. Windows only. Actions: "
            "click, doubleClick, rightClick, setFocus, select, expand, collapse, "
            "setText (value), setChecked (bool).",
            {
                {"handle", {{"type", "integer"}, {"description", "Element handle from uia_tree/uia_find"}}},
                {"action", {{"type", "string"}, {"enum", {"click", "doubleClick", "rightClick", "setFocus", "select", "expand", "collapse", "setText", "setChecked"}}}},
                {"value", {{"type", "string"}, {"description", "New text for setText"}}},
                {"checked", {{"type", "boolean"}, {"description", "Target state for setChecked"}}},
            },
            {"handle", "action"});

    addTool(tools, "wingman_screen_shot",
            "Capture the screen (or a region) and return it as an MCP image content "
            "block plus metadata. Requires a vision build (OpenCV); otherwise an "
            "explicit error is returned. region: [x, y, width, height].",
            {
                {"monitor", {{"type", "integer"}, {"description", "Monitor id (default: primary)"}}},
                {"region", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"minItems", 4}, {"maxItems", 4},
                            {"description", "Capture region [x, y, width, height] (pixels)"}}},
            });

    addTool(tools, "wingman_input_click",
            "Coordinate click via the input backend (fallback when UIA is "
            "unavailable). Moves the mouse to (x, y) then clicks; button: left "
            "(default) or right.",
            {
                {"x", {{"type", "integer"}}},
                {"y", {{"type", "integer"}}},
                {"button", {{"type", "string"}, {"enum", {"left", "right"}}}},
            },
            {"x", "y"});

    addTool(tools, "wingman_script_run",
            "Run a Lua script (sandboxed) and return its printed output. code: "
            "script source; timeoutMs: cooperative timeout (default 30000) - the "
            "run is abandoned and reported as an error after the deadline.",
            {
                {"code", {{"type", "string"}}},
                {"timeoutMs", {{"type", "integer"}}},
            },
            {"code"});

    return tools;
}

nlohmann::json McpServer::handleToolsList() {
    nlohmann::json tools = nlohmann::json::array();
    for (auto& tool : toolCatalog()) {
        tools.push_back(std::move(tool));
    }
    return {{"tools", std::move(tools)}};
}

std::optional<nlohmann::json> McpServer::handleToolsCall(const nlohmann::json& request) {
    const auto& params = request.value("params", nlohmann::json::object());
    const auto name = params.value("name", "");
    if (name.empty()) {
        return errorFrame(request.at("id"), -32602, "tools/call requires params.name");
    }
    const auto args = params.value("arguments", nlohmann::json::object());
    if (!args.is_object()) {
        return errorFrame(request.at("id"), -32602, "tools/call arguments must be an object");
    }

    // 未知 tool 是协议级错误（客户端选错了名字）；tool 执行失败是
    // isError 结果（LLM 需看见错误自行纠正）——SDK 规范注释原文语义。
    try {
        auto result = callTool(name, args, toolsContext_);
        if (!result) {
            return errorFrame(request.at("id"), -32602, "Unknown tool: " + name);
        }
        return resultFrame(request.at("id"), std::move(*result));
    } catch (const std::exception& e) {
        return resultFrame(request.at("id"), errorResult(std::string("tool crashed: ") + e.what()));
    } catch (...) {
        return resultFrame(request.at("id"), errorResult("tool crashed with unknown error"));
    }
}

std::optional<nlohmann::json> McpServer::handleMessage(const nlohmann::json& request) {
    if (!request.is_object()) {
        return errorFrame(nullptr, -32600, "request must be a JSON object");
    }
    // 无 id = notification：规范要求不回帧（含 notifications/initialized）。
    if (!request.contains("id")) {
        return std::nullopt;
    }
    if (!request.contains("method") || !request.at("method").is_string()) {
        return errorFrame(request.at("id"), -32600, "request requires a string method");
    }
    const auto method = request.at("method").get<std::string>();

    if (method == "initialize") {
        return resultFrame(request.at("id"), handleInitialize(request));
    }
    if (method == "tools/list") {
        return resultFrame(request.at("id"), handleToolsList());
    }
    if (method == "tools/call") {
        return handleToolsCall(request);
    }
    if (method == "ping") {
        return resultFrame(request.at("id"), nlohmann::json::object());
    }
    return errorFrame(request.at("id"), -32601, "Method not found: " + method);
}

void McpServer::run(std::istream& in, std::ostream& out) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // 容忍 CRLF 客户端
        }
        if (line.empty()) {
            continue; // 空行零输出
        }

        std::optional<nlohmann::json> response;
        try {
            const auto request = nlohmann::json::parse(line);
            response = handleMessage(request);
        } catch (const nlohmann::json::parse_error&) {
            response = errorFrame(nullptr, -32700, "Parse error"); // id 未知 → null
        } catch (...) {
            response = errorFrame(nullptr, -32700, "Parse error");
        }

        if (response) {
            out << response->dump() << '\n';
            out.flush(); // 逐行 flush：客户端按行读，不 flush 会死锁
        }
    }
}

} // namespace wingman::mcp
