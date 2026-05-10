#pragma once

#include "wingman/runtime/config.hpp"
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace wingman::runtime {

// 前向声明
namespace wingman::transport {
    class TcpServer;
}

// ========== 会话信息 ==========

struct SessionInfo {
    std::string id;
    std::string remoteAddress;
    uint16_t remotePort;
    std::chrono::system_clock::time_point connectTime;
};

// ========== 会话事件 ==========

enum class SessionEventType {
    Connected,
    Disconnected,
    Error
};

struct SessionEvent {
    std::string sessionId;
    SessionEventType type;
    std::string message;
};

using SessionCallback = std::function<void(const SessionEvent&)>;

// ========== 消息处理 ==========

using MessageHandler = std::function<std::vector<uint8_t>(const std::string& sessionId, const std::vector<uint8_t>& data)>;

// ========== 被动模式 ==========

class PassiveMode {
    class Impl;

public:
public:
    PassiveMode(const PassiveModeConfig& config);
    ~PassiveMode();

    // 启动/停止
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // 会话管理
    size_t getSessionCount() const;
    std::vector<std::string> getSessionIds() const;
    SessionInfo getSession(const std::string& id);

    // 广播消息
    bool broadcast(const std::vector<uint8_t>& data);
    bool send(const std::string& sessionId, const std::vector<uint8_t>& data);

    // 事件回调
    void setSessionCallback(SessionCallback callback) {
        sessionCallback_ = std::move(callback);
    }

    // 消息处理回调
    void setMessageHandler(MessageHandler handler) {
        messageHandler_ = std::move(handler);
    }

    // 获取配置
    const PassiveModeConfig& getConfig() const;

private:
    // 新会话处理
    void onNewSession(const std::string& sessionId);
    void onSessionEvent(const std::string& sessionId, SessionEventType type, const std::string& message);

    // 消息处理
    void onMessage(const std::string& sessionId, const std::vector<uint8_t>& data);

    // P-Impl
    std::unique_ptr<Impl> impl_;

    std::atomic<bool> running_{false};

    SessionCallback sessionCallback_;
    MessageHandler messageHandler_;
};

} // namespace wingman::runtime
