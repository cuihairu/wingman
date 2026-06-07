#pragma once

// 在包含 Windows 头文件之前定义保护宏，避免宏冲突
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#endif

#include <string>
#include <memory>
#include <functional>
#include <deque>
#include <mutex>
#include <asio.hpp>

namespace wingman::transport {

// ========== 会话 ID ==========

using SessionId = uint64_t;

// ========== 会话事件 ==========

enum class SessionEvent {
    Connected,      // 连接建立
    Disconnected,   // 连接断开
    Error,          // 发生错误
    Timeout         // 超时
};

// ========== 消息定义 ==========

enum class MessageType : uint8_t {
    Request  = 1,
    Response = 2,
    Notify   = 3,
    Error    = 4
};

struct MessageHeader {
    uint32_t length;      // 消息体长度
    uint32_t sequence;    // 序列号
    MessageType type;     // 消息类型
    uint32_t reserved;    // 保留字段
};

class Message {
public:
    static std::shared_ptr<Message> create() {
        return std::make_shared<Message>();
    }

    static std::shared_ptr<Message> create(MessageType type, const std::string& body) {
        auto msg = create();
        msg->header.type = type;
        msg->body = body;
        return msg;
    }

    MessageHeader header{};
    std::string body;

    // 序列化
    std::vector<uint8_t> serialize() const {
        MessageHeader h = header;
        h.length = static_cast<uint32_t>(body.size());
        std::vector<uint8_t> buffer(sizeof(MessageHeader) + body.size());
        memcpy(buffer.data(), &h, sizeof(MessageHeader));
        memcpy(buffer.data() + sizeof(MessageHeader), body.data(), body.size());
        return buffer;
    }

    // 反序列化
    static std::shared_ptr<Message> deserialize(const std::vector<uint8_t>& buffer) {
        if (buffer.size() < sizeof(MessageHeader)) {
            return nullptr;
        }

        auto msg = create();
        memcpy(&msg->header, buffer.data(), sizeof(MessageHeader));

        if (buffer.size() >= sizeof(MessageHeader) + msg->header.length) {
            msg->body.assign(buffer.begin() + sizeof(MessageHeader),
                           buffer.begin() + sizeof(MessageHeader) + msg->header.length);
        }

        return msg;
    }
};

using MessagePtr = std::shared_ptr<Message>;

// ========== 回调定义 ==========

using MessageCallback = std::function<void(const MessagePtr&)>;
using EventCallback = std::function<void(SessionEvent, const std::string&)>;

// ========== 会话基类 ==========

class Session {
public:
    Session(SessionId id, asio::ip::tcp::socket socket)
        : id_(id), socket_(std::move(socket)) {}

    virtual ~Session() {
        close();
    }

    // 禁止拷贝
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // 基本信息
    SessionId getId() const { return id_; }
    std::string getRemoteAddress() const {
        try {
            return socket_.remote_endpoint().address().to_string();
        } catch (...) {
            return "";
        }
    }
    uint16_t getRemotePort() const {
        try {
            return socket_.remote_endpoint().port();
        } catch (...) {
            return 0;
        }
    }

    // 连接状态
    bool isConnected() const {
        return socket_.is_open();
    }

    // 关闭连接
    void close() {
        if (socket_.is_open()) {
            asio::error_code ec;
            socket_.close(ec);
        }
    }

    // 发送消息
    bool send(const MessagePtr& message) {
        if (!isConnected()) {
            return false;
        }

        // 更新头部长度
        message->header.length = static_cast<uint32_t>(message->body.size());

        // 序列化
        auto data = std::make_shared<std::vector<uint8_t>>(message->serialize());

        std::lock_guard lock(sendMutex_);
        if (sendQueue_.size() >= kMaxSendQueueSize) {
            return false;  // 队列已满，拒绝新消息
        }
        const bool writeInProgress = !sendQueue_.empty();
        sendQueue_.push_back(std::move(data));
        if (!writeInProgress) {
            writeNextLocked();
        }

        return true;
    }

    // 接收消息（启动接收循环）
    void startReceive() {
        receiveHeader();
    }

    // 回调设置
    void setMessageCallback(MessageCallback callback) {
        messageCallback_ = std::move(callback);
    }

    void setEventCallback(EventCallback callback) {
        eventCallback_ = std::move(callback);
    }

protected:
    void writeNextLocked() {
        auto data = sendQueue_.front();
        asio::async_write(socket_, asio::buffer(*data),
            [this, data](asio::error_code ec, size_t /*bytes_sent*/) {
                bool writeMore = false;
                {
                    std::lock_guard lock(sendMutex_);
                    if (!sendQueue_.empty() && sendQueue_.front() == data) {
                        sendQueue_.pop_front();
                    }
                    if (ec) {
                        auto dropped = sendQueue_.size();
                        sendQueue_.clear();
                        if (dropped > 0 && eventCallback_) {
                            // 释放锁后报告错误（避免回调中再次加锁）
                            // handleError 在锁外调用
                        }
                    }
                    writeMore = !ec && !sendQueue_.empty();
                    if (writeMore) {
                        writeNextLocked();
                    }
                }
                if (ec) {
                    handleError(ec);
                }
            });
    }

    void receiveHeader() {
        asio::async_read(socket_, asio::buffer(&receiveBuffer_.header, sizeof(MessageHeader)),
            [this](asio::error_code ec, size_t /*bytes_read*/) {
                if (ec) {
                    handleError(ec);
                    return;
                }
                receiveBody();
            });
    }

    void receiveBody() {
        // 准备接收缓冲区
        receiveBuffer_.body.resize(receiveBuffer_.header.length);

        asio::async_read(socket_, asio::buffer(receiveBuffer_.body),
            [this](asio::error_code ec, size_t /*bytes_read*/) {
                if (ec) {
                    handleError(ec);
                    return;
                }

                // 构造消息
                auto message = Message::create();
                message->header = receiveBuffer_.header;
                message->body = std::move(receiveBuffer_.body);

                // 触发回调
                if (messageCallback_) {
                    messageCallback_(message);
                }

                // 继续接收
                receiveHeader();
            });
    }

    void handleError(const asio::error_code& ec) {
        close();
        if (eventCallback_) {
            eventCallback_(SessionEvent::Error, ec.message());
        }
    }

    SessionId id_;
    asio::ip::tcp::socket socket_;
    Message receiveBuffer_;
    MessageCallback messageCallback_;
    EventCallback eventCallback_;
    std::mutex sendMutex_;
    std::deque<std::shared_ptr<std::vector<uint8_t>>> sendQueue_;
    static constexpr size_t kMaxSendQueueSize = 1024;
};

using SessionPtr = std::shared_ptr<Session>;

} // namespace wingman::transport
