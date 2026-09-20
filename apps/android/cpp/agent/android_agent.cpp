#include "android_agent.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>

namespace wingman::android {

using runtime::CommandData;
using runtime::CommandResult;
using runtime::RemoteClient;
using runtime::RemoteClientConfig;

namespace {

// server 下发的 path 形如 "scripts/hello.lua"：取 basename 去扩展名，
// 作为日志回传的 scriptId（与桌面 execution_id 展示口径一致）
std::string scriptIdFromPath(const std::string& path) {
    std::string name = path;
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
        name = name.substr(slash + 1);
    }
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos && name.substr(dot) == ".lua") {
        name = name.substr(0, dot);
    }
    return name;
}

} // namespace

AndroidAgent::~AndroidAgent() {
    stop();
}

bool AndroidAgent::start(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (client_ && client_->isRunning()) {
        return true;
    }

    config_ = config;

    auto rc = std::make_unique<RemoteClientConfig>();
    rc->serverIp = config.serverIp;
    rc->serverPort = config.serverPort;

    client_ = std::make_unique<RemoteClient>(*rc);
    if (!config.agentId.empty() || !config.hostname.empty()) {
        client_->setIdentity(config.agentId, config.hostname);
    }
    nlohmann::json metadata;
    metadata["platform"] = config.platform;
    if (!config.capabilitiesJson.empty()) {
        try {
            metadata["capabilities"] = nlohmann::json::parse(config.capabilitiesJson);
        } catch (...) {
            metadata["capabilities"] = nlohmann::json::object();
        }
    }
    client_->setRegisterMetadata(metadata);
    if (!config.authToken.empty()) {
        client_->setAuthToken(config.authToken);
    }

    // 脚本输出 → agent.event "script_output"（字段与桌面 standalone_mode.cpp
    // 的 script.output 一致：id/output 加 level；server handleEvent 落库
    // ExecutionLog（scriptId/message/level）并转发 Dashboard）
    runner_.setOutputCallback(
        [this](const std::string& line) {
            std::string scriptId;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                scriptId = activeScriptId_;
            }
            sendScriptOutput(scriptId, line, "");
        });

    client_->setCommandCallback(
        [this](const std::string& command, const CommandData& data) {
            return onCommand(command, data);
        });

    if (!client_->start()) {
        client_.reset();
        return false;
    }
    return true;
}

void AndroidAgent::stop() {
    runner_.stop();  // 置停止标志（幂等）；执行线程由 ScriptRunner 析构 join
    std::unique_ptr<RemoteClient> client;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client = std::move(client_);
    }
    if (client) {
        client->stop();
    }
}

bool AndroidAgent::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return client_ && client_->isRunning();
}

bool AndroidAgent::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return client_ && client_->isConnected();
}

bool AndroidAgent::isScriptRunning() const {
    return runner_.isRunning();
}

std::string AndroidAgent::statusJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json status = {
        {"running", client_ && client_->isRunning()},
        {"connected", client_ && client_->isConnected()},
        {"connectionState", client_ ? client_->connectionStateName() : "disconnected"},
        {"script", {
            {"running", runner_.isRunning()},
            {"executionId", runner_.executionId()},
        }},
    };
    return status.dump();
}

CommandResult AndroidAgent::onCommand(const std::string& command,
                                      const CommandData& data) {
    if (command == "run_script") {
        // Android 无服务器文件系统：必须走 content 内联（§3.2）
        const auto contentIt = data.find("content");
        if (contentIt == data.end() || contentIt->second.empty()) {
            return CommandResult::error(
                "missing content: android agent cannot access server filesystem");
        }
        std::string language = "lua";
        const auto langIt = data.find("language");
        if (langIt != data.end() && !langIt->second.empty()) {
            language = langIt->second;
        }
        std::string path = "inline.lua";
        const auto pathIt = data.find("path");
        if (pathIt != data.end() && !pathIt->second.empty()) {
            path = pathIt->second;
        }
        const std::string scriptId = scriptIdFromPath(path);
        const std::string executionId = nextExecutionId();
        if (!runner_.start(executionId, contentIt->second, language)) {
            return CommandResult::error(
                "script already running or unsupported language: " + language);
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            activeScriptId_ = scriptId;
        }
        return CommandResult::ok("script started: " + executionId);

    } else if (command == "stop_script") {
        runner_.stop();
        return CommandResult::ok("stop requested");

    } else if (command == "system.shutdown") {
        // 服务生命周期归 Kotlin 前台服务管理，不响应远端关停
        return CommandResult::error("system.shutdown not supported on android agent");
    }

    return CommandResult::error("command not supported: " + command);
}

void AndroidAgent::sendScriptOutput(const std::string& scriptId,
                                    const std::string& message,
                                    const std::string& level) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!client_) {
        return;
    }
    nlohmann::json payload = {
        {"scriptId", scriptId},
        {"message", message},
    };
    if (!level.empty()) {
        payload["level"] = level;
    }
    client_->sendAgentEvent("script_output", payload);
}

std::string AndroidAgent::nextExecutionId() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    return "exec_" + std::to_string(seconds) + "_" + std::to_string(++executionSeq_);
}

} // namespace wingman::android
