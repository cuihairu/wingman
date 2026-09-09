#pragma once

#include "wingman/transport/transport.hpp"
#include "wingman/transport/session/session.hpp"
#include "wingman/transport/session/tcp_session.hpp"
#include "wingman/transport/channel/channel.hpp"
#include <asio.hpp>
#include <atomic>
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
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host), static_cast<asio::ip::port_type>(port));
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

            session_->setEventCallback([this](SessionEvent event, const std::string& /*info*/) {
                // 服务端断开表现为 async_read EOF -> Session 发出 Error 事件，
                // 同样需要清除连接标志，否则 isConnected()/getSessionCount() 状态陈旧
                if (event == SessionEvent::Disconnected || event == SessionEvent::Error) {
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
        } catch (const std::exception&) {
            return false;
        }
    }

    // 断开连接
    void disconnect() override {
        connected_ = false;

        if (session_) {
            // 取消挂起的异步操作（close 已做并发串行化）。
            // 不先 stop()：让 IO 线程把被取消的回调排干后因无工作自然退出，
            // 否则被中止的处理器会残留在已 restart 的 io_context 中，
            // 在下一次 connect() 的 run() 里执行并污染新连接的状态。
            session_->close();
        }

        if (ioThread_.joinable()) {
            ioThread_.join();
        }

        // join 之后 IO 线程已退出，此时 reset 无并发读者
        session_.reset();

        // 重置 IO 上下文，为下一次 connect 做准备
        ioContext_.restart();
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

    Session* getSession() override {
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
    std::atomic<bool> connected_;

    uint32_t nextSequence_ = 1;
    std::mutex pendingMutex_;
    std::map<uint32_t, std::shared_ptr<std::promise<MessagePtr>>> pendingRequests_;
};

} // namespace wingman::transport
