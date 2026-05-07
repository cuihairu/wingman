#pragma once

#include "wingman/transport/transport.hpp"
#include "wingman/transport/session/session.hpp"
#include "wingman/transport/session/tcp_session.hpp"
#include "wingman/transport/channel/channel.hpp"
#include <asio.hpp>
#include <future>

namespace wingman::transport {

// ========== Transport Client 实现 ==========

class TcpClient : public TransportClient {
public:
    TcpClient()
        : ioContext_(),
          socket_(ioContext_),
          session_(nullptr),
          connected_(false) {}

    ~TcpClient() override {
        disconnect();
    }

    // 连接
    bool connect(const std::string& host, int port) override {
        try {
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host), port);
            asio::error_code ec;

            // 同步连接
            socket_.connect(endpoint, ec);
            if (ec) {
                if (eventHandler_) {
                    eventHandler_(nullptr, SessionEvent::Error);
                }
                return false;
            }

            // 创建会话
            session_ = TcpSession::create(0, std::move(socket_));

            // 设置回调
            session_->setMessageCallback([this](const MessagePtr& msg) {
                handleMessage(session_.get(), msg);
            });

            session_->setEventCallback([this](SessionEvent event, const std::string& info) {
                if (event == SessionEvent::Disconnected) {
                    connected_ = false;
                }
                if (eventHandler_) {
                    eventHandler_(session_.get(), event);
                }
            });

            // 启动接收
            session_->startReceive();

            // 启动 IO 线程
            ioThread_ = std::thread([this]() {
                ioContext_.run();
            });

            connected_ = true;

            // 触发连接事件
            if (eventHandler_) {
                eventHandler_(session_.get(), SessionEvent::Connected);
            }

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    // 断开连接
    void disconnect() override {
        if (session_) {
            session_->close();
            session_.reset();
        }

        connected_ = false;

        ioContext_.stop();
        if (ioThread_.joinable()) {
            ioThread_.join();
        }

        // 重置 IO 上下文
        ioContext_.reset();
        socket_ = asio::ip::tcp::socket(ioContext_);
    }

    // 发送消息
    bool send(const MessagePtr& message) override {
        if (!session_ || !connected_) {
            return false;
        }
        return session_->send(message);
    }

    // 请求-响应模式
    std::future<MessagePtr> request(const MessagePtr& request) {
        auto promise = std::make_shared<std::promise<MessagePtr>>();

        if (!session_ || !connected_) {
            promise->set_value(nullptr);
            return promise->get_future();
        }

        // 设置序列号
        request->header.sequence = nextSequence_++;
        request->header.type = MessageType::Request;

        // 保存 promise
        {
            std::lock_guard lock(pendingMutex_);
            pendingRequests_[request->header.sequence] = promise;
        }

        // 发送
        session_->send(request);

        return promise->get_future();
    }

    // 是否已连接
    bool isConnected() const override {
        return connected_ && session_ && session_->isConnected();
    }

    // 启动/停止（客户端不需要显式启动）
    bool start() override { return true; }
    void stop() override { disconnect(); }
    bool isRunning() const override { return isConnected(); }

    // 会话管理（客户端只有一个会话）
    size_t getSessionCount() const override {
        return connected_ ? 1 : 0;
    }

    std::vector<SessionId> getSessionIds() const override {
        if (connected_) {
            return {0};
        }
        return {};
    }

    Session* getSession(SessionId /*id*/) override {
        return session_.get();
    }

    Session* getSession() {
        return session_.get();
    }

    void closeSession(SessionId /*id*/) override {
        disconnect();
    }

    void broadcast(const MessagePtr& message) override {
        send(message);
    }

private:
    void handleMessage(Session* /*session*/, const MessagePtr& message) {
        // 检查是否是响应
        if (message->header.type == MessageType::Response) {
            std::lock_guard lock(pendingMutex_);
            auto it = pendingRequests_.find(message->header.sequence);
            if (it != pendingRequests_.end()) {
                it->second->set_value(message);
                pendingRequests_.erase(it);
                return;
            }
        }

        // 其他消息交给处理器
        Transport::handleMessage(session_ ? session_.get() : nullptr, message);
    }

    asio::io_context ioContext_;
    asio::ip::tcp::socket socket_;
    std::thread ioThread_;
    SessionPtr session_;
    bool connected_;

    uint32_t nextSequence_ = 1;
    std::mutex pendingMutex_;
    std::map<uint32_t, std::shared_ptr<std::promise<MessagePtr>>> pendingRequests_;
};

} // namespace wingman::transport
