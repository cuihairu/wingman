#include "wingman/transport/transport.hpp"
#include <cstdint>

namespace wingman::transport {

// ========== 消息处理器 ==========

ByteBuffer MessageHandler::encode(const ByteBuffer& data) {
    ByteBuffer result;

    // 写入长度头（4字节，大端序）
    uint32_t length = static_cast<uint32_t>(data.size());
    result.push_back((length >> 24) & 0xFF);
    result.push_back((length >> 16) & 0xFF);
    result.push_back((length >> 8) & 0xFF);
    result.push_back(length & 0xFF);

    // 写入数据
    result.insert(result.end(), data.begin(), data.end());

    return result;
}

ByteBuffer MessageHandler::encode(const std::string& data) {
    return encode(ByteBuffer(data.begin(), data.end()));
}

bool MessageHandler::decode(const ByteBuffer& buffer, ByteBuffer& message) {
    if (buffer.size() < 4) {
        return false;
    }

    // 读取长度头
    uint32_t length = (static_cast<uint32_t>(buffer[0]) << 24) |
                      (static_cast<uint32_t>(buffer[1]) << 16) |
                      (static_cast<uint32_t>(buffer[2]) << 8) |
                      static_cast<uint32_t>(buffer[3]);

    if (buffer.size() < 4 + length) {
        return false;
    }

    // 提取消息
    message.assign(buffer.begin() + 4, buffer.begin() + 4 + length);
    return true;
}

void MessageHandler::processBuffer(const ByteBuffer& buffer,
                                   std::function<void(const ByteBuffer&)> handler) {
    size_t offset = 0;

    while (offset < buffer.size()) {
        ByteBuffer message;
        ByteBuffer remaining(buffer.begin() + offset, buffer.end());

        if (decode(remaining, message)) {
            handler(message);
            offset += 4 + message.size();
        } else {
            break;
        }
    }
}

} // namespace wingman::transport
