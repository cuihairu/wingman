#pragma once

#include "wingman/runtime/config.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <map>

namespace wingman::runtime {

// ========== Command Callback ==========
// 用于处理 server 下发的命令（如 system.shutdown, run_script）

using CommandData = std::map<std::string, std::string>;
using CommandCallback = std::function<void(const std::string& command, const CommandData& data)>;

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
    void reconnect();

    // 心跳
    void startHeartbeat();
    void sendHeartbeat();

    // 注册
    void sendRegister();
    void handleRegisterAck(const std::string& data);

    // 消息处理
    void onMessage(const std::vector<uint8_t>& data);
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
