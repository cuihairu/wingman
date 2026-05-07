#pragma once

// ========== wingman-transport ==========
// 分层传输架构
//
// 架构层次：
// 1. Session 层 - 单个连接会话管理
// 2. Channel 层 - 消息通道（多路复用）
// 3. Transport 层 - 传输抽象（TCP/WebSocket）
// 4. Handler 层 - 消息处理

#include "wingman/transport/session/session.hpp"
#include "wingman/transport/session/tcp_session.hpp"
#include "wingman/transport/channel/channel.hpp"
#include "wingman/transport/transport_server.hpp"
#include "wingman/transport/transport_client.hpp"

namespace wingman::transport {

// ========== 消息定义 ==========

using MessageHandler = std::function<void(const MessagePtr&)>;
using EventHandler = std::function<void(Session*, SessionEvent)>;

// ========== 传输层 ==========

class Transport {
public:
    virtual ~Transport() = default;

    // 启动/停止
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // 会话管理
    virtual size_t getSessionCount() const = 0;
    virtual std::vector<SessionId> getSessionIds() const = 0;
    virtual Session* getSession(SessionId id) = 0;
    virtual void closeSession(SessionId id) = 0;

    // 广播消息
    virtual void broadcast(const MessagePtr& message) = 0;

    // 回调设置
    void setMessageHandler(MessageHandler handler) { messageHandler_ = std::move(handler); }
    void setEventHandler(EventHandler handler) { eventHandler_ = std::move(handler); }

protected:
    MessageHandler messageHandler_;
    EventHandler eventHandler_;

    void handleMessage(Session* session, const MessagePtr& message) {
        if (messageHandler_) {
            messageHandler_(message);
        }
    }

    void handleEvent(Session* session, SessionEvent event) {
        if (eventHandler_) {
            eventHandler_(session, event);
        }
    }
};

// ========== 客户端传输 ==========

class TransportClient : public Transport {
public:
    static std::unique_ptr<TransportClient> create(TransportType type);

    // 连接
    virtual bool connect(const std::string& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // 发送消息
    virtual bool send(const MessagePtr& message) = 0;

    // 获取当前会话
    virtual Session* getSession() = 0;
};

// ========== 服务端传输 ==========

class TransportServer : public Transport {
public:
    static std::unique_ptr<TransportServer> create(TransportType type);

    // 监听
    virtual bool listen(const std::string& host, int port) = 0;

    // 发送消息到指定会话
    virtual bool send(SessionId sessionId, const MessagePtr& message) = 0;
};

// ========== 便捷类型 ==========

using TcpServerPtr = std::unique_ptr<TransportServer>;
using TcpClientPtr = std::unique_ptr<TransportClient>;

// 创建函数
inline TcpServerPtr createTcpServer() {
    return TransportServer::create(TransportType::TCP);
}

inline TcpClientPtr createTcpClient() {
    return TransportClient::create(TransportType::TCP);
}

} // namespace wingman::transport
