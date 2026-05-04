#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace wingman::server {

// Request types
enum class RequestType {
    kExecuteScript,
    kStopScript,
    kGetStatus,
    kPing,
    kScreenshot,
    kMouseMove,
    kMouseClick,
    kKeyPress,
    kUnknown
};

// Response status
enum class ResponseStatus {
    kOk,
    kError,
    kNotFound
};

struct Request {
    RequestType type = RequestType::kUnknown;
    std::string id;
    nlohmann::json data;

    static Request fromJson(const std::string& jsonStr);
    std::string toJson() const;
};

struct Response {
    std::string requestId;
    ResponseStatus status = ResponseStatus::kOk;
    std::string message;
    nlohmann::json data;

    std::string toJson() const;
    static Response fromJson(const std::string& jsonStr);
};

// Protocol helper
class Protocol {
public:
    static std::string encode(const Response& response);
    static std::optional<Response> decode(const std::string& buffer);

    static RequestType parseRequestType(const std::string& type);
    static std::string requestTypeToString(RequestType type);
};

} // namespace wingman::server
