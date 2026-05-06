#pragma once

#include <string>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include "wingman_protocol.pb.h"

namespace wingman::server {

// ========== 类型转换函数（protobuf <-> 本地枚举） ==========

inline ErrorCode protobufToErrorCode(wingman::protocol::ErrorCode code) {
    return static_cast<ErrorCode>(code);
}

inline wingman::protocol::ErrorCode errorCodeToProtobuf(ErrorCode code) {
    return static_cast<wingman::protocol::ErrorCode>(code);
}

inline RequestType protobufToRequestType(wingman::protocol::RequestType type) {
    switch (type) {
        case wingman::protocol::RequestType::EXECUTE_SCRIPT: return RequestType::kExecuteScript;
        case wingman::protocol::RequestType::STOP_SCRIPT: return RequestType::kStopScript;
        case wingman::protocol::RequestType::GET_STATUS: return RequestType::kGetStatus;
        case wingman::protocol::RequestType::SCREENSHOT: return RequestType::kScreenshot;
        case wingman::protocol::RequestType::MOUSE_MOVE: return RequestType::kMouseMove;
        case wingman::protocol::RequestType::MOUSE_CLICK: return RequestType::kMouseClick;
        case wingman::protocol::RequestType::KEY_PRESS: return RequestType::kKeyPress;
        case wingman::protocol::RequestType::REGISTER: return RequestType::kRegister;
        case wingman::protocol::RequestType::HEARTBEAT: return RequestType::kHeartbeat;
        case wingman::protocol::RequestType::GET_AGENTS: return RequestType::kGetAgents;
        case wingman::protocol::RequestType::SYNC_TASK: return RequestType::kSyncTask;
        case wingman::protocol::RequestType::SHUTDOWN: return RequestType::kShutdown;
        case wingman::protocol::RequestType::SUBMIT_WORKFLOW: return RequestType::kSubmitWorkflow;
        case wingman::protocol::RequestType::CANCEL_WORKFLOW: return RequestType::kCancelWorkflow;
        case wingman::protocol::RequestType::GET_WORKFLOW: return RequestType::kGetWorkflow;
        case wingman::protocol::RequestType::GET_NEXT_TASK: return RequestType::kGetNextTask;
        case wingman::protocol::RequestType::REPORT_PROGRESS: return RequestType::kReportProgress;
        case wingman::protocol::RequestType::COMPLETE_TASK: return RequestType::kCompleteTask;
        case wingman::protocol::RequestType::FAIL_TASK: return RequestType::kFailTask;
        default: return RequestType::kUnknown;
    }
}

inline wingman::protocol::RequestType requestTypeToProtobuf(RequestType type) {
    switch (type) {
        case RequestType::kExecuteScript: return wingman::protocol::RequestType::EXECUTE_SCRIPT;
        case RequestType::kStopScript: return wingman::protocol::RequestType::STOP_SCRIPT;
        case RequestType::kGetStatus: return wingman::protocol::RequestType::GET_STATUS;
        case RequestType::kScreenshot: return wingman::protocol::RequestType::SCREENSHOT;
        case RequestType::kMouseMove: return wingman::protocol::RequestType::MOUSE_MOVE;
        case RequestType::kMouseClick: return wingman::protocol::RequestType::MOUSE_CLICK;
        case RequestType::kKeyPress: return wingman::protocol::RequestType::KEY_PRESS;
        case RequestType::kRegister: return wingman::protocol::RequestType::REGISTER;
        case RequestType::kHeartbeat: return wingman::protocol::RequestType::HEARTBEAT;
        case RequestType::kGetAgents: return wingman::protocol::RequestType::GET_AGENTS;
        case RequestType::kSyncTask: return wingman::protocol::RequestType::SYNC_TASK;
        case RequestType::kShutdown: return wingman::protocol::RequestType::SHUTDOWN;
        case RequestType::kSubmitWorkflow: return wingman::protocol::RequestType::SUBMIT_WORKFLOW;
        case RequestType::kCancelWorkflow: return wingman::protocol::RequestType::CANCEL_WORKFLOW;
        case RequestType::kGetWorkflow: return wingman::protocol::RequestType::GET_WORKFLOW;
        case RequestType::kGetNextTask: return wingman::protocol::RequestType::GET_NEXT_TASK;
        case RequestType::kReportProgress: return wingman::protocol::RequestType::REPORT_PROGRESS;
        case RequestType::kCompleteTask: return wingman::protocol::RequestType::COMPLETE_TASK;
        case RequestType::kFailTask: return wingman::protocol::RequestType::FAIL_TASK;
        default: return wingman::protocol::RequestType::UNKNOWN;
    }
}

inline AgentStatus protobufToAgentStatus(wingman::protocol::AgentStatus status) {
    return static_cast<AgentStatus>(status);
}

inline wingman::protocol::AgentStatus agentStatusToProtobuf(AgentStatus status) {
    return static_cast<wingman::protocol::AgentStatus>(status);
}

// ========== 错误码定义（保留向后兼容）==========
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

    // 转换到 protobuf
    wingman::protocol::ResourceStats toProtobuf() const {
        wingman::protocol::ResourceStats proto;
        proto.mutable_cpu()->set_usage(cpuUsage);
        proto.mutable_cpu()->set_cores(cpuCores);
        proto.mutable_cpu()->set_model(cpuModel);
        proto.mutable_memory()->set_total(totalMemory);
        proto.mutable_memory()->set_available(availableMemory);
        proto.mutable_memory()->set_usage(memoryUsage);
        proto.mutable_disk()->set_total(totalDisk);
        proto.mutable_disk()->set_available(availableDisk);
        proto.mutable_disk()->set_usage(diskUsage);
        proto.mutable_network()->set_up(networkUp);
        proto.mutable_network()->set_down(networkDown);
        proto.mutable_network()->set_local_ip(localIp);
        proto.mutable_network()->set_public_ip(publicIp);
        proto.mutable_system()->set_temperature(temperature);
        proto.mutable_system()->set_os(os);
        proto.mutable_system()->set_arch(arch);
        proto.set_timestamp(timestamp);
        return proto;
    }

    // 从 protobuf 转换
    static ResourceStats fromProtobuf(const wingman::protocol::ResourceStats& proto) {
        ResourceStats stats;
        if (proto.has_cpu()) {
            stats.cpuUsage = proto.cpu().usage();
            stats.cpuCores = proto.cpu().cores();
            stats.cpuModel = proto.cpu().model();
        }
        if (proto.has_memory()) {
            stats.totalMemory = proto.memory().total();
            stats.availableMemory = proto.memory().available();
            stats.memoryUsage = proto.memory().usage();
        }
        if (proto.has_disk()) {
            stats.totalDisk = proto.disk().total();
            stats.availableDisk = proto.disk().available();
            stats.diskUsage = proto.disk().usage();
        }
        if (proto.has_network()) {
            stats.networkUp = proto.network().up();
            stats.networkDown = proto.network().down();
            stats.localIp = proto.network().local_ip();
            stats.publicIp = proto.network().public_ip();
        }
        if (proto.has_system()) {
            stats.temperature = proto.system().temperature();
            stats.os = proto.system().os();
            stats.arch = proto.system().arch();
        }
        stats.timestamp = proto.timestamp();
        return stats;
    }

    // 保留 JSON 方法（用于复杂嵌套数据）
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["cpu"] = {{"usage", cpuUsage}, {"cores", cpuCores}, {"model", cpuModel}};
        j["memory"] = {{"total", totalMemory}, {"available", availableMemory}, {"usage", memoryUsage}};
        j["disk"] = {{"total", totalDisk}, {"available", availableDisk}, {"usage", diskUsage}};
        j["network"] = {{"up", networkUp}, {"down", networkDown}, {"local_ip", localIp}, {"public_ip", publicIp}};
        j["system"] = {{"temperature", temperature}, {"os", os}, {"arch", arch}};
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

    // 转换到 protobuf
    wingman::protocol::AgentInfo toProtobuf() const {
        wingman::protocol::AgentInfo proto;
        proto.set_agent_id(agentId);
        proto.set_hostname(hostname);
        proto.set_ip(ip);
        proto.set_status(agentStatusToProtobuf(status));
        proto.set_current_task(currentTask.dump());
        *proto.mutable_resources() = resources.toProtobuf();
        proto.set_last_seen(std::chrono::system_clock::to_time_t(lastSeen));
        return proto;
    }

    // 从 protobuf 转换
    static AgentInfo fromProtobuf(const wingman::protocol::AgentInfo& proto) {
        AgentInfo info;
        info.agentId = proto.agent_id();
        info.hostname = proto.hostname();
        info.ip = proto.ip();
        info.status = protobufToAgentStatus(proto.status());
        if (!proto.current_task().empty()) {
            info.currentTask = nlohmann::json::parse(proto.current_task());
        }
        if (proto.has_resources()) {
            info.resources = ResourceStats::fromProtobuf(proto.resources());
        }
        info.lastSeen = std::chrono::system_clock::from_time_t(proto.last_seen());
        return info;
    }

    // 保留 JSON 方法
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
    nlohmann::json data;               // 业务数据（保留用于复杂嵌套）

    // 转换到 protobuf
    wingman::protocol::Request toProtobuf() const {
        wingman::protocol::Request proto;
        proto.set_type(requestTypeToProtobuf(type));
        proto.set_id(id);
        proto.set_timestamp(timestamp);
        if (!agentId.empty()) proto.set_agent_id(agentId);
        if (priority != 0) proto.set_priority(priority);
        proto.set_data(data.dump());
        return proto;
    }

    // 从 protobuf 转换
    static Request fromProtobuf(const wingman::protocol::Request& proto) {
        Request request;
        request.type = protobufToRequestType(proto.type());
        request.id = proto.id();
        request.timestamp = proto.timestamp();
        request.agentId = proto.agent_id();
        request.priority = proto.priority();
        if (!proto.data().empty()) {
            request.data = nlohmann::json::parse(proto.data());
        }
        return request;
    }

    // 保留 JSON 方法用于向后兼容
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

    // 转换到 protobuf
    wingman::protocol::Response toProtobuf() const {
        wingman::protocol::Response proto;
        proto.set_request_id(requestId);
        proto.set_code(errorCodeToProtobuf(code));
        proto.set_timestamp(timestamp);
        if (!message.empty()) proto.set_message(message);
        proto.set_data(data.dump());
        return proto;
    }

    // 从 protobuf 转换
    static Response fromProtobuf(const wingman::protocol::Response& proto) {
        Response response;
        response.requestId = proto.request_id();
        response.code = protobufToErrorCode(proto.code());
        response.timestamp = proto.timestamp();
        response.message = proto.message();
        if (!proto.data().empty()) {
            response.data = nlohmann::json::parse(proto.data());
        }
        return response;
    }

    // 保留 JSON 方法
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
    // 使用 protobuf 编码（信封协议：length\nprotobuf\n）
    static std::string encode(const Response& response);
    static std::string encode(const Request& request);

    // 解码
    static std::optional<Request> decodeRequest(const std::string& buffer);
    static std::optional<Response> decodeResponse(const std::string& buffer);

    // 保留 JSON 解码（向后兼容）
    static std::optional<Request> decode(const std::string& buffer);
    static std::string encodeJson(const Response& response);

    // 解析请求类型
    static RequestType parseRequestType(const std::string& type);
    static std::string requestTypeToString(RequestType type);

    // 获取当前时间戳
    static uint64_t now() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
};

} // namespace wingman::server
