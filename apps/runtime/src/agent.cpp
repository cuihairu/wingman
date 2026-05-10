#include "wingman/runtime/agent.hpp"
#include "wingman/runtime/active_mode.hpp"
#include "wingman/runtime/passive_mode.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace wingman::runtime {

// ========== Agent 实现 ==========

class Agent::Impl {
public:
    AgentConfig config;
    RunMode mode = RunMode::Unknown;

    std::unique_ptr<ActiveMode> activeMode;
    std::unique_ptr<PassiveMode> passiveMode;
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
        impl_->mode == RunMode::Active ? "active" :
        impl_->mode == RunMode::Passive ? "passive" :
        impl_->mode == RunMode::Standalone ? "standalone" : "both");

    // 初始化对应模式
    if (impl_->mode == RunMode::Active || impl_->mode == RunMode::Both) {
        if (!initActiveMode()) {
            spdlog::error("Failed to initialize active mode");
            return false;
        }
    }

    if (impl_->mode == RunMode::Passive || impl_->mode == RunMode::Both) {
        if (!initPassiveMode()) {
            spdlog::error("Failed to initialize passive mode");
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

    if (impl_->activeMode) {
        impl_->activeMode->stop();
        impl_->activeMode.reset();
    }

    if (impl_->passiveMode) {
        impl_->passiveMode->stop();
        impl_->passiveMode.reset();
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

    if (impl_->activeMode && (impl_->mode == RunMode::Active || impl_->mode == RunMode::Both)) {
        if (!impl_->activeMode->start()) {
            spdlog::error("Failed to start active mode");
            success = false;
        }
    }

    if (impl_->passiveMode && (impl_->mode == RunMode::Passive || impl_->mode == RunMode::Both)) {
        if (!impl_->passiveMode->start()) {
            spdlog::error("Failed to start passive mode");
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

    if (impl_->activeMode) impl_->activeMode->stop();
    if (impl_->passiveMode) impl_->passiveMode->stop();
    if (impl_->standaloneMode) impl_->standaloneMode->stop();
}

ActiveMode* Agent::getActiveMode() {
    return impl_->activeMode.get();
}

PassiveMode* Agent::getPassiveMode() {
    return impl_->passiveMode.get();
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

bool Agent::initActiveMode() {
    impl_->activeMode = std::make_unique<ActiveMode>(impl_->config.active);
    return true;
}

bool Agent::initPassiveMode() {
    impl_->passiveMode = std::make_unique<PassiveMode>(impl_->config.passive);

    // 设置消息处理器
    impl_->passiveMode->setMessageHandler([this](const std::string& sessionId, const std::vector<uint8_t>& data) {
        return handleMessage(sessionId, data);
    });

    return true;
}

bool Agent::initStandaloneMode() {
    impl_->standaloneMode = std::make_unique<StandaloneMode>(impl_->config.standalone);
    return true;
}

// ========== TCP 消息处理 ==========

std::vector<uint8_t> Agent::handleMessage(const std::string& sessionId, const std::vector<uint8_t>& data) {
    nlohmann::json response;
    response["success"] = false;
    response["message"] = "unknown error";

    try {
        // 解析 JSON 请求
        nlohmann::json request = nlohmann::json::parse(data.begin(), data.end());

        if (!request.contains("type")) {
            response["message"] = "missing 'type' field";
            std::string responseStr = response.dump();
            return std::vector<uint8_t>(responseStr.begin(), responseStr.end());
        }

        std::string type = request["type"];
        spdlog::debug("Received message type: {}", type);

        // 路由消息类型
        if (type == "ping") {
            response["success"] = true;
            response["message"] = "pong";
            response["data"]["status"] = "ok";
            response["data"]["mode"] = impl_->mode == RunMode::Active ? "active" :
                                      impl_->mode == RunMode::Passive ? "passive" :
                                      impl_->mode == RunMode::Standalone ? "standalone" : "both";
        }
        else if (type == "get_status") {
            response["success"] = true;
            response["data"]["running"] = running_.load();
            response["data"]["mode"] = impl_->mode == RunMode::Active ? "active" :
                                      impl_->mode == RunMode::Passive ? "passive" :
                                      impl_->mode == RunMode::Standalone ? "standalone" : "both";
        }
        else if (type == "list_scripts") {
            // TODO: 实现 ScriptManager 集成
            response["success"] = true;
            response["data"]["scripts"] = nlohmann::json::array();
        }
        else if (type == "run_script") {
            if (!request.contains("path")) {
                response["message"] = "missing 'path' field";
            } else {
                // TODO: 实现 ScriptManager 集成
                std::string path = request["path"];
                response["success"] = true;
                response["message"] = "script execution started";
                response["data"]["script_id"] = "script_" + path;
            }
        }
        else if (type == "stop_script") {
            if (!request.contains("script_id")) {
                response["message"] = "missing 'script_id' field";
            } else {
                // TODO: 实现 ScriptManager 集成
                std::string scriptId = request["script_id"];
                response["success"] = true;
                response["message"] = "script stopped";
                response["data"]["script_id"] = scriptId;
            }
        }
        else if (type == "get_script_status") {
            if (!request.contains("script_id")) {
                response["message"] = "missing 'script_id' field";
            } else {
                // TODO: 实现 ScriptManager 集成
                std::string scriptId = request["script_id"];
                response["success"] = true;
                response["data"]["script_id"] = scriptId;
                response["data"]["status"] = "stopped";
            }
        }
        else {
            response["message"] = "unknown message type: " + type;
        }

    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("JSON parse error: {}", e.what());
        response["message"] = "json parse error: " + std::string(e.what());
    } catch (const std::exception& e) {
        spdlog::error("Message handling error: {}", e.what());
        response["message"] = "error: " + std::string(e.what());
    }

    // 转换为 JSON 字符串并返回
    std::string responseStr = response.dump();
    return std::vector<uint8_t>(responseStr.begin(), responseStr.end());
}

} // namespace wingman::runtime
