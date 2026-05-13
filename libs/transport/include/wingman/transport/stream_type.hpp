#pragma once

#include <cstdint>
#include <string>

namespace wingman::transport {

/**
 * @brief 流类型
 *
 * 三流分离架构：每种流使用独立的 Socket 连接
 */
enum class StreamType : uint8_t {
    CONTROL = 0,    // 控制流 - 命令、响应、配置
    SCREEN  = 1,    // 屏幕流 - 截图、帧数据
    EVENT   = 2,    // 事件流 - 触发器、通知、日志
};

/**
 * @brief 获取流类型名称
 */
inline const char* streamTypeName(StreamType type) {
    switch (type) {
        case StreamType::CONTROL: return "CONTROL";
        case StreamType::SCREEN:  return "SCREEN";
        case StreamType::EVENT:   return "EVENT";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief 流参数配置
 *
 * 针对不同流类型优化 Socket 参数
 */
struct StreamParams {
    StreamType type;

    // Socket 选项
    bool tcpNoDelay = false;      // 禁用 Nagle 算法（低延迟）
    bool tcpCork = false;         // 批量传输（高吞吐）
    bool keepAlive = false;       // 保持连接
    int sendBufferSize = 0;       // 发送缓冲区大小（字节）
    int recvBufferSize = 0;       // 接收缓冲区大小（字节）
    int keepAliveIdle = 0;        // keep-alive 空闲时间（秒）
    int keepAliveInterval = 0;    // keep-alive 间隔（秒）
    int keepAliveCount = 0;       // keep-alive 重试次数

    // 流配置
    size_t maxMessageSize = 0;    // 最大消息大小（字节）
    int timeoutMs = 0;            // 操作超时（毫秒）

    /**
     * @brief 获取默认配置
     */
    static StreamParams getDefault(StreamType type) {
        switch (type) {
            case StreamType::CONTROL:
                // 控制流：低延迟优先
                return {
                    .type = type,
                    .tcpNoDelay = true,           // 禁用 Nagle，立即发送
                    .keepAlive = true,
                    .keepAliveIdle = 30,
                    .keepAliveInterval = 5,
                    .keepAliveCount = 3,
                    .maxMessageSize = 1 * 1024 * 1024,  // 1MB
                    .timeoutMs = 5000
                };

            case StreamType::SCREEN:
                // 屏幕流：高吞吐优先
                return {
                    .type = type,
                    .tcpCork = true,              // 批量传输
                    .sendBufferSize = 256 * 1024,  // 256KB
                    .recvBufferSize = 256 * 1024,
                    .maxMessageSize = 16 * 1024 * 1024,  // 16MB
                    .timeoutMs = 10000
                };

            case StreamType::EVENT:
                // 事件流：低频、保活
                return {
                    .type = type,
                    .tcpNoDelay = true,
                    .keepAlive = true,
                    .keepAliveIdle = 60,
                    .keepAliveInterval = 10,
                    .keepAliveCount = 3,
                    .maxMessageSize = 256 * 1024,  // 256KB
                    .timeoutMs = 3000
                };
        }
        return {};
    }
};

} // namespace wingman::transport
