#include "wingman/runtime/remote_client.hpp"
#include "wingman/transport/transport_client.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace wingman::runtime {

// ========== RemoteClient 实现 ==========

class RemoteClient::Impl {
public:
    RemoteClientConfig config;
    transport::TcpClientPtr client;
    std::thread heartbeatThread;
    std::thread reconnectThread;
    std::atomic<bool> shouldStop{false};

    // 上次心跳时间
    std::chrono::steady_clock::time_point lastHeartbeat;
};

RemoteClient::RemoteClient(const RemoteClientConfig& config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->client = transport::createTcpClient();
}

RemoteClient::~RemoteClient() {
    stop();
}

bool RemoteClient::start() {
    if (running_.load()) {
        spdlog::warn("RemoteClient already running");
        return true;
    }

    spdlog::info("Starting RemoteClient - connecting to {}:{}",
        impl_->config.serverIp, impl_->config.serverPort);

    impl_->shouldStop.store(false);

    // 设置消息处理回调
    impl_->client->setMessageHandler([this](const transport::MessagePtr& msg) {
        onMessage(std::vector<uint8_t>(msg->body.begin(), msg->body.end()));
    });

    // 设置事件处理回调
    impl_->client->setEventHandler([this](transport::Session* session, transport::SessionEvent event) {
        switch (event) {
            case transport::SessionEvent::Connected:
                onEvent(ConnectionState::Connected, "Connected to server");
                break;
            case transport::SessionEvent::Disconnected:
                onEvent(ConnectionState::Disconnected, "Disconnected from server");
                // 尝试重连
                if (!impl_->shouldStop.load()) {
                    reconnect();
                }
                break;
            case transport::SessionEvent::Error:
                onEvent(ConnectionState::Error, "Connection error");
                break;
            case transport::SessionEvent::Timeout:
                onEvent(ConnectionState::Error, "Connection timeout");
                break;
        }
    });

    // 连接服务器
    if (!impl_->client->connect(impl_->config.serverIp, impl_->config.serverPort)) {
        spdlog::warn("Failed to connect to server {}:{}, will retry in background",
            impl_->config.serverIp, impl_->config.serverPort);
        onEvent(ConnectionState::Error, "Failed to connect to server");

        // 启动重连线程
        impl_->reconnectThread = std::thread([this]() {
            int retryCount = 0;
            while (!impl_->shouldStop.load() && !isConnected()) {
                std::this_thread::sleep_for(std::chrono::seconds(impl_->config.reconnectInterval));
                if (!impl_->shouldStop.load()) {
                    retryCount++;
                    spdlog::warn("Attempting to reconnect to {}:{} (retry {})",
                        impl_->config.serverIp, impl_->config.serverPort, retryCount);
                    connect();
                }
            }
        });

        running_.store(true);
        return false;  // 连接未建立，后台重连中
    }

    // 启动心跳
    startHeartbeat();

    running_.store(true);
    spdlog::info("RemoteClient started");
    return true;
}

void RemoteClient::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("Stopping RemoteClient");
    impl_->shouldStop.store(true);
    running_.store(false);
    connected_.store(false);

    // 停止心跳线程
    if (impl_->heartbeatThread.joinable()) {
        impl_->heartbeatThread.join();
    }

    // 停止重连线程
    if (impl_->reconnectThread.joinable()) {
        impl_->reconnectThread.join();
    }

    // 断开连接
    if (impl_->client) {
        impl_->client->disconnect();
    }

    spdlog::info("RemoteClient stopped");
}

void RemoteClient::connect() {
    if (isConnected()) {
        return;
    }

    onEvent(ConnectionState::Connecting, "Connecting to server...");

    if (impl_->client->connect(impl_->config.serverIp, impl_->config.serverPort)) {
        connected_.store(true);
        impl_->lastHeartbeat = std::chrono::steady_clock::now();
        startHeartbeat();
    } else {
        onEvent(ConnectionState::Error, "Connection failed");
    }
}

void RemoteClient::disconnect() {
    connected_.store(false);
    impl_->client->disconnect();
}

void RemoteClient::reconnect() {
    if (impl_->shouldStop.load()) {
        return;
    }

    onEvent(ConnectionState::Reconnecting, "Reconnecting to server...");

    // 等待重连间隔
    std::this_thread::sleep_for(std::chrono::seconds(impl_->config.reconnectInterval));

    if (!impl_->shouldStop.load()) {
        connect();
    }
}

void RemoteClient::startHeartbeat() {
    // 停止之前的心跳线程
    if (impl_->heartbeatThread.joinable()) {
        impl_->heartbeatThread.join();
    }

    impl_->lastHeartbeat = std::chrono::steady_clock::now();

    // 启动新的心跳线程
    impl_->heartbeatThread = std::thread([this]() {
        while (!impl_->shouldStop.load() && isConnected()) {
            std::this_thread::sleep_for(std::chrono::seconds(impl_->config.heartbeatInterval));
            if (!impl_->shouldStop.load() && isConnected()) {
                sendHeartbeat();
            }
        }
    });
}

void RemoteClient::sendHeartbeat() {
    // 创建心跳消息
    auto message = std::make_shared<transport::Message>();
    message->header.type = transport::MessageType::Notify;
    message->body = "PING";

    if (impl_->client->send(message)) {
        impl_->lastHeartbeat = std::chrono::steady_clock::now();
        spdlog::trace("Heartbeat sent");
    } else {
        spdlog::warn("Failed to send heartbeat");
        connected_.store(false);
    }
}

void RemoteClient::onMessage(const std::vector<uint8_t>& data) {
    // 解析消息
    if (data.size() >= 4) {
        std::string type(data.begin(), data.begin() + 4);

        if (type == "PONG") {
            // 心跳响应
            spdlog::trace("Heartbeat acknowledged");
        } else {
            // 其他消息交给上层处理
            spdlog::debug("Received message: {} bytes", data.size());
        }
    }
}

void RemoteClient::onEvent(ConnectionState state, const std::string& message) {
    spdlog::info("RemoteClient event: {} - {}",
        state == ConnectionState::Connected ? "Connected" :
        state == ConnectionState::Connecting ? "Connecting" :
        state == ConnectionState::Disconnected ? "Disconnected" :
        state == ConnectionState::Reconnecting ? "Reconnecting" : "Error",
        message);

    if (eventCallback_) {
        ConnectionEvent event;
        event.state = state;
        event.message = message;
        eventCallback_(event);
    }
}

const RemoteClientConfig& RemoteClient::getConfig() const {
    return impl_->config;
}

} // namespace wingman::runtime
