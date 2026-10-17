#include "android_agent.hpp"

#include "agent/android_screenshot.hpp"
#include "platform/android/android_host_bridge.hpp"
#include "wingman/rpc/asset_handler.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>

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

// dispatchViaRpcDispatcher 把远程命令转成 RPC 请求经共享 dispatcher 执行
//（与桌面 apps/agent/src/agent.cpp 同形；Dispatcher Reuse——远程 agent 命令
// 复用 lib/wingman 注册的同一套 handler，CommandData 的字符串值尽量按
// JSON 解析）
CommandResult dispatchViaRpcDispatcher(rpc::RpcDispatcher& dispatcher,
                                       const std::string& command,
                                       const CommandData& data,
                                       const std::string& errorPrefix) {
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

AndroidAgent::AndroidAgent() = default;

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
    rc->useChallengeAuth = config.useChallengeAuth;

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

    // A2 能力注入：宿主桥（jni_bridge 在 start 前注册全局；未注册时脚本
    // API 降级）+ 模板图根目录
    runner_.setHostBridge(platform::android::globalHostBridge());
    runner_.setFilesDir(config.filesDir);

    // asset.sync 端侧落盘（A3）：模板图片等资产落 <filesDir>/assets/
    //（脚本以 assets/<name> 相对引用——findImage 相对路径按 filesDir
    // 解析，与桌面 scriptDir/assets 的引用形态一致）
    assetDispatcher_ = std::make_unique<rpc::RpcDispatcher>();
    rpc::registerAssetHandlers(
        *assetDispatcher_,
        (std::filesystem::path(config.filesDir) / "assets").generic_string());

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

    } else if (command == "screenshot.capture") {
        // A2：与桌面同形返回（JPEG q82 + base64 data URI），Go server
        // workflow 的 screenshot 步骤零改动消费；桥在命令时点解析，
        // 支持服务连接晚于 agent 启动的时序
        return handleScreenshotCapture(
            data, platform::android::globalHostBridge());

    } else if (command.rfind("asset.", 0) == 0) {
        // asset.sync：server 下发模板图片等资产落盘 filesDir/assets
        //（远程通道复用 RPC handler，与桌面 agent.cpp 的 asset.* 分支同形）
        if (!assetDispatcher_) {
            return CommandResult::error("asset handlers not available");
        }
        return dispatchViaRpcDispatcher(*assetDispatcher_, command, data, "asset command");

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
