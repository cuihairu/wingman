#include "wingman/runtime/agent.hpp"
#include "wingman/platform/screen_factory.hpp"
#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/rpc/trigger_handler.hpp"
#include "wingman/trigger.hpp"
#include "wingman/window.hpp"
#include "wingman/runtime/event_buffer.hpp"
#include "wingman/runtime/local_ipc_server.hpp"
#include "wingman/runtime/remote_client.hpp"
#include "wingman/runtime/rpc/screenshot_handler.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace wingman::runtime {

namespace {

std::string scriptStateToString(ScriptState state) {
    switch (state) {
    case ScriptState::Loaded:
        return "loaded";
    case ScriptState::Running:
        return "running";
    case ScriptState::Paused:
        return "paused";
    case ScriptState::Stopped:
        return "stopped";
    case ScriptState::Error:
        return "error";
    case ScriptState::Unknown:
    default:
        return "unknown";
    }
}

int64_t encodeWindowHandle(WindowHandle handle) {
#ifdef _WIN32
    return static_cast<int64_t>(reinterpret_cast<uintptr_t>(handle));
#else
    return static_cast<int64_t>(handle);
#endif
}

// dispatchViaRpcDispatcher 把远程命令转成 RPC 请求经共享 dispatcher 执行。
// 遵循架构约束的 Dispatcher Reuse：本地 IPC 与远程 agent 命令共用同一套 handler
//（如 trigger.* / screenshot.*），CommandData 的字符串值尽量按 JSON 解析。
CommandResult dispatchViaRpcDispatcher(rpc::RpcDispatcher& dispatcher, const std::string& command,
    const CommandData& data, const std::string& errorPrefix)
{
    nlohmann::json params = nlohmann::json::object();
    for (const auto& [key, value] : data) {
        try {
            params[key] = nlohmann::json::parse(value);
        } catch (const std::exception&) {
            params[key] = value;
        }
    }

    nlohmann::json request = {
        {"type", "call"},
        {"id", "remote-" + command},
        {"method", command},
        {"params", params}
    };

    try {
        auto response = nlohmann::json::parse(dispatcher.dispatch(request.dump()));
        auto payload = response.value("data", nlohmann::json::object());
        if (payload.value("success", true) == false) {
            return CommandResult::error(payload.value("error", errorPrefix + " failed"));
        }
        if (payload.contains("result")) {
            return CommandResult::okData(payload["result"].dump());
        }
        return CommandResult::okData(payload.dump());
    } catch (const std::exception& e) {
        return CommandResult::error(errorPrefix + " failed: " + e.what());
    }
}

} // namespace

// ========== Agent 实现 ==========

class Agent::Impl {
public:
    AgentConfig config;
    RunMode mode = RunMode::Unknown;

    std::unique_ptr<RemoteClient> remoteClient;
    std::unique_ptr<StandaloneMode> standaloneMode;
    std::unique_ptr<LocalIpcServer> localIpcServer;
    std::unique_ptr<rpc::RpcDispatcher> remoteDispatcher;
    std::unique_ptr<platform::IScreen> screen;
    // 共享触发器管理器：本地 IPC 与远程 agent 通道看到同一份触发器
    std::unique_ptr<TriggerManager> triggerManager;
};

Agent::Agent() : impl_(std::make_unique<Impl>()) {}

Agent::~Agent() {
    shutdown();
}

bool Agent::initialize(const std::string& configPath) {
    try {
        impl_->config = AgentConfig::loadFromFile(configPath);
    } catch (const std::exception& e) {
        // 配置文件不存在或加载失败，创建默认配置
        spdlog::warn("Failed to load config: {}, using default configuration", e.what());
        impl_->config = AgentConfig{};  // 使用默认值

        // 尝试保存默认配置文件
        if (impl_->config.saveToFile(configPath)) {
            spdlog::info("Created default config file: {}", configPath);
        } else {
            spdlog::warn("Failed to create default config file: {}", configPath);
        }
    }
    return initialize(impl_->config);
}

bool Agent::initialize(const AgentConfig& config) {
    impl_->config = config;
    impl_->mode = config.getRunMode();

    // 共享触发器管理器：命中时推送到 EventBuffer，由本地 IPC 缓冲与远程 sink
    //（initRemoteClient 中注册）分别转发给 GUI 与 Go server。
    impl_->triggerManager = std::make_unique<TriggerManager>();
    impl_->triggerManager->setOnFired([](const TriggerInstance& t) {
        EventBuffer::instance().push("trigger.fired", {
            {"id", t.id},
            {"name", t.config.name},
            {"triggered", t.triggered},
            {"lastTriggerTime", t.lastTriggerTime},
        });
    });

    auto caps = config.getCapabilities();

    spdlog::info("Initializing Agent with capabilities:");
    spdlog::info("  - Remote outbound: {}",
        hasCapability(caps, RunCapability::RemoteOutbound) ? "enabled" : "disabled");
    spdlog::info("  - Local IPC: {}",
        hasCapability(caps, RunCapability::LocalIpc) ? "enabled" : "disabled");
    spdlog::info("  - Standalone script: {}",
        hasCapability(caps, RunCapability::StandaloneScript) ? "enabled" : "disabled");

    // 初始化远端客户端（基于能力）
    if (hasCapability(caps, RunCapability::RemoteOutbound)) {
        if (!initRemoteClient()) {
            spdlog::error("Failed to initialize remote client");
            return false;
        }
    }

    // 初始化单机模式（即使仅用于本地 IPC 也需要 StandaloneMode 实例）
    if (hasCapability(caps, RunCapability::LocalIpc) ||
        hasCapability(caps, RunCapability::StandaloneScript)) {
        if (!initStandaloneMode()) {
            spdlog::error("Failed to initialize standalone mode");
            return false;
        }
    }

    return true;
}

void Agent::shutdown() {
    spdlog::info("Shutting down Agent");

    if (impl_->remoteClient) {
        impl_->remoteClient->stop();
        impl_->remoteClient.reset();
    }

    if (impl_->localIpcServer) {
        impl_->localIpcServer->stop();
        impl_->localIpcServer.reset();
    }

    if (impl_->standaloneMode) {
        impl_->standaloneMode->stop();
        impl_->standaloneMode.reset();
    }

    running_.store(false);
}

bool Agent::start() {
    running_.store(true);

    auto caps = impl_->config.getCapabilities();
    bool success = true;

    // 启动远端客户端（基于能力）
    if (impl_->remoteClient && hasCapability(caps, RunCapability::RemoteOutbound)) {
        if (!impl_->remoteClient->start()) {
            spdlog::error("Failed to start remote client");
            success = false;
        }
    }

    // 启动单机模式（如果需要脚本执行）
    if (impl_->standaloneMode && hasCapability(caps, RunCapability::StandaloneScript)) {
        if (!impl_->standaloneMode->start()) {
            spdlog::error("Failed to start standalone mode");
            success = false;
        }
    }

    // 启动本地 IPC 服务器（基于能力；共享触发器管理器，远程/本地同一份触发器）
    if (impl_->standaloneMode && hasCapability(caps, RunCapability::LocalIpc)) {
        impl_->localIpcServer = std::make_unique<LocalIpcServer>(
            *impl_->standaloneMode, std::string(), impl_->triggerManager.get());
        // 注入 system.getStatus 上下文：GUI 心跳即可刷新远程链路徽标与 IPC 客户端状态
        auto* ipcServer = impl_->localIpcServer.get();
        auto* remoteClient = impl_->remoteClient.get();
        impl_->localIpcServer->setStatusProviders(rpc::RuntimeStatusProviders{
            [remoteClient]() -> bool {
                return remoteClient && remoteClient->isConnected();
            },
            [remoteClient]() -> std::string {
                return remoteClient ? remoteClient->connectionStateName() : "disabled";
            },
            [ipcServer]() -> bool {
                return ipcServer && ipcServer->isClientConnected();
            },
            [mode = impl_->mode]() -> int {
                return static_cast<int>(mode);
            },
        });
        if (!impl_->localIpcServer->start()) {
            spdlog::error("Failed to start local IPC server");
            success = false;
        }
    }

    if (success) {
        spdlog::info("Agent started successfully");
    }

    return success;
}

void Agent::stop() {
    spdlog::info("Stopping Agent");
    running_.store(false);

    if (impl_->remoteClient) impl_->remoteClient->stop();
    if (impl_->localIpcServer) impl_->localIpcServer->stop();
    if (impl_->standaloneMode) impl_->standaloneMode->stop();
}

RemoteClient* Agent::getRemoteClient() {
    return impl_->remoteClient.get();
}

StandaloneMode* Agent::getStandaloneMode() {
    return impl_->standaloneMode.get();
}

RunMode Agent::getMode() const {
    return impl_->mode;
}

const AgentConfig& Agent::getConfig() const {
    return impl_->config;
}

bool Agent::initRemoteClient() {
    impl_->remoteClient = std::make_unique<RemoteClient>(impl_->config.remoteClient);

    impl_->remoteDispatcher = std::make_unique<rpc::RpcDispatcher>();
    impl_->screen = platform::createPlatformScreen();
    if (impl_->screen) {
        rpc::registerScreenshotHandlers(*impl_->remoteDispatcher, *impl_->screen);
    } else {
        rpc::registerScreenshotHandlers(*impl_->remoteDispatcher);
    }
    // trigger.* 复用本地 IPC 的 RPC handler（Dispatcher Reuse）
    if (impl_->triggerManager) {
        rpc::registerTriggerHandlers(*impl_->remoteDispatcher, *impl_->triggerManager);
    }

    // 绑定命令回调，处理 server 下发的命令
    impl_->remoteClient->setCommandCallback([this](const std::string& command, const CommandData& data) {
        return handleRemoteCommand(command, data);
    });

    // 注册远程事件转发：EventBuffer 收到事件时，选择性地以 agent.event 推送到 Go server，
    // 由 server 广播到 Dashboard WS（listener.go handleEvent）。
    // 仅转发 server 关心的事件，避免高频 log.line 淹没 agent 上行链路。
    EventBuffer::instance().setRemoteSink([this](const std::string& method, const nlohmann::json& payload) {
        if (!impl_->remoteClient) return;
        if (method == "trigger.fired") {
            impl_->remoteClient->sendAgentEvent("trigger_fired", payload);
        } else if (method == "script.state_changed") {
            impl_->remoteClient->sendAgentEvent("script_state", payload);
        } else if (method == "script.output") {
            impl_->remoteClient->sendAgentEvent("script_output", payload);
        }
        // log.line / connection.state_changed 为本地相关，不转发
    });

    return true;
}

bool Agent::initStandaloneMode() {
    impl_->standaloneMode = std::make_unique<StandaloneMode>(impl_->config.standalone);
    return true;
}

// ========== 远程命令处理 ==========

CommandResult Agent::handleRemoteCommand(const std::string& command, const CommandData& data) {
    spdlog::info("Received remote command: {}", command);

    if (command == "run_script") {
        // 获取脚本路径
        auto pathIt = data.find("path");
        if (pathIt == data.end() || pathIt->second.empty()) {
            spdlog::warn("run_script missing path parameter");
            return CommandResult::error("missing path parameter");
        }

        if (!impl_->standaloneMode || !impl_->standaloneMode->isRunning()) {
            spdlog::warn("StandaloneMode not available, cannot run script");
            return CommandResult::error("StandaloneMode not available");
        }

        // 加载并启动脚本
        std::string scriptId = impl_->standaloneMode->loadScript(pathIt->second);
        if (!scriptId.empty()) {
            if (impl_->standaloneMode->startScript(scriptId)) {
                spdlog::info("Script started: {} (path: {})", scriptId, pathIt->second);
                return CommandResult::ok("script started: " + scriptId);
            } else {
                spdlog::error("Failed to start script: {}", scriptId);
                return CommandResult::error("failed to start script: " + scriptId);
            }
        } else {
            spdlog::error("Failed to load script: {}", pathIt->second);
            return CommandResult::error("failed to load script: " + pathIt->second);
        }

    } else if (command == "system.shutdown") {
        // 关闭 agent
        spdlog::info("Shutting down agent due to remote command");
        stop();
        return CommandResult::ok("agent shutting down");

    } else if (command == "list_windows") {
        nlohmann::json windows = nlohmann::json::array();
        for (const auto& info : Window::enumerate()) {
            windows.push_back({
                {"handle", encodeWindowHandle(info.handle)},
                {"title", info.title},
                {"isForeground", info.isForeground},
                {"bounds", {
                    {"x", info.bounds.x},
                    {"y", info.bounds.y},
                    {"width", info.bounds.width},
                    {"height", info.bounds.height}
                }}
            });
        }
        return CommandResult::okData(windows.dump());

    } else if (command == "get_status") {
        nlohmann::json scripts = nlohmann::json::array();
        if (impl_->standaloneMode) {
            for (const auto& script : impl_->standaloneMode->listScripts()) {
                scripts.push_back({
                    {"id", script.id},
                    {"path", script.path},
                    {"state", scriptStateToString(script.state)},
                    {"error", script.error},
                    {"uptime", script.uptime}
                });
            }
        }

        nlohmann::json status = {
            {"mode", static_cast<int>(impl_->mode)},
            {"remoteEnabled", impl_->config.enableRemote},
            {"localIpcEnabled", impl_->config.enableLocalIpc},
            {"standaloneScriptEnabled", impl_->config.enableStandaloneScript},
            {"standaloneRunning", impl_->standaloneMode ? impl_->standaloneMode->isRunning() : false},
            {"remoteConnected", impl_->remoteClient ? impl_->remoteClient->isConnected() : false},
            {"remoteState", impl_->remoteClient ? impl_->remoteClient->connectionStateName() : std::string("disabled")},
            {"ipcClientConnected", impl_->localIpcServer ? impl_->localIpcServer->isClientConnected() : false},
            {"scripts", scripts}
        };
        return CommandResult::okData(status.dump());

    } else if (command == "screenshot.capture") {
        if (!impl_->remoteDispatcher) {
            return CommandResult::error("remote dispatcher not available");
        }
        return dispatchViaRpcDispatcher(*impl_->remoteDispatcher, command, data, "screenshot capture");

    } else if (command.rfind("trigger.", 0) == 0) {
        // trigger.list / toggle / add / remove / update：远程通道复用本地 IPC 的
        // RPC handler（Dispatcher Reuse），Dashboard 经 Go server 透传访问。
        if (!impl_->remoteDispatcher || !impl_->triggerManager) {
            return CommandResult::error("trigger handlers not available");
        }
        return dispatchViaRpcDispatcher(*impl_->remoteDispatcher, command, data, "trigger command");

    } else if (command == "stop_script") {
        auto scriptIdIt = data.find("script_id");
        if (scriptIdIt != data.end() && !scriptIdIt->second.empty()) {
            if (impl_->standaloneMode) {
                if (impl_->standaloneMode->stopScript(scriptIdIt->second)) {
                    spdlog::info("Script stopped: {}", scriptIdIt->second);
                    return CommandResult::ok("script stopped: " + scriptIdIt->second);
                } else {
                    spdlog::error("Failed to stop script: {}", scriptIdIt->second);
                    return CommandResult::error("failed to stop script: " + scriptIdIt->second);
                }
            } else {
                return CommandResult::error("StandaloneMode not available");
            }
        }
        return CommandResult::error("missing script_id parameter");
    } else {
        spdlog::warn("Unknown remote command: {}", command);
        return CommandResult::error("unknown command: " + command);
    }
}

} // namespace wingman::runtime
