#pragma once

#include "wingman/runtime/config.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <functional>
#include <string>
#include <vector>

namespace wingman::runtime {

// 前向声明
namespace wingman::transport {
    class TcpClient;
}

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

// ========== 主动模式 ==========

class ActiveMode {
    class Impl;

public:
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
    const ActiveModeConfig& getConfig() const;

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

    // P-Impl
    std::unique_ptr<Impl> impl_;

    // 运行状态
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    ConnectionCallback eventCallback_;
};

} // namespace wingman::runtime
