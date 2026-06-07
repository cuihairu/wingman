#pragma once

// Guard macros for Windows headers
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

// ========== Session ID ==========

using SessionId = uint64_t;

// ========== Session Events ==========

enum class SessionEvent {
    Connected,
    Disconnected,
    Error,
    Timeout
};

// ========== Message Types ==========

enum class MessageType : uint8_t {
    Request  = 1,
    Response = 2,
    Notify   = 3,
    Error    = 4
};

struct MessageHeader {
    uint32_t length;
    uint32_t sequence;
    MessageType type;
    uint32_t reserved;
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

    std::vector<uint8_t> serialize() const {
        MessageHeader h = header;
        h.length = static_cast<uint32_t>(body.size());
        std::vector<uint8_t> buffer(sizeof(MessageHeader) + body.size());
        memcpy(buffer.data(), &h, sizeof(MessageHeader));
        memcpy(buffer.data() + sizeof(MessageHeader), body.data(), body.size());
        return buffer;
    }

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

// ========== Callbacks ==========

using MessageCallback = std::function<void(const MessagePtr&)>;
using EventCallback = std::function<void(SessionEvent, const std::string&)>;

// ========== Session ==========

class Session : public std::enable_shared_from_this<Session> {
public:
    // Use create() instead of constructing directly — shared_from_this()
    // requires a shared_ptr to exist at construction time.
    static std::shared_ptr<Session> create(SessionId id, asio::ip::tcp::socket socket) {
        return std::shared_ptr<Session>(new Session(id, std::move(socket)));
    }

    virtual ~Session() {
        close();
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Basic info
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

    // Connection state
    bool isConnected() const {
        return socket_.is_open();
    }

    // Close the socket, cancelling all pending async operations.
    void close() {
        if (socket_.is_open()) {
            asio::error_code ec;
            socket_.close(ec);
        }
    }

    // Send a message. Returns false if not connected or queue is full.
    bool send(const MessagePtr& message) {
        if (!isConnected()) {
            return false;
        }

        message->header.length = static_cast<uint32_t>(message->body.size());
        auto data = std::make_shared<std::vector<uint8_t>>(message->serialize());

        std::lock_guard lock(sendMutex_);
        if (sendQueue_.size() >= kMaxSendQueueSize) {
            return false;
        }
        const bool writeInProgress = !sendQueue_.empty();
        sendQueue_.push_back(std::move(data));
        if (!writeInProgress) {
            doWrite();
        }

        return true;
    }

    // Start the receive loop.
    void startReceive() {
        receiveHeader();
    }

    // Callbacks
    void setMessageCallback(MessageCallback callback) {
        messageCallback_ = std::move(callback);
    }

    void setEventCallback(EventCallback callback) {
        eventCallback_ = std::move(callback);
    }

protected:
    // Accessible to subclasses (e.g. TcpSession). Use create() or
    // subclass factories to ensure the object lives in a shared_ptr.
    Session(SessionId id, asio::ip::tcp::socket socket)
        : id_(id), socket_(std::move(socket)) {}

    // Write the front of the queue. Caller must hold sendMutex_.
    // Captures shared_from_this() so the Session lives as long as the
    // async operation is outstanding.
    void doWrite() {
        auto self = shared_from_this();
        auto data = sendQueue_.front();
        asio::async_write(socket_, asio::buffer(*data),
            [this, self, data](asio::error_code ec, size_t /*bytes_sent*/) {
                bool writeMore = false;
                {
                    std::lock_guard lock(sendMutex_);
                    if (!sendQueue_.empty() && sendQueue_.front() == data) {
                        sendQueue_.pop_front();
                    }
                    if (ec) {
                        sendQueue_.clear();
                    }
                    writeMore = !ec && !sendQueue_.empty();
                    if (writeMore) {
                        doWrite();
                    }
                }
                if (ec) {
                    handleError(ec);
                }
            });
    }

    void receiveHeader() {
        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(&receiveBuffer_.header, sizeof(MessageHeader)),
            [this, self](asio::error_code ec, size_t /*bytes_read*/) {
                if (ec) {
                    handleError(ec);
                    return;
                }
                receiveBody();
            });
    }

    void receiveBody() {
        receiveBuffer_.body.resize(receiveBuffer_.header.length);

        auto self = shared_from_this();
        asio::async_read(socket_, asio::buffer(receiveBuffer_.body),
            [this, self](asio::error_code ec, size_t /*bytes_read*/) {
                if (ec) {
                    handleError(ec);
                    return;
                }

                auto message = Message::create();
                message->header = receiveBuffer_.header;
                message->body = std::move(receiveBuffer_.body);

                if (messageCallback_) {
                    messageCallback_(message);
                }

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
