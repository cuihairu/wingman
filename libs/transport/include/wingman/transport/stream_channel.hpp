#pragma once

#include "wingman/transport/stream_type.hpp"
#include "wingman/transport/simple_protocol.hpp"
#include <functional>
#include <memory>
#include <system_error>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    typedef int SOCKET;
    #define INVALID_SOCKET_VALUE -1
#endif

namespace wingman::transport {

/**
 * @brief 数据接收回调
 */
using StreamDataCallback = std::function<void(const uint8_t* data, size_t size)>;

/**
 * @brief 错误回调
 */
using StreamErrorCallback = std::function<void(const std::error_code& ec)>;

/**
 * @brief 流通道状态
 */
enum class StreamState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief 流状态字符串
 */
inline const char* streamStateName(StreamState state) {
    switch (state) {
        case StreamState::Disconnected:  return "Disconnected";
        case StreamState::Connecting:    return "Connecting";
        case StreamState::Connected:     return "Connected";
        case StreamState::Disconnecting: return "Disconnecting";
        case StreamState::Error:         return "Error";
        default:                         return "Unknown";
    }
}

/**
 * @brief 单向流通道
 *
 * 每个流类型使用独立的 Socket 连接，针对该流的特性进行优化。
 */
class StreamChannel {
public:
    /**
     * @brief 构造函数
     * @param type 流类型
     * @param params 流参数
     */
    StreamChannel(StreamType type, const StreamParams& params);

    /**
     * @brief 析构函数（自动断开连接）
     */
    ~StreamChannel();

    // 禁止拷贝
    StreamChannel(const StreamChannel&) = delete;
    StreamChannel& operator=(const StreamChannel&) = delete;

    /**
     * @brief 连接到远程端点（客户端模式）
     * @param host 主机地址
     * @param port 端口
     * @return 成功返回 true
     */
    bool connect(const std::string& host, int port);

    /**
     * @brief 绑定并监听（服务端模式）
     * @param host 绑定地址
     * @param port 监听端口
     * @return 成功返回 true
     */
    bool listen(const std::string& host, int port);

    /**
     * @brief 接受连接（服务端模式）
     * @return 新的流通道，调用者负责管理其生命周期
     */
    std::unique_ptr<StreamChannel> accept();

    /**
     * @brief 断开连接
     */
    void disconnect();

    /**
     * @brief 发送数据
     * @param data 数据指针
     * @param size 数据大小
     * @return 成功返回 true
     */
    bool send(const uint8_t* data, size_t size);

    /**
     * @brief 发送数据（vector 版本）
     */
    bool send(const std::vector<uint8_t>& data);

    /**
     * @brief 发送字符串消息
     */
    bool send(const std::string& data);

    /**
     * @brief 开始异步接收数据
     * @param dataCallback 数据回调
     * @param errorCallback 错误回调（可选）
     */
    void startReceiving(StreamDataCallback dataCallback,
                       StreamErrorCallback errorCallback = nullptr);

    /**
     * @brief 停止接收
     */
    void stopReceiving();

    // 获取器
    StreamType getType() const { return type_; }
    StreamState getState() const { return state_.load(); }
    const std::string& getRemoteEndpoint() const { return remoteEndpoint_; }
    int getLocalPort() const { return localPort_; }
    Protocol::SocketType getSocket() const { return socket_; }
    const StreamParams& getParams() const { return params_; }

    /**
     * @brief 检查是否已连接
     */
    bool isConnected() const {
        return state_.load() == StreamState::Connected;
    }

    /**
     * @brief 设置数据回调（用于 startReceiving）
     */
    void setDataCallback(StreamDataCallback cb) { dataCallback_ = std::move(cb); }

    /**
     * @brief 设置错误回调
     */
    void setErrorCallback(StreamErrorCallback cb) { errorCallback_ = std::move(cb); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    StreamType type_;
    StreamParams params_;
    std::atomic<StreamState> state_{StreamState::Disconnected};
    std::string remoteEndpoint_;
    int localPort_ = 0;
    Protocol::SocketType socket_ = INVALID_SOCKET_VALUE;

    StreamDataCallback dataCallback_;
    StreamErrorCallback errorCallback_;

    // 接收线程
    std::thread receiveThread_;
    std::atomic<bool> receiving_{false};

    void setState(StreamState state);
    bool applySocketOptions();
    void receiveLoop();
};

/**
 * @brief 流通道对（双向通信）
 *
 * 客户端使用：一个 StreamChannelPair 包含请求和响应通道
 */
class StreamChannelPair {
public:
    /**
     * @brief 构造函数
     * @param requestParams 请求通道参数
     * @param responseParams 响应通道参数
     */
    StreamChannelPair(const StreamParams& requestParams,
                     const StreamParams& responseParams);

    ~StreamChannelPair() = default;

    // 禁止拷贝
    StreamChannelPair(const StreamChannelPair&) = delete;
    StreamChannelPair& operator=(const StreamChannelPair&) = delete;

    /**
     * @brief 连接到远程
     * @param host 主机地址
     * @param requestPort 请求端口
     * @param responsePort 响应端口
     * @return 成功返回 true
     */
    bool connect(const std::string& host, int requestPort, int responsePort);

    /**
     * @brief 断开所有连接
     */
    void disconnect();

    /**
     * @brief 检查连接状态
     */
    bool isConnected() const;

    // 获取通道
    StreamChannel* getRequestChannel() { return requestChannel_.get(); }
    StreamChannel* getResponseChannel() { return responseChannel_.get(); }

private:
    std::unique_ptr<StreamChannel> requestChannel_;
    std::unique_ptr<StreamChannel> responseChannel_;
};

} // namespace wingman::transport
