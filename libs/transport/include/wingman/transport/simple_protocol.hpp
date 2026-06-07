#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <system_error>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace wingman::transport {

/**
 * @brief 简化的消息格式
 *
 * 由于三流分离，每个流只处理一种数据类型，协议可以大幅简化：
 *
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * |                    LENGTH (4 bytes, network byte order)           |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 * |                          PAYLOAD (LENGTH bytes)                   |
 * +--------+--------+--------+--------+--------+--------+--------+--------+
 *
 * 即: [4字节长度] [数据]
 *
 * 不需要 MAGIC 的原因:
 * 1. 每个 Socket 连接只传输一种类型的数据
 * 2. 连接建立时已协商好数据格式
 * 3. TCP 是可靠传输，不需要额外的同步标记
 */
class SimpleMessage {
public:
    // Maximum message size (16 MiB) - consistent with Session limit for security
    static constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;
    static constexpr size_t LENGTH_SIZE = 4;

    /**
     * @brief 创建消息
     * @param payload 消息负载数据
     * @return 消息对象，如果数据过大则返回 nullptr
     */
    static std::unique_ptr<SimpleMessage> create(const std::vector<uint8_t>& payload);

    /**
     * @brief 从字符串创建消息
     * @param payload 消息负载字符串
     * @return 消息对象
     */
    static std::unique_ptr<SimpleMessage> create(const std::string& payload);

    /**
     * @brief 序列化消息（网络字节序）
     * @return 序列化后的字节数组
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief 获取负载
     * @return 负载数据的常量引用
     */
    const std::vector<uint8_t>& getPayload() const { return payload_; }

    /**
     * @brief 获取负载作为字符串
     * @return 负载数据的字符串形式
     */
    std::string getPayloadAsString() const {
        return std::string(payload_.begin(), payload_.end());
    }

    /**
     * @brief 获取负载大小
     * @return 负载数据的字节数
     */
    size_t size() const { return payload_.size(); }

private:
    std::vector<uint8_t> payload_;

    explicit SimpleMessage(const std::vector<uint8_t>& payload);
};

/**
 * @brief 消息接收器
 *
 * 处理 TCP 流的分包和粘包问题
 */
class MessageReceiver {
public:
    MessageReceiver() = default;
    ~MessageReceiver() = default;

    // 禁止拷贝
    MessageReceiver(const MessageReceiver&) = delete;
    MessageReceiver& operator=(const MessageReceiver&) = delete;

    /**
     * @brief 接收数据，返回完整消息
     * @param data 新接收到的数据
     * @param size 数据大小
     * @return 完整的消息列表（可能为空）
     */
    std::vector<std::unique_ptr<SimpleMessage>> receive(const uint8_t* data, size_t size);

    /**
     * @brief 接收数据（字符串版本）
     * @param data 字符串数据
     * @return 完整的消息列表
     */
    std::vector<std::unique_ptr<SimpleMessage>> receive(const std::string& data);

    /**
     * @brief 清除缓冲区
     */
    void clear();

    /**
     * @brief 获取缓冲区大小
     * @return 当前缓冲区字节数
     */
    size_t getBufferSize() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;

    /**
     * @brief 尝试从缓冲区解析消息
     * @return 解析出的消息，如果没有完整消息则返回 nullptr
     */
    std::unique_ptr<SimpleMessage> tryParseMessage();
};

/**
 * @brief 协议辅助函数
 *
 * 提供网络字节序转换和 Socket I/O 操作
 */
namespace Protocol {

/**
 * @brief Socket 类型定义
 */
#ifdef _WIN32
    using SocketType = SOCKET;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
#else
    using SocketType = int;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
#endif

/**
 * @brief 32位整数转网络字节序（大端）
 * @param hostlong 主机字节序的32位整数
 * @return 网络字节序的32位整数
 */
inline uint32_t hostToNetwork32(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) |
           ((hostlong & 0xFF00) << 8) |
           ((hostlong & 0xFF0000) >> 8) |
           ((hostlong & 0xFF000000) >> 24);
}

/**
 * @brief 网络字节序转32位整数（大端转主机序）
 * @param netlong 网络字节序的32位整数
 * @return 主机字节序的32位整数
 */
inline uint32_t networkToHost32(uint32_t netlong) {
    return ((netlong & 0xFF) << 24) |
           ((netlong & 0xFF00) << 8) |
           ((netlong & 0xFF0000) >> 8) |
           ((netlong & 0xFF000000) >> 24);
}

/**
 * @brief 写入消息（完整发送）
 * @param socket Socket 句柄
 * @param message 要发送的消息
 * @param ec 错误码（输出）
 * @return 成功返回 true，失败返回 false
 */
bool sendMessage(SocketType socket, const SimpleMessage& message, std::error_code& ec);

/**
 * @brief 读取消息（完整接收）
 * @param socket Socket 句柄
 * @param ec 错误码（输出）
 * @return 读取到的消息，失败返回 nullptr
 */
std::unique_ptr<SimpleMessage> readMessage(SocketType socket, std::error_code& ec);

} // namespace Protocol

} // namespace wingman::transport
