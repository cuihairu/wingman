#pragma once

#include "wingman/agent/config.hpp"
#include <atomic>
#include <memory>
#include <functional>
#include <map>

namespace wingman::agent {

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

namespace transport {
    class TcpServer;
}

// ========== 消息处理 ==========

using MessageHandler = std::function<std::vector<uint8_t>(const std::string& sessionId, const std::vector<uint8_t>& data)>;

// ========== 被动模式 ==========

class PassiveMode {
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
    const PassiveModeConfig& getConfig() const { return config_; }

private:
    // 新会话处理
    void onNewSession(const std::string& sessionId);
    void onSessionEvent(const std::string& sessionId, SessionEventType type, const std::string& message);

    // 消息处理
    void onMessage(const std::string& sessionId, const std::vector<uint8_t>& data);

    PassiveModeConfig config_;
    std::unique_ptr<transport::TcpServer> server_;

    std::atomic<bool> running_{false};

    // 会话管理
    std::map<std::string, SessionInfo> sessions_;
    mutable std::mutex sessionsMutex_;

    SessionCallback sessionCallback_;
    MessageHandler messageHandler_;
};

} // namespace wingman::agent
