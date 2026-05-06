#include "wingman/server/protocol.hpp"
#include <sstream>
#include <iomanip>

namespace wingman::server {

// ========== Request 实现 ==========
std::string Request::toJson() const {
    nlohmann::json j;
    j["type"] = Protocol::requestTypeToString(type);
    j["id"] = id;
    j["timestamp"] = timestamp;
    if (!agentId.empty()) j["agent_id"] = agentId;
    if (priority != 0) j["priority"] = priority;
    if (!data.is_null()) j["data"] = data;
    return j.dump();
}

Request Request::fromJson(const std::string& jsonStr) {
    Request request;
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("type")) {
            request.type = Protocol::parseRequestType(j["type"].get<std::string>());
        }
        if (j.contains("id")) request.id = j["id"].get<std::string>();
        if (j.contains("timestamp")) request.timestamp = j["timestamp"].get<uint64_t>();
        if (j.contains("agent_id")) request.agentId = j["agent_id"].get<std::string>();
        if (j.contains("priority")) request.priority = j["priority"].get<int>();
        if (j.contains("data")) request.data = j["data"];
    } catch (const std::exception& e) {
        request.type = RequestType::kUnknown;
    }
    return request;
}

// ========== Response 实现 ==========
std::string Response::toJson() const {
    nlohmann::json j;
    j["request_id"] = requestId;
    j["code"] = static_cast<int>(code);
    j["timestamp"] = timestamp;
    if (!message.empty()) j["message"] = message;
    if (!data.is_null()) j["data"] = data;
    return j.dump();
}

Response Response::fromJson(const std::string& jsonStr) {
    Response response;
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("request_id")) response.requestId = j["request_id"].get<std::string>();
        if (j.contains("code")) response.code = static_cast<ErrorCode>(j["code"].get<int>());
        if (j.contains("timestamp")) response.timestamp = j["timestamp"].get<uint64_t>();
        if (j.contains("message")) response.message = j["message"].get<std::string>();
        if (j.contains("data")) response.data = j["data"];
    } catch (const std::exception& e) {
        response.code = ErrorCode::UNKNOWN;
        response.message = e.what();
    }
    return response;
}

// ========== Protocol 实现 ==========
std::string Protocol::encode(const Response& response) {
    // Format: length_in_hex + '\n' + json + '\n'
    std::string json = response.toJson();
    std::ostringstream oss;
    oss << std::hex << json.length() << "\n" << json << "\n";
    return oss.str();
}

std::optional<Request> Protocol::decode(const std::string& buffer) {
    std::istringstream iss(buffer);
    std::string line;

    // Read length
    if (!std::getline(iss, line)) return std::nullopt;
    size_t length = std::stoul(line, nullptr, 16);

    // Read JSON
    if (!std::getline(iss, line)) return std::nullopt;
    if (line.length() < length) return std::nullopt;

    return Request::fromJson(line);
}

RequestType Protocol::parseRequestType(const std::string& type) {
    if (type == "execute_script") return RequestType::kExecuteScript;
    if (type == "stop_script") return RequestType::kStopScript;
    if (type == "get_status") return RequestType::kGetStatus;
    if (type == "screenshot") return RequestType::kScreenshot;
    if (type == "mouse_move") return RequestType::kMouseMove;
    if (type == "mouse_click") return RequestType::kMouseClick;
    if (type == "key_press") return RequestType::kKeyPress;
    if (type == "register") return RequestType::kRegister;
    if (type == "heartbeat") return RequestType::kHeartbeat;
    if (type == "get_agents") return RequestType::kGetAgents;
    if (type == "sync_task") return RequestType::kSyncTask;
    if (type == "shutdown") return RequestType::kShutdown;
    return RequestType::kUnknown;
}

std::string Protocol::requestTypeToString(RequestType type) {
    switch (type) {
        case RequestType::kExecuteScript: return "execute_script";
        case RequestType::kStopScript: return "stop_script";
        case RequestType::kGetStatus: return "get_status";
        case RequestType::kScreenshot: return "screenshot";
        case RequestType::kMouseMove: return "mouse_move";
        case RequestType::kMouseClick: return "mouse_click";
        case RequestType::kKeyPress: return "key_press";
        case RequestType::kRegister: return "register";
        case RequestType::kHeartbeat: return "heartbeat";
        case RequestType::kGetAgents: return "get_agents";
        case RequestType::kSyncTask: return "sync_task";
        case RequestType::kShutdown: return "shutdown";
        default: return "unknown";
    }
}

} // namespace wingman::server
