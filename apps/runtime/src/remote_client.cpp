#include "wingman/runtime/remote_client.hpp"
#include "wingman/transport/transport_client.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace wingman::runtime {

// ========== 辅助函数 ==========

namespace {

std::string getHostname() {
#ifdef _WIN32
    char buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buffer);
    if (GetComputerNameA(buffer, &size)) {
        return std::string(buffer);
    }
    return "windows-pc";
#else
    struct utsname uts;
    if (uname(&uts) == 0) {
        return std::string(uts.nodename);
    }
    return "unix-pc";
#endif
}

std::string generateAgentId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    return "agent_" + getHostname() + "_" + std::to_string(dis(gen));
}

} // anonymous namespace

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

    // Agent 信息
    std::string agentId;
    std::string hostname;
    bool registered = false;
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

        // 连接成功后发送 agent.register 消息
        sendRegister();

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
    // 发送 agent.heartbeat JSON 消息（与 server 侧 listener.go:342 匹配）
    nlohmann::json heartbeatMsg = {
        {"type", "agent.heartbeat"},
        {"status", "online"},
        {"resources", nlohmann::json::object()}
    };

    auto message = std::make_shared<transport::Message>();
    message->header.type = transport::MessageType::Notify;
    message->body = heartbeatMsg.dump();

    if (impl_->client->send(message)) {
        impl_->lastHeartbeat = std::chrono::steady_clock::now();
        spdlog::trace("Agent heartbeat sent");
    } else {
        spdlog::warn("Failed to send agent heartbeat");
        connected_.store(false);
    }
}

void RemoteClient::sendRegister() {
    // 生成或复用 agent ID
    if (impl_->agentId.empty()) {
        impl_->agentId = generateAgentId();
    }
    if (impl_->hostname.empty()) {
        impl_->hostname = getHostname();
    }

    // 发送 agent.register JSON 消息（与 server 侧 listener.go:340 匹配）
    nlohmann::json registerMsg = {
        {"type", "agent.register"},
        {"agentId", impl_->agentId},
        {"hostname", impl_->hostname}
    };

    auto message = std::make_shared<transport::Message>();
    message->header.type = transport::MessageType::Notify;
    message->body = registerMsg.dump();

    if (impl_->client->send(message)) {
        spdlog::info("Sent agent.register: {} ({})", impl_->agentId, impl_->hostname);
    } else {
        spdlog::warn("Failed to send agent.register");
    }
}

void RemoteClient::handleRegisterAck(const std::string& data) {
    try {
        auto ack = nlohmann::json::parse(data);
        bool success = ack.value("success", false);
        std::string agentId = ack.value("agentId", "");

        if (success && !agentId.empty()) {
            impl_->agentId = agentId;
            impl_->registered = true;
            spdlog::info("Agent registered successfully: {}", agentId);
            onEvent(ConnectionState::Connected, "Agent registered");
        } else {
            spdlog::warn("Agent registration failed");
            onEvent(ConnectionState::Error, "Registration failed");
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse register_ack: {}", e.what());
    }
}

void RemoteClient::onMessage(const std::vector<uint8_t>& data) {
    try {
        // 解析 JSON 消息
        std::string body(data.begin(), data.end());
        auto msg = nlohmann::json::parse(body);

        std::string type = msg.value("type", "");
        std::string event = msg.value("event", "");

        if (type == "agent.register_ack") {
            handleRegisterAck(body);
        } else if (event == "shutdown") {
            // server 下发的 system.shutdown 命令
            spdlog::info("Received shutdown command from server");
            if (commandCallback_) {
                CommandData cmdData;
                if (msg.contains("data") && msg["data"].is_object()) {
                    for (auto& [key, value] : msg["data"].items()) {
                        if (value.is_string()) {
                            cmdData[key] = value.get<std::string>();
                        }
                    }
                }
                commandCallback_("system.shutdown", cmdData);
            }
        } else if (type == "run_script") {
            // server 下发的 run_script 命令
            spdlog::info("Received run_script command from server");
            if (commandCallback_) {
                CommandData cmdData;
                for (auto& [key, value] : msg.items()) {
                    if (key != "type" && value.is_string()) {
                        cmdData[key] = value.get<std::string>();
                    }
                }
                commandCallback_("run_script", cmdData);
            }
        } else {
            spdlog::debug("Received message: type={}, event={}", type, event);
        }
    } catch (const std::exception& e) {
        // 如果不是 JSON，尝试旧的 PING/PONG 处理
        if (data.size() >= 4) {
            std::string type(data.begin(), data.begin() + 4);
            if (type == "PONG") {
                spdlog::trace("Heartbeat acknowledged (legacy format)");
                return;
            }
        }
        spdlog::debug("Received message: {} bytes (not JSON)", data.size());
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
