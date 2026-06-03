#include "wingman/runtime/agent.hpp"
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

    spdlog::info("Initializing Agent, mode: {}",
        impl_->mode == RunMode::Remote ? "remote" : "standalone");

    // 初始化对应模式
    if (impl_->mode == RunMode::Remote) {
        if (!initRemoteClient()) {
            spdlog::error("Failed to initialize remote mode");
            return false;
        }
    }

    if (impl_->mode == RunMode::Standalone) {
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

    if (impl_->standaloneMode) {
        impl_->standaloneMode->stop();
        impl_->standaloneMode.reset();
    }

    running_.store(false);
}

bool Agent::start() {
    running_.store(true);

    bool success = true;

    if (impl_->remoteClient && impl_->mode == RunMode::Remote) {
        if (!impl_->remoteClient->start()) {
            spdlog::error("Failed to start remote mode");
            success = false;
        }
    }

    if (impl_->standaloneMode && impl_->mode == RunMode::Standalone) {
        if (!impl_->standaloneMode->start()) {
            spdlog::error("Failed to start standalone mode");
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
    return true;
}

bool Agent::initStandaloneMode() {
    impl_->standaloneMode = std::make_unique<StandaloneMode>(impl_->config.standalone);
    return true;
}

} // namespace wingman::runtime
