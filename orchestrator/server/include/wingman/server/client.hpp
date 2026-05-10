#pragma once

#include <asio.hpp>
#include <string>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <atomic>
#include "wingman/server/protocol.hpp"
#include "wingman/server/resource_collector.hpp"

namespace wingman::server {

using asio::ip::tcp;

// 连接状态事件回调
using StateCallback = std::function<void(bool connected)>;
using MessageCallback = std::function<void(const Response&)>;

// TCP Client
class Client : public std::enable_shared_from_this<Client> {
public:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    explicit Client(asio::io_context& ioContext);
    ~Client();

    // ========== 连接管理 ==========

    // 连接到服务器
    std::future<bool> connect(const std::string& host, unsigned short port);
    void disconnect();

    // 检查连接状态
    bool isConnected() const;

    // ========== 配置 ==========

    void setAgentId(const std::string& id) { agentId_ = id; }
    std::string getAgentId() const { return agentId_; }

    // 启用/禁用自动重连
    void enableAutoReconnect(bool enable = true, std::chrono::seconds interval = std::chrono::seconds(5));

    // ========== 事件回调 ==========

    void setStateCallback(StateCallback callback) { stateCallback_ = std::move(callback); }
    void setMessageCallback(MessageCallback callback) { messageCallback_ = std::move(callback); }

    // ========== 消息发送 ==========

    // 发送请求并等待响应（异步回调）
    void send(const Request& request, MessageCallback callback);

    // 发送请求并等待响应（同步）
    std::future<Response> sendSync(const Request& request);

    // ========== 心跳 ==========

    void startHeartbeat(std::chrono::seconds interval = std::chrono::seconds(15));
    void stopHeartbeat();

    void setHeartbeatData(const nlohmann::json& data) { heartbeatData_ = data; }

    // 资源收集
    ResourceStats collectResources() const { return resourceCollector_.collect(); }

private:
    asio::io_context& ioContext_;
    std::unique_ptr<tcp::socket> socket_;
    std::atomic<bool> connected_{false};
    std::mutex socketMutex_;
    asio::streambuf buffer_;

    // 配置
    std::string agentId_;
    std::string host_;
    unsigned short port_;

    // 资源收集器
    ResourceCollector resourceCollector_;

    // 自动重连
    bool autoReconnect_ = false;
    std::chrono::seconds reconnectInterval_{5};
    asio::steady_timer reconnectTimer_;

    // 心跳
    asio::steady_timer heartbeatTimer_;
    std::chrono::seconds heartbeatInterval_{15};
    nlohmann::json heartbeatData_;
    std::atomic<bool> heartbeatRunning_{false};

    // 事件回调
    StateCallback stateCallback_;
    MessageCallback messageCallback_;

    // 内部方法
    void doRead();
    void doWrite(const std::string& data);
    void notifyStateChange(bool connected);

    // 自动重连
    void doAutoReconnect();
    void scheduleReconnect();

    // 心跳
    void doHeartbeat();
    void scheduleHeartbeat();
};

} // namespace wingman::server
