#include "wingman/node_status.hpp"
#include <nlohmann/json.hpp>

namespace wingman {

// ========== GameWindowStatus Implementation ==========

std::string GameWindowStatus::toJson() const {
    nlohmann::json j;
    j["title"] = title;
    j["processName"] = processName;
    j["handle"] = handle;
    j["x"] = x;
    j["y"] = y;
    j["width"] = width;
    j["height"] = height;
    j["isForeground"] = isForeground;
    return j.dump();
}

GameWindowStatus GameWindowStatus::fromJson(const std::string& json) {
    GameWindowStatus status{};
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("title")) status.title = j["title"];
        if (j.contains("processName")) status.processName = j["processName"];
        if (j.contains("handle")) status.handle = j["handle"];
        if (j.contains("x")) status.x = j["x"];
        if (j.contains("y")) status.y = j["y"];
        if (j.contains("width")) status.width = j["width"];
        if (j.contains("height")) status.height = j["height"];
        if (j.contains("isForeground")) status.isForeground = j["isForeground"];
    } catch (...) {
        // 解析失败，返回默认值
    }
    return status;
}

// ========== ScriptStatus Implementation ==========

std::string ScriptStatus::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["state"] = state;
    j["uptimeSeconds"] = uptimeSeconds;
    j["lastError"] = lastError;
    return j.dump();
}

ScriptStatus ScriptStatus::fromJson(const std::string& json) {
    ScriptStatus status{};
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("name")) status.name = j["name"];
        if (j.contains("state")) status.state = j["state"];
        if (j.contains("uptimeSeconds")) status.uptimeSeconds = j["uptimeSeconds"];
        if (j.contains("lastError")) status.lastError = j["lastError"];
    } catch (...) {
        // 解析失败，返回默认值
    }
    return status;
}

// ========== NodeHeartbeat Implementation ==========

std::string NodeHeartbeat::toJson() const {
    nlohmann::json j;
    j["nodeId"] = nodeId;
    j["hostname"] = hostname;
    j["timestamp"] = timestamp;

    // 状态转换为字符串
    switch (status) {
        case NodeState::Online: j["status"] = "online"; break;
        case NodeState::Busy: j["status"] = "busy"; break;
        case NodeState::Idle: j["status"] = "idle"; break;
        case NodeState::Error: j["status"] = "error"; break;
        case NodeState::Offline: j["status"] = "offline"; break;
    }

    j["cpuUsage"] = cpuUsage;
    j["memoryUsage"] = memoryUsage;
    j["version"] = version;

    // 游戏窗口
    nlohmann::json gamesArray = nlohmann::json::array();
    for (const auto& game : games) {
        gamesArray.push_back(nlohmann::json::parse(game.toJson()));
    }
    j["games"] = gamesArray;

    // 脚本状态
    nlohmann::json scriptsArray = nlohmann::json::array();
    for (const auto& script : scripts) {
        scriptsArray.push_back(nlohmann::json::parse(script.toJson()));
    }
    j["scripts"] = scriptsArray;

    return j.dump();
}

NodeHeartbeat NodeHeartbeat::fromJson(const std::string& json) {
    NodeHeartbeat heartbeat{};
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("nodeId")) heartbeat.nodeId = j["nodeId"];
        if (j.contains("hostname")) heartbeat.hostname = j["hostname"];
        if (j.contains("timestamp")) heartbeat.timestamp = j["timestamp"];
        if (j.contains("cpuUsage")) heartbeat.cpuUsage = j["cpuUsage"];
        if (j.contains("memoryUsage")) heartbeat.memoryUsage = j["memoryUsage"];
        if (j.contains("version")) heartbeat.version = j["version"];

        // 解析状态字符串
        if (j.contains("status")) {
            std::string statusStr = j["status"];
            if (statusStr == "online") heartbeat.status = NodeState::Online;
            else if (statusStr == "busy") heartbeat.status = NodeState::Busy;
            else if (statusStr == "idle") heartbeat.status = NodeState::Idle;
            else if (statusStr == "error") heartbeat.status = NodeState::Error;
            else if (statusStr == "offline") heartbeat.status = NodeState::Offline;
        }

        // 解析游戏窗口
        if (j.contains("games") && j["games"].is_array()) {
            for (const auto& gameJson : j["games"]) {
                heartbeat.games.push_back(GameWindowStatus::fromJson(gameJson.dump()));
            }
        }

        // 解析脚本状态
        if (j.contains("scripts") && j["scripts"].is_array()) {
            for (const auto& scriptJson : j["scripts"]) {
                heartbeat.scripts.push_back(ScriptStatus::fromJson(scriptJson.dump()));
            }
        }
    } catch (...) {
        // 解析失败，返回默认值
    }
    return heartbeat;
}

// ========== ServerCommandData Implementation ==========

std::string ServerCommandData::toJson() const {
    nlohmann::json j;
    j["timestamp"] = timestamp;

    // 命令转换为字符串
    switch (command) {
        case ServerCommand::None: j["command"] = "none"; break;
        case ServerCommand::StartScript: j["command"] = "start_script"; break;
        case ServerCommand::StopScript: j["command"] = "stop_script"; break;
        case ServerCommand::PauseScript: j["command"] = "pause_script"; break;
        case ServerCommand::ResumeScript: j["command"] = "resume_script"; break;
        case ServerCommand::Restart: j["command"] = "restart"; break;
        case ServerCommand::Shutdown: j["command"] = "shutdown"; break;
        case ServerCommand::UpdateConfig: j["command"] = "update_config"; break;
    }

    j["scriptPath"] = scriptPath;
    j["configData"] = configData;
    return j.dump();
}

ServerCommandData ServerCommandData::fromJson(const std::string& json) {
    ServerCommandData data{};
    data.command = ServerCommand::None;
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        if (j.contains("timestamp")) data.timestamp = j["timestamp"];
        if (j.contains("scriptPath")) data.scriptPath = j["scriptPath"];
        if (j.contains("configData")) data.configData = j["configData"];

        // 解析命令字符串
        if (j.contains("command")) {
            std::string commandStr = j["command"];
            if (commandStr == "start_script") data.command = ServerCommand::StartScript;
            else if (commandStr == "stop_script") data.command = ServerCommand::StopScript;
            else if (commandStr == "pause_script") data.command = ServerCommand::PauseScript;
            else if (commandStr == "resume_script") data.command = ServerCommand::ResumeScript;
            else if (commandStr == "restart") data.command = ServerCommand::Restart;
            else if (commandStr == "shutdown") data.command = ServerCommand::Shutdown;
            else if (commandStr == "update_config") data.command = ServerCommand::UpdateConfig;
        }
    } catch (...) {
        // 解析失败，返回默认值
    }
    return data;
}

} // namespace wingman
