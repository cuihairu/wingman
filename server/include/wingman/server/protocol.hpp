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

    // 工作流管理
    kSubmitWorkflow,   // 提交工作流
    kCancelWorkflow,   // 取消工作流
    kGetWorkflow,      // 获取工作流状态
    kGetNextTask,      // 获取下一步任务
    kReportProgress,   // 报告任务进度
    kCompleteTask,     // 完成任务
    kFailTask,         // 任务失败

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

// ========== 资源状态定义 ==========
struct ResourceStats {
    // CPU
    double cpuUsage = 0.0;           // CPU 使用率 0-100
    int cpuCores = 0;                // CPU 核心数
    std::string cpuModel;            // CPU 型号

    // 内存
    uint64_t totalMemory = 0;        // 总内存 (字节)
    uint64_t availableMemory = 0;    // 可用内存 (字节)
    double memoryUsage = 0.0;        // 内存使用率 0-100

    // 硬盘
    uint64_t totalDisk = 0;          // 总磁盘空间 (字节)
    uint64_t availableDisk = 0;      // 可用磁盘空间 (字节)
    double diskUsage = 0.0;          // 磁盘使用率 0-100

    // 网络
    uint64_t networkUp = 0;          // 上行速度 (字节/秒)
    uint64_t networkDown = 0;        // 下行速度 (字节/秒)
    std::string localIp;             // 本地 IP
    std::string publicIp;            // 公网 IP (可选)

    // 系统
    double temperature = 0.0;        // 温度 (摄氏度)
    std::string os;                  // 操作系统
    std::string arch;                // 架构 (x64/arm)

    // 时间戳
    uint64_t timestamp = 0;          // 采集时间

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["cpu"] = {
            {"usage", cpuUsage},
            {"cores", cpuCores},
            {"model", cpuModel}
        };
        j["memory"] = {
            {"total", totalMemory},
            {"available", availableMemory},
            {"usage", memoryUsage}
        };
        j["disk"] = {
            {"total", totalDisk},
            {"available", availableDisk},
            {"usage", diskUsage}
        };
        j["network"] = {
            {"up", networkUp},
            {"down", networkDown},
            {"local_ip", localIp},
            {"public_ip", publicIp}
        };
        j["system"] = {
            {"temperature", temperature},
            {"os", os},
            {"arch", arch}
        };
        j["timestamp"] = timestamp;
        return j;
    }

    static ResourceStats fromJson(const nlohmann::json& j) {
        ResourceStats stats;
        if (j.contains("cpu")) {
            auto& cpu = j["cpu"];
            if (cpu.contains("usage")) stats.cpuUsage = cpu["usage"];
            if (cpu.contains("cores")) stats.cpuCores = cpu["cores"];
            if (cpu.contains("model")) stats.cpuModel = cpu["model"];
        }
        if (j.contains("memory")) {
            auto& mem = j["memory"];
            if (mem.contains("total")) stats.totalMemory = mem["total"];
            if (mem.contains("available")) stats.availableMemory = mem["available"];
            if (mem.contains("usage")) stats.memoryUsage = mem["usage"];
        }
        if (j.contains("disk")) {
            auto& disk = j["disk"];
            if (disk.contains("total")) stats.totalDisk = disk["total"];
            if (disk.contains("available")) stats.availableDisk = disk["available"];
            if (disk.contains("usage")) stats.diskUsage = disk["usage"];
        }
        if (j.contains("network")) {
            auto& net = j["network"];
            if (net.contains("up")) stats.networkUp = net["up"];
            if (net.contains("down")) stats.networkDown = net["down"];
            if (net.contains("local_ip")) stats.localIp = net["local_ip"];
            if (net.contains("public_ip")) stats.publicIp = net["public_ip"];
        }
        if (j.contains("system")) {
            auto& sys = j["system"];
            if (sys.contains("temperature")) stats.temperature = sys["temperature"];
            if (sys.contains("os")) stats.os = sys["os"];
            if (sys.contains("arch")) stats.arch = sys["arch"];
        }
        if (j.contains("timestamp")) stats.timestamp = j["timestamp"];
        return stats;
    }
};

// ========== Agent 信息 ==========
struct AgentInfo {
    std::string agentId;              // 客户端 ID
    std::string hostname;             // 主机名
    std::string ip;                   // IP 地址
    AgentStatus status = AgentStatus::Online;
    nlohmann::json currentTask;       // 当前任务（业务层）
    std::chrono::system_clock::time_point lastSeen;

    // 资源状态（注册中心层）
    ResourceStats resources;          // 系统资源状态

    // 序列化
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["agent_id"] = agentId;
        j["hostname"] = hostname;
        j["ip"] = ip;
        j["status"] = agentStatusToString(status);
        j["current_task"] = currentTask;
        j["resources"] = resources.toJson();
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
        if (j.contains("resources")) info.resources = ResourceStats::fromJson(j["resources"]);
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
