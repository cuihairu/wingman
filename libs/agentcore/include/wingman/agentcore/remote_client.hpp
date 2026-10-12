#pragma once

#include "wingman/agentcore/remote_client_config.hpp"

// 自 apps/agent 下沉（2026-09-22）：桌面 runtime 与 Android agent 同源链接。
// 命名空间保持 wingman::runtime（与文档/调用方一致）。
#include "wingman/transport/transport.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <functional>
#include <mutex>
#include <queue>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

namespace wingman::runtime {

// ========== Command Callback ==========
// 用于处理 server 下发的命令（如 system.shutdown, run_script）

using CommandData = std::map<std::string, std::string>;

/**
 * @brief Command execution result
 * Returns actual success/failure status to the server
 */
struct CommandResult {
    bool success = false;
    std::string message;  // Error message if success=false, or additional info
    std::string data;     // Optional JSON object/array payload

    static CommandResult ok(const std::string& msg = "") {
        return {true, msg, ""};
    }

    static CommandResult okData(const std::string& jsonData, const std::string& msg = "") {
        return {true, msg, jsonData};
    }

    static CommandResult error(const std::string& msg) {
        return {false, msg, ""};
    }
};

using CommandCallback = std::function<CommandResult(const std::string& command, const CommandData& data)>;

// ========== Notify 下行处理器 ==========
// server 推送的 Notify 帧中，除 RemoteClient 内建处理的 agent.register_ack /
// proxy.*（与 auth.challenge）之外（inbox.message、team.error、inbox.*_ack 及
// 未来新增类型）都经此交给宿主装配层分派。handler 运行在消息线程上，
// 不得重入 RemoteClient 的加锁方法。
using NotifyHandler = std::function<void(const std::string& type, const nlohmann::json& body)>;

// ========== Connection State ==========

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

// ========== 连接事件 ==========

struct ConnectionEvent {
    ConnectionState state;
    std::string message;
};

using ConnectionCallback = std::function<void(const ConnectionEvent&)>;

// ========== 主动模式 ==========

class RemoteClient {
    class Impl;

public:
    RemoteClient(const RemoteClientConfig& config);
    ~RemoteClient();

    // 启动/停止
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    bool isConnected() const { return connected_.load(); }

    // 事件回调
    void setEventCallback(ConnectionCallback callback) {
        eventCallback_ = std::move(callback);
    }

    // 命令回调（用于处理 server 下发的命令）
    void setCommandCallback(CommandCallback callback) {
        commandCallback_ = std::move(callback);
    }

    // 获取配置
    const RemoteClientConfig& getConfig() const;

    // 设置注册身份（须在 start() 前调用）。留空的字段保持默认行为：
    // agentId 自动生成 agent_<hostname>_<rand>，hostname 取系统主机名。
    // Android 端经 JNI 由 Kotlin 侧下发设备标识（docs/android-agent-design.md §3.3）。
    void setIdentity(const std::string& agentId, const std::string& hostname);

    // 设置 agent.register 的附加字段（须在 start() 前调用），逐键合并进注册
    // 消息顶层。Android 端传 {"platform": "android", "capabilities": {...}}；
    // 桌面端不设置，server 侧归一为 desktop。
    void setRegisterMetadata(const nlohmann::json& fields);

    // 设置注册鉴权 token（须在 start() 前调用）：写入 register payload 顶层
    // "token" 字段；留空则不携带该字段（server 鉴权关闭时无需设置）。
    // 见 docs/agent-token-auth-design.md §2.1/§4.1。
    void setAuthToken(const std::string& token);

    // 当前连接状态名（connected/connecting/reconnecting/disconnected/error）。
    // 本地 RPC system.getStatus 经此向 GUI 暴露远程链路状态，修复事件驱动陈旧问题。
    std::string connectionStateName() const {
        return stateName(static_cast<ConnectionState>(state_.load(std::memory_order_relaxed)));
    }

    // stateName 将 ConnectionState 映射为远程事件使用的字符串名。
    static std::string stateName(ConnectionState state);

    // 事件转发：将运行时事件（trigger_fired / script_state 等）以 agent.event Notify 推送到 Go server。
    // 对应 server 侧 listener.go handleNotify -> case "agent.event" -> handleEvent（广播到 Dashboard WS）。
    void sendAgentEvent(const std::string& event, const nlohmann::json& data);

    // 通用 Notify 发送（server 侧 sendNotify 的对称面）：type + 逐键合并 fields
    // 构成帧体。queueable=true：断线时入 outbox，重连后 flush。
    // 团队协同桥（apps/agent）经此转发脚本 wingman.team.* 的 team.outbound 事件
    // （team.join / team.leave / team.vote_* / team.status_report / team.broadcast）。
    void sendNotify(const std::string& type, const nlohmann::json& fields);

    // Notify 下行分派回调（内建类型之外的帧都经此交给宿主，见 NotifyHandler 注释）
    void setNotifyHandler(NotifyHandler callback) {
        notifyHandler_ = std::move(callback);
    }

private:
    // 连接管理
    void connect();
    void disconnect();
    void reconnectLoop();           // 持久重连循环（指数退避）
    int computeBackoff(int attempt) const;          // 计算退避毫秒数（含 0~999ms 抖动，已封顶）
    bool sleepInterruptible(int milliseconds);      // 可被停止信号打断的睡眠（100ms 轮询粒度）
    void startReconnectLoop();              // 启动持久重连循环

    // 出站消息投递（带断线缓冲）
    bool deliverMessage(const transport::MessagePtr& msg, bool queueable);
    void flushOutbox();                     // 重连成功后冲刷缓冲队列

    // 心跳
    void startHeartbeat();
    void sendHeartbeat();

    // 链路质量统计（供 agent.heartbeat 携带上报）
    void markConnected();                    // 连接建立：刷新 connectedAt，非首次则累计重连次数
    void recordDisconnect(const std::string& reason);  // 记录最近一次断线原因
    nlohmann::json linkStats() const;        // 组装 link 统计 JSON（线程安全快照）

    // 注册
    void sendRegister();
    void handleRegisterAck(const std::string& data);

    // 消息处理
    void onMessage(const transport::MessagePtr& msg);  // 接收完整消息（含 header）
    void handleNotifyMessage(const transport::MessagePtr& msg);  // 处理 Notify 消息
    void handleRequestMessage(const transport::MessagePtr& msg);  // 处理 Request 消息并回复 Response
    void handleAuthChallenge(std::uint32_t sequence, const std::string& nonce);  // A3-P2 §6.1：auth.challenge 应答（HMAC-SHA256）
    void onEvent(ConnectionState state, const std::string& message);

    // P-Impl
    std::unique_ptr<Impl> impl_;

    // 运行状态
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    // 当前 ConnectionState（int 存储，供本地状态查询）
    std::atomic<int> state_{static_cast<int>(ConnectionState::Disconnected)};

    ConnectionCallback eventCallback_;
    CommandCallback commandCallback_;
    NotifyHandler notifyHandler_;
};

} // namespace wingman::runtime
