#include "wingman/server/protocol.hpp"
#include <sstream>

namespace wingman::server {

// Request implementation
Request Request::fromJson(const std::string& jsonStr) {
    Request request;
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("type")) {
            request.type = Protocol::parseRequestType(j["type"].get<std::string>());
        }
        if (j.contains("id")) {
            request.id = j["id"].get<std::string>();
        }
        if (j.contains("data")) {
            request.data = j["data"];
        }
    } catch (const std::exception& e) {
        request.type = RequestType::kUnknown;
    }
    return request;
}

std::string Request::toJson() const {
    nlohmann::json j;
    j["type"] = Protocol::requestTypeToString(type);
    j["id"] = id;
    j["data"] = data;
    return j.dump();
}

// Response implementation
std::string Response::toJson() const {
    nlohmann::json j;
    j["requestId"] = requestId;
    j["status"] = (status == ResponseStatus::kOk) ? "ok" :
                  (status == ResponseStatus::kError) ? "error" : "not_found";
    j["message"] = message;
    if (!data.is_null()) {
        j["data"] = data;
    }
    return j.dump();
}

Response Response::fromJson(const std::string& jsonStr) {
    Response response;
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("requestId")) {
            response.requestId = j["requestId"].get<std::string>();
        }
        if (j.contains("status")) {
            std::string status = j["status"].get<std::string>();
            if (status == "ok") response.status = ResponseStatus::kOk;
            else if (status == "error") response.status = ResponseStatus::kError;
            else if (status == "not_found") response.status = ResponseStatus::kNotFound;
        }
        if (j.contains("message")) {
            response.message = j["message"].get<std::string>();
        }
        if (j.contains("data")) {
            response.data = j["data"];
        }
    } catch (const std::exception& e) {
        response.status = ResponseStatus::kError;
        response.message = e.what();
    }
    return response;
}

// Protocol implementation
std::string Protocol::encode(const Response& response) {
    // Format: length_in_hex + '\n' + json + '\n'
    std::string json = response.toJson();
    std::ostringstream oss;
    oss << std::hex << json.length() << "\n" << json << "\n";
    return oss.str();
}

std::optional<Response> Protocol::decode(const std::string& buffer) {
    std::istringstream iss(buffer);
    std::string line;

    // Read length
    if (!std::getline(iss, line)) return std::nullopt;
    size_t length = std::stoul(line, nullptr, 16);

    // Read JSON
    if (!std::getline(iss, line)) return std::nullopt;
    if (line.length() < length) return std::nullopt;

    return Response::fromJson(line);
}

RequestType Protocol::parseRequestType(const std::string& type) {
    if (type == "execute_script") return RequestType::kExecuteScript;
    if (type == "stop_script") return RequestType::kStopScript;
    if (type == "get_status") return RequestType::kGetStatus;
    if (type == "ping") return RequestType::kPing;
    if (type == "screenshot") return RequestType::kScreenshot;
    if (type == "mouse_move") return RequestType::kMouseMove;
    if (type == "mouse_click") return RequestType::kMouseClick;
    if (type == "key_press") return RequestType::kKeyPress;
    return RequestType::kUnknown;
}

std::string Protocol::requestTypeToString(RequestType type) {
    switch (type) {
        case RequestType::kExecuteScript: return "execute_script";
        case RequestType::kStopScript: return "stop_script";
        case RequestType::kGetStatus: return "get_status";
        case RequestType::kPing: return "ping";
        case RequestType::kScreenshot: return "screenshot";
        case RequestType::kMouseMove: return "mouse_move";
        case RequestType::kMouseClick: return "mouse_click";
        case RequestType::kKeyPress: return "key_press";
        default: return "unknown";
    }
}

} // namespace wingman::server
