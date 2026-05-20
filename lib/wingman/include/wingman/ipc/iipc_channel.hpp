#pragma once

#include "wingman/platform/platform_types.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace wingman::ipc {

/**
 * @brief IPC 传输类型
 */
enum class IpcTransport : uint8_t {
    Auto,           // 自动选择最佳方案
    NamedPipe,      // Windows Named Pipe
    UnixSocket,     // Unix Domain Socket
    TcpPipe         // TCP (回退方案)
};

/**
 * @brief IPC 消息类型
 */
enum class IpcMessageType : uint8_t {
    Request,        // 请求
    Response,       // 响应
    Event,          // 事件（单向）
    Error           // 错误
};

/**
 * @brief IPC 消息
 */
struct IpcMessage {
    IpcMessageType type;
    std::string method;       // 方法名（如 "script.start"）
    std::string payload;      // JSON payload
    uint64_t id;             // 消息 ID（请求/响应匹配）
    uint64_t timestamp;      // 时间戳

    IpcMessage() : type(IpcMessageType::Request), id(0), timestamp(0) {}
};

/**
 * @brief 消息回调
 */
using MessageCallback = std::function<void(const IpcMessage&)>;

/**
 * @brief 错误回调
 */
using ErrorCallback = std::function<void(const std::string&)>;

/**
 * @brief IPC 状态
 */
enum class IpcState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief IPC 通道接口
 *
 * 抽象的进程间通信通道，支持多种传输方式。
 */
class IIpcChannel {
public:
    virtual ~IIpcChannel() = default;

    // ========== 连接管理 ==========

    /**
     * @brief 连接到服务端
     * @param endpoint 端点（Named Pipe 名字 / Unix Socket 路径 / TCP 地址）
     * @return 成功返回 true
     */
    virtual bool connect(const std::string& endpoint) = 0;

    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;

    /**
     * @brief 检查连接状态
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief 获取当前状态
     */
    virtual IpcState getState() const = 0;

    // ========== 消息发送 ==========

    /**
     * @brief 发送消息
     * @param message 消息内容
     * @return 成功返回 true
     */
    virtual bool send(const IpcMessage& message) = 0;

    /**
     * @brief 发送请求（自动生成 ID）
     * @param method 方法名
     * @param payload JSON payload
     * @return 消息 ID，失败返回 0
     */
    virtual uint64_t sendRequest(const std::string& method, const std::string& payload = "{}") = 0;

    /**
     * @brief 发送事件（单向）
     * @param method 事件名
     * @param payload JSON payload
     * @return 成功返回 true
     */
    virtual bool sendEvent(const std::string& method, const std::string& payload = "{}") = 0;

    // ========== 消息接收 ==========

    /**
     * @brief 设置消息回调
     * @param callback 回调函数
     */
    virtual void setMessageCallback(MessageCallback callback) = 0;

    /**
     * @brief 设置错误回调
     * @param callback 回调函数
     */
    virtual void setErrorCallback(ErrorCallback callback) = 0;

    /**
     * @brief 开始接收消息（异步）
     */
    virtual void startReceiving() = 0;

    /**
     * @brief 停止接收消息
     */
    virtual void stopReceiving() = 0;

    // ========== 后端信息 ==========

    /**
     * @brief 获取传输类型
     */
    virtual IpcTransport getTransport() const = 0;

    /**
     * @brief 获取后端名称
     */
    virtual std::string getBackendName() const = 0;

    /**
     * @brief 获取端点
     */
    virtual std::string getEndpoint() const = 0;
};

} // namespace wingman::ipc
