#pragma once

#include "wingman/transport/transport.hpp"
#include "wingman/transport/session/session.hpp"
#include "wingman/transport/channel/channel.hpp"
#include <asio.hpp>
#include <map>
#include <mutex>

namespace wingman::transport {

// ========== 传输类型 ==========

enum class TransportType {
    TCP,
    WebSocket
};

// ========== Transport Server 实现 ==========

class TcpServer : public TransportServer {
public:
    TcpServer()
        : ioContext_(),
          acceptor_(ioContext_),
          nextSessionId_(1) {}

    ~TcpServer() override {
        stop();
    }

    // 启动
    bool start() override {
        if (running_) return true;
        running_ = true;

        // 启动 IO 线程
        ioThread_ = std::thread([this]() {
            ioContext_.run();
        });

        return true;
    }

    // 停止
    void stop() override {
        if (!running_) return;
        running_ = false;

        // 关闭所有会话
        closeAllSessions();

        // 停止接受连接
        asio::error_code ec;
        acceptor_.close(ec);

        // 停止 IO 上下文
        ioContext_.stop();

        // 等待 IO 线程
        if (ioThread_.joinable()) {
            ioThread_.join();
        }
    }

    // 监听
    bool listen(const std::string& host, int port) override {
        try {
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host), port);
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();

            // 开始接受连接
            acceptNext();

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    // 会话管理
    size_t getSessionCount() const override {
        std::lock_guard lock(sessionsMutex_);
        return sessions_.size();
    }

    std::vector<SessionId> getSessionIds() const override {
        std::lock_guard lock(sessionsMutex_);
        std::vector<SessionId> ids;
        for (const auto& [id, _] : sessions_) {
            ids.push_back(id);
        }
        return ids;
    }

    Session* getSession(SessionId id) override {
        std::lock_guard lock(sessionsMutex_);
        auto it = sessions_.find(id);
        return it != sessions_.end() ? it->second.get() : nullptr;
    }

    void closeSession(SessionId id) override {
        std::lock_guard lock(sessionsMutex_);
        auto it = sessions_.find(id);
        if (it != sessions_.end()) {
            it->second->close();
            channelManager_.closeBySession(id);
            sessions_.erase(it);
        }
    }

    void closeAllSessions() {
        std::lock_guard lock(sessionsMutex_);
        for (auto& [id, session] : sessions_) {
            session->close();
        }
        sessions_.clear();
        channelManager_.closeAll();
    }

    // 发送消息
    bool send(SessionId sessionId, const MessagePtr& message) override {
        std::lock_guard lock(sessionsMutex_);
        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) {
            return false;
        }
        return it->second->send(message);
    }

    // 广播消息
    void broadcast(const MessagePtr& message) override {
        std::lock_guard lock(sessionsMutex_);
        for (auto& [id, session] : sessions_) {
            session->send(message);
        }
    }

    bool isRunning() const override {
        return running_;
    }

private:
    void acceptNext() {
        acceptor_.async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
            if (!running_) return;

            if (!ec) {
                // 创建新会话
                auto sessionId = nextSessionId_++;
                auto session = TcpSession::create(sessionId, std::move(socket));

                // 设置回调
                session->setMessageCallback([this, session](const MessagePtr& msg) {
                    handleMessage(session.get(), msg);
                });

                session->setEventCallback([this, session](SessionEvent event, const std::string& info) {
                    handleEvent(session.get(), event);
                    if (event == SessionEvent::Disconnected) {
                        closeSession(session->getId());
                    }
                });

                // 启动接收
                session->startReceive();

                // 添加到会话列表
                {
                    std::lock_guard lock(sessionsMutex_);
                    sessions_[sessionId] = session;
                }

                // 触发连接事件
                handleEvent(session.get(), SessionEvent::Connected);
            }

            // 继续接受
            acceptNext();
        });
    }

    asio::io_context ioContext_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread ioThread_;
    bool running_ = false;
    SessionId nextSessionId_;

    mutable std::mutex sessionsMutex_;
    std::map<SessionId, SessionPtr> sessions_;

    ChannelManager channelManager_;
};

// ========== 工厂函数 ==========

inline std::unique_ptr<TransportServer> TransportServer::create(TransportType type) {
    switch (type) {
        case TransportType::TCP:
            return std::make_unique<TcpServer>();
        default:
            return nullptr;
    }
}

} // namespace wingman::transport
