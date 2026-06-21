#pragma once

#include "wingman/runtime/config.hpp"
#include "wingman/transport/transport.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <functional>
#include <mutex>
#include <queue>
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

    // 注册
    void sendRegister();
    void handleRegisterAck(const std::string& data);

    // 消息处理
    void onMessage(const transport::MessagePtr& msg);  // 接收完整消息（含 header）
    void handleNotifyMessage(const transport::MessagePtr& msg);  // 处理 Notify 消息
    void handleRequestMessage(const transport::MessagePtr& msg);  // 处理 Request 消息并回复 Response
    void onEvent(ConnectionState state, const std::string& message);

    // P-Impl
    std::unique_ptr<Impl> impl_;

    // 运行状态
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    ConnectionCallback eventCallback_;
    CommandCallback commandCallback_;
};

} // namespace wingman::runtime
