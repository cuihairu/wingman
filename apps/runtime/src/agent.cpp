#include "wingman/runtime/agent.hpp"
#include "wingman/runtime/local_ipc_server.hpp"
#include "wingman/runtime/remote_client.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include <spdlog/spdlog.h>

namespace wingman::runtime {

// ========== Agent 实现 ==========

class Agent::Impl {
public:
    AgentConfig config;
    RunMode mode = RunMode::Unknown;

    std::unique_ptr<RemoteClient> remoteClient;
    std::unique_ptr<StandaloneMode> standaloneMode;
    std::unique_ptr<LocalIpcServer> localIpcServer;
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

    // 启动本地 IPC 服务器（基于能力）
    if (impl_->standaloneMode && hasCapability(caps, RunCapability::LocalIpc)) {
        impl_->localIpcServer = std::make_unique<LocalIpcServer>(*impl_->standaloneMode);
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

    // 绑定命令回调，处理 server 下发的命令
    impl_->remoteClient->setCommandCallback([this](const std::string& command, const CommandData& data) {
        return handleRemoteCommand(command, data);
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
        // TODO: 实现窗口列表功能
        spdlog::debug("list_windows command not yet implemented");
        return CommandResult::error("list_windows not implemented");

    } else if (command == "get_status") {
        // TODO: 实现状态查询功能
        spdlog::debug("get_status command not yet implemented");
        return CommandResult::error("get_status not implemented");

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
