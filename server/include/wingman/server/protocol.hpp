#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace wingman::server {

// ========== 错误码定义 ==========
// 0-1023 系统保留，1024+ 用户自定义
enum class ErrorCode : int {
    OK              = 0,    // 成功
    UNKNOWN         = 1,    // 未知错误
    INVALID_REQUEST = 2,    // 请求格式错误或参数无效
    NOT_FOUND       = 3,    // 资源未找到
    TIMEOUT         = 4,    // 操作超时
    BUSY            = 5,    // 服务忙碌
    NOT_AUTHORIZED  = 6,    // 未授权
    ALREADY_EXISTS  = 7,    // 资源已存在
    FAILED          = 8,    // 操作失败
    DISCONNECTED    = 9,    // 连接断开
    RATE_LIMITED    = 10,   // 请求频率限制
    USER_DEFINED_START = 1024  // 用户自定义错误码起始值
};

// 错误码转字符串
inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::UNKNOWN: return "UNKNOWN";
        case ErrorCode::INVALID_REQUEST: return "INVALID_REQUEST";
        case ErrorCode::NOT_FOUND: return "NOT_FOUND";
        case ErrorCode::TIMEOUT: return "TIMEOUT";
        case ErrorCode::BUSY: return "BUSY";
        case ErrorCode::NOT_AUTHORIZED: return "NOT_AUTHORIZED";
        case ErrorCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
        case ErrorCode::FAILED: return "FAILED";
        case ErrorCode::DISCONNECTED: return "DISCONNECTED";
        case ErrorCode::RATE_LIMITED: return "RATE_LIMITED";
        default: return "USER_DEFINED_" + std::to_string(static_cast<int>(code));
    }
}

// ========== 消息类型 ==========
enum class RequestType {
    // 控制指令（已有）
    kExecuteScript,
    kStopScript,
    kGetStatus,
    kScreenshot,
    kMouseMove,
    kMouseClick,
    kKeyPress,

    // 会话管理（新增）
    kRegister,       // 客户端注册
    kHeartbeat,      // 心跳
    kGetAgents,      // 获取所有在线客户端列表
    kSyncTask,       // 同步任务状态
    kShutdown,       // 关闭客户端

    kUnknown
};

// ========== Agent 状态 ==========
enum class AgentStatus {
    Offline,
    Online,
    Idle,
    Busy,
    Error
};

inline std::string agentStatusToString(AgentStatus status) {
    switch (status) {
        case AgentStatus::Offline: return "offline";
        case AgentStatus::Online: return "online";
        case AgentStatus::Idle: return "idle";
        case AgentStatus::Busy: return "busy";
        case AgentStatus::Error: return "error";
        default: return "unknown";
    }
}

inline AgentStatus parseAgentStatus(const std::string& str) {
    if (str == "online") return AgentStatus::Online;
    if (str == "idle") return AgentStatus::Idle;
    if (str == "busy") return AgentStatus::Busy;
    if (str == "error") return AgentStatus::Error;
    return AgentStatus::Offline;
}

// ========== Agent 信息 ==========
struct AgentInfo {
    std::string agentId;              // 客户端 ID
    std::string hostname;             // 主机名
    std::string ip;                   // IP 地址
    AgentStatus status = AgentStatus::Online;
    nlohmann::json currentTask;       // 当前任务
    std::chrono::system_clock::time_point lastSeen;

    // 序列化
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["agent_id"] = agentId;
        j["hostname"] = hostname;
        j["ip"] = ip;
        j["status"] = agentStatusToString(status);
        j["current_task"] = currentTask;
        j["last_seen"] = std::chrono::system_clock::to_time_t(lastSeen);
        return j;
    }

    // 反序列化
    static AgentInfo fromJson(const nlohmann::json& j) {
        AgentInfo info;
        if (j.contains("agent_id")) info.agentId = j["agent_id"];
        if (j.contains("hostname")) info.hostname = j["hostname"];
        if (j.contains("ip")) info.ip = j["ip"];
        if (j.contains("status")) info.status = parseAgentStatus(j["status"]);
        if (j.contains("current_task")) info.currentTask = j["current_task"];
        if (j.contains("last_seen")) {
            info.lastSeen = std::chrono::system_clock::from_time_t(j["last_seen"].get<uint64_t>());
        }
        return info;
    }
};

// ========== 请求消息 ==========
struct Request {
    RequestType type = RequestType::kUnknown;
    std::string id;                    // 请求 ID
    uint64_t timestamp = 0;            // 发送时间戳
    std::string agentId;               // 发送者 ID（已注册客户端）
    int priority = 0;                  // 优先级（可选）
    nlohmann::json data;               // 业务数据

    static Request fromJson(const std::string& jsonStr);
    std::string toJson() const;

    // 生成唯一 ID
    static std::string generateId() {
        static std::atomic<uint64_t> counter{0};
        return "req_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count())
               + "_" + std::to_string(counter++);
    }
};

// ========== 响应消息 ==========
struct Response {
    std::string requestId;             // 对应的请求 ID
    ErrorCode code = ErrorCode::OK;    // 错误码（数字）
    uint64_t timestamp = 0;            // 响应时间戳
    std::string message;               // 可读的错误描述
    nlohmann::json data;               // 业务数据（成功时）

    std::string toJson() const;
    static Response fromJson(const std::string& jsonStr);

    // 快速创建响应
    static Response ok(const std::string& requestId, const nlohmann::json& data = nullptr) {
        Response resp;
        resp.requestId = requestId;
        resp.code = ErrorCode::OK;
        resp.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        resp.data = data;
        return resp;
    }

    static Response error(const std::string& requestId, ErrorCode code, const std::string& message) {
        Response resp;
        resp.requestId = requestId;
        resp.code = code;
        resp.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        resp.message = message;
        return resp;
    }
};

// ========== 协议辅助类 ==========
class Protocol {
public:
    // 编码响应（信封协议）
    static std::string encode(const Response& response);

    // 解码请求
    static std::optional<Request> decode(const std::string& buffer);

    // 解析请求类型
    static RequestType parseRequestType(const std::string& type);
    static std::string requestTypeToString(RequestType type);

    // 获取当前时间戳
    static uint64_t now() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
};

} // namespace wingman::server
