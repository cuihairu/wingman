#include "wingman/transport/simple_protocol.hpp"

#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <errno.h>
#endif

namespace wingman::transport {

// ========== SimpleMessage ==========

std::unique_ptr<SimpleMessage> SimpleMessage::create(const std::vector<uint8_t>& payload) {
    if (payload.size() > MAX_MESSAGE_SIZE) {
        return nullptr;
    }
    return std::unique_ptr<SimpleMessage>(new SimpleMessage(payload));
}

std::unique_ptr<SimpleMessage> SimpleMessage::create(const std::string& payload) {
    std::vector<uint8_t> data(payload.begin(), payload.end());
    return create(data);
}

std::vector<uint8_t> SimpleMessage::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(SimpleMessage::LENGTH_SIZE + payload_.size());

    // 写入长度（网络字节序）
    uint32_t length = Protocol::hostToNetwork32(static_cast<uint32_t>(payload_.size()));
    const uint8_t* lengthPtr = reinterpret_cast<const uint8_t*>(&length);
    result.insert(result.end(), lengthPtr, lengthPtr + SimpleMessage::LENGTH_SIZE);

    // 写入数据
    result.insert(result.end(), payload_.begin(), payload_.end());

    return result;
}

SimpleMessage::SimpleMessage(const std::vector<uint8_t>& payload)
    : payload_(payload) {}

// ========== MessageReceiver ==========

std::vector<std::unique_ptr<SimpleMessage>> MessageReceiver::receive(
    const uint8_t* data, size_t size) {

    // 将新数据追加到缓冲区
    buffer_.insert(buffer_.end(), data, data + size);

    std::vector<std::unique_ptr<SimpleMessage>> messages;

    // 尝试解析消息
    while (auto msg = tryParseMessage()) {
        messages.push_back(std::move(msg));
    }

    return messages;
}

std::vector<std::unique_ptr<SimpleMessage>> MessageReceiver::receive(const std::string& data) {
    return receive(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void MessageReceiver::clear() {
    buffer_.clear();
}

std::unique_ptr<SimpleMessage> MessageReceiver::tryParseMessage() {
    // 检查是否有足够的数据读取长度
    if (buffer_.size() < SimpleMessage::LENGTH_SIZE) {
        return nullptr;
    }

    // 读取长度
    uint32_t length;
    std::memcpy(&length, buffer_.data(), SimpleMessage::LENGTH_SIZE);
    length = Protocol::networkToHost32(length);

    // 检查长度是否合法
    if (length > SimpleMessage::MAX_MESSAGE_SIZE) {
        clear();  // 清除非法数据
        return nullptr;
    }

    // 检查是否有完整的数据
    if (buffer_.size() < SimpleMessage::LENGTH_SIZE + length) {
        return nullptr;  // 数据不完整，等待更多数据
    }

    // 提取数据
    std::vector<uint8_t> payload(
        buffer_.begin() + SimpleMessage::LENGTH_SIZE,
        buffer_.begin() + SimpleMessage::LENGTH_SIZE + length
    );

    // 从缓冲区移除已处理的数据
    buffer_.erase(
        buffer_.begin(),
        buffer_.begin() + SimpleMessage::LENGTH_SIZE + length
    );

    return SimpleMessage::create(payload);
}

// ========== Protocol ==========

bool Protocol::sendMessage(SocketType socket, const SimpleMessage& message, std::error_code& ec) {
    auto data = message.serialize();

    const uint8_t* ptr = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
#ifdef _WIN32
        int sent = ::send(socket, reinterpret_cast<const char*>(ptr),
                         static_cast<int>(remaining), 0);
#else
        ssize_t sent = ::send(socket, ptr, remaining, 0);
#endif

        if (sent <= 0) {
#ifdef _WIN32
            if (sent == SOCKET_ERROR_VALUE) {
                ec.assign(WSAGetLastError(), std::system_category());
            }
#else
            if (sent < 0) {
                ec.assign(errno, std::system_category());
            }
#endif
            return false;
        }

        ptr += sent;
        remaining -= sent;
    }

    ec.clear();
    return true;
}

std::unique_ptr<SimpleMessage> Protocol::readMessage(SocketType socket, std::error_code& ec) {
    // 先读取长度
    uint8_t lengthBytes[SimpleMessage::LENGTH_SIZE];
    size_t bytesRead = 0;

    while (bytesRead < SimpleMessage::LENGTH_SIZE) {
#ifdef _WIN32
        int n = ::recv(socket, reinterpret_cast<char*>(lengthBytes) + bytesRead,
                       static_cast<int>(SimpleMessage::LENGTH_SIZE - bytesRead), 0);
        if (n == SOCKET_ERROR_VALUE) {
            ec.assign(WSAGetLastError(), std::system_category());
            return nullptr;
        }
        if (n == 0) {
            // 连接关闭
            ec.assign(0, std::system_category());
            return nullptr;
        }
#else
        ssize_t n = ::recv(socket, lengthBytes + bytesRead, SimpleMessage::LENGTH_SIZE - bytesRead, 0);
        if (n < 0) {
            ec.assign(errno, std::system_category());
            return nullptr;
        }
        if (n == 0) {
            // 连接关闭
            ec.assign(0, std::system_category());
            return nullptr;
        }
#endif

        bytesRead += n;
    }

    uint32_t length;
    std::memcpy(&length, lengthBytes, SimpleMessage::LENGTH_SIZE);
    length = Protocol::networkToHost32(length);

    if (length > SimpleMessage::MAX_MESSAGE_SIZE) {
        // 数据过大，视为错误
        ec.assign(EMSGSIZE, std::system_category());
        return nullptr;
    }

    // 读取数据
    std::vector<uint8_t> payload(length);
    bytesRead = 0;

    while (bytesRead < length) {
#ifdef _WIN32
        int n = ::recv(socket, reinterpret_cast<char*>(payload.data()) + bytesRead,
                       static_cast<int>(length - bytesRead), 0);
        if (n == SOCKET_ERROR_VALUE) {
            ec.assign(WSAGetLastError(), std::system_category());
            return nullptr;
        }
        if (n == 0) {
            // 连接关闭
            ec.assign(0, std::system_category());
            return nullptr;
        }
#else
        ssize_t n = ::recv(socket, payload.data() + bytesRead, length - bytesRead, 0);
        if (n < 0) {
            ec.assign(errno, std::system_category());
            return nullptr;
        }
        if (n == 0) {
            // 连接关闭
            ec.assign(0, std::system_category());
            return nullptr;
        }
#endif

        bytesRead += n;
    }

    ec.clear();
    return SimpleMessage::create(payload);
}

} // namespace wingman::transport
