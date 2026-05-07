#pragma once

#include "wingman/agent/config.hpp"
#include <atomic>
#include <memory>
#include <functional>

namespace wingman::agent {

// ========== 连接状态 ==========

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

// ========== 消息处理 ==========

namespace transport {
    class TcpClient;
}

// ========== 主动模式 ==========

class ActiveMode {
public:
    ActiveMode(const ActiveModeConfig& config);
    ~ActiveMode();

    // 启动/停止
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    bool isConnected() const { return connected_.load(); }

    // 事件回调
    void setEventCallback(ConnectionCallback callback) {
        eventCallback_ = std::move(callback);
    }

    // 获取配置
    const ActiveModeConfig& getConfig() const { return config_; }

private:
    // 连接管理
    void connect();
    void disconnect();
    void reconnect();

    // 心跳
    void startHeartbeat();
    void sendHeartbeat();

    // 消息处理
    void onMessage(const std::vector<uint8_t>& data);
    void onEvent(ConnectionState state, const std::string& message);

    ActiveModeConfig config_;
    std::unique_ptr<transport::TcpClient> client_;

    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    ConnectionCallback eventCallback_;
};

} // namespace wingman::agent
