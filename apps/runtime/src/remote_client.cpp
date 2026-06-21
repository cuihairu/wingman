#include "wingman/runtime/remote_client.hpp"
#include "wingman/runtime/event_buffer.hpp"
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

    // 重连退避状态
    std::atomic<int> reconnectAttempt{0};

    // 断线期间的出站消息缓冲（仅 Notify/事件类，Response 类不缓冲以免陈旧）
    static constexpr size_t kMaxOutbox = 100;
    std::mutex outboxMutex;
    std::queue<transport::MessagePtr> outbox;
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

    // 设置消息处理回调（传递完整消息，包含 header）
    impl_->client->setMessageHandler([this](const transport::MessagePtr& msg) {
        onMessage(msg);
    });

    // 设置事件处理回调
    impl_->client->setEventHandler([this](transport::Session* session, transport::SessionEvent event) {
        switch (event) {
            case transport::SessionEvent::Connected:
                connected_.store(true);
                impl_->lastHeartbeat = std::chrono::steady_clock::now();
                onEvent(ConnectionState::Connected, "Connected to server");
                break;
            case transport::SessionEvent::Disconnected:
                connected_.store(false);
                impl_->registered = false;
                onEvent(ConnectionState::Disconnected, "Disconnected from server");
                // 重连由 reconnectLoop 探测 isConnected() 自行处理，无需在此直接调用
                break;
            case transport::SessionEvent::Error:
                connected_.store(false);
                impl_->registered = false;
                onEvent(ConnectionState::Error, "Connection error");
                break;
            case transport::SessionEvent::Timeout:
                connected_.store(false);
                impl_->registered = false;
                onEvent(ConnectionState::Error, "Connection timeout");
                break;
        }
    });

    // 连接服务器
    if (!impl_->client->connect(impl_->config.serverIp, impl_->config.serverPort)) {
        spdlog::warn("Failed to connect to server {}:{}, will retry in background",
            impl_->config.serverIp, impl_->config.serverPort);
        onEvent(ConnectionState::Error, "Failed to connect to server");

        running_.store(true);
        // 启动持久重连循环（指数退避）
        startReconnectLoop();
        return false;  // 连接未建立，后台重连中
    }

    connected_.store(true);
    impl_->lastHeartbeat = std::chrono::steady_clock::now();
    impl_->reconnectAttempt.store(0);  // 连接成功，重置退避计数
    sendRegister();

    // 启动心跳
    startHeartbeat();

    // 启动持久重连循环（用于处理后续断线重连）
    startReconnectLoop();

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

        // 冲刷断线期间缓冲的出站消息
        flushOutbox();

        startHeartbeat();
    } else {
        onEvent(ConnectionState::Error, "Connection failed");
    }
}

void RemoteClient::disconnect() {
    connected_.store(false);
    impl_->client->disconnect();
}

void RemoteClient::reconnectLoop() {
    spdlog::info("Reconnect loop started (base={}s, cap={}s)",
        impl_->config.reconnectInterval, impl_->config.maxReconnectInterval);
    while (!impl_->shouldStop.load()) {
        if (isConnected()) {
            // 已连接：重置退避计数，短休眠后再次探测
            impl_->reconnectAttempt.store(0);
            sleepInterruptible(1000);
            continue;
        }

        const int attempt = impl_->reconnectAttempt.fetch_add(1) + 1;
        const int delayMs = computeBackoff(attempt);
        spdlog::warn("Reconnect attempt {} to {}:{} (waiting {:.3f}s)",
            attempt, impl_->config.serverIp, impl_->config.serverPort, delayMs / 1000.0);
        onEvent(ConnectionState::Reconnecting,
            "Reconnecting (attempt " + std::to_string(attempt) + ", backoff "
            + std::to_string(delayMs / 1000) + "." + std::to_string(delayMs % 1000) + "s)");

        if (sleepInterruptible(delayMs)) {
            break;  // 停止信号
        }

        if (!impl_->shouldStop.load()) {
            connect();
        }
    }
    spdlog::info("Reconnect loop ended");
}

// computeBackoff 计算第 attempt 次重连的退避毫秒数：base * 2^(attempt-1)，封顶 cap，加 0~999ms 抖动。
// 返回毫秒以支持亚秒级抖动（修复：原先 /1000 整数除法使抖动退化为 0/1 秒二值）。
int RemoteClient::computeBackoff(int attempt) const {
    const int base = impl_->config.reconnectInterval > 0 ? impl_->config.reconnectInterval : 5;
    int cap = impl_->config.maxReconnectInterval > 0 ? impl_->config.maxReconnectInterval : 60;
    if (cap < base) {
        cap = base;
    }

    int delay = base;
    for (int i = 1; i < attempt && delay < cap; ++i) {
        delay *= 2;
        if (delay >= cap) {
            delay = cap;
            break;
        }
    }
    if (delay > cap) {
        delay = cap;
    }

    // 0~999 毫秒抖动，避免多个 agent 同时重连惊群
    std::uniform_int_distribution<> jitter(0, 999);
    std::random_device rd;
    const int jitterMs = jitter(rd);
    return delay * 1000 + jitterMs;  // 返回毫秒
}

// sleepInterruptible 可被停止信号打断的睡眠（毫秒粒度，100ms 轮询停止信号）；返回 true 表示被停止信号唤醒。
bool RemoteClient::sleepInterruptible(int milliseconds) {
    if (milliseconds < 0) {
        milliseconds = 0;
    }
    for (int slept = 0; slept < milliseconds; slept += 100) {
        if (impl_->shouldStop.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

// startReconnectLoop 启动持久重连循环（仅当未运行时）。
void RemoteClient::startReconnectLoop() {
    if (impl_->reconnectThread.joinable()) {
        return;  // 已有重连循环在运行
    }
    impl_->reconnectThread = std::thread([this]() { reconnectLoop(); });
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

    if (deliverMessage(message, true)) {
        impl_->lastHeartbeat = std::chrono::steady_clock::now();
        spdlog::trace("Agent heartbeat sent");
    } else {
        spdlog::warn("Failed to send agent heartbeat (queued for reconnect)");
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

    if (deliverMessage(message, true)) {
        spdlog::info("Sent agent.register: {} ({})", impl_->agentId, impl_->hostname);
    } else {
        spdlog::warn("Failed to send agent.register (queued for reconnect)");
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

void RemoteClient::sendAgentEvent(const std::string& event, const nlohmann::json& data) {
    // 发送 agent.event Notify（与 server 侧 listener.go:384 case "agent.event" 匹配）。
    // queueable=true：断线时入 outbox，重连后 flush，避免事件丢失。
    nlohmann::json msg = {
        {"type", "agent.event"},
        {"event", event},
        {"data", data},
    };
    auto message = std::make_shared<transport::Message>();
    message->header.type = transport::MessageType::Notify;
    message->body = msg.dump();
    deliverMessage(message, true);
}

void RemoteClient::onMessage(const transport::MessagePtr& msg) {
    // 处理不同类型的消息
    switch (msg->header.type) {
        case transport::MessageType::Notify:
            // Notify 消息（注册确认、心跳响应等）
            handleNotifyMessage(msg);
            break;

        case transport::MessageType::Request:
            // Request 消息（server 下发命令，需要回复 Response）
            handleRequestMessage(msg);
            break;

        case transport::MessageType::Response:
            // Response 消息（我们暂时不主动发请求，所以忽略）
            spdlog::trace("Received Response message (seq={})", msg->header.sequence);
            break;

        default:
            spdlog::warn("Unknown message type: {}", static_cast<int>(msg->header.type));
            break;
    }
}

void RemoteClient::handleNotifyMessage(const transport::MessagePtr& msg) {
    try {
        std::string body(msg->body.begin(), msg->body.end());
        auto json = nlohmann::json::parse(body);

        std::string type = json.value("type", "");

        if (type == "agent.register_ack") {
            handleRegisterAck(body);
        } else {
            spdlog::debug("Received Notify: type={}", type);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse Notify message: {}", e.what());
    }
}

void RemoteClient::handleRequestMessage(const transport::MessagePtr& msg) {
    try {
        std::string body(msg->body.begin(), msg->body.end());
        auto json = nlohmann::json::parse(body);

        std::string method = json.value("method", "");
        std::string type = json.value("type", "");

        // 兼容旧格式：type 就是 method
        if (method.empty() && !type.empty()) {
            method = type;
        }

        spdlog::info("Received Request: method={}, seq={}", method, msg->header.sequence);

        // 检查命令回调是否绑定
        if (!commandCallback_) {
            spdlog::warn("No command callback bound, cannot execute method={}", method);

            // 发送失败响应
            auto responseMsg = std::make_shared<transport::Message>();
            responseMsg->header.type = transport::MessageType::Response;
            responseMsg->header.sequence = msg->header.sequence;
            responseMsg->header.reserved = 0;
            responseMsg->body = nlohmann::json{
                {"success", false},
                {"method", method},
                {"error", "command callback not bound"}
            }.dump();
            deliverMessage(responseMsg, false);
            return;
        }

        // 解析命令数据
        CommandData cmdData;
        for (auto& [key, value] : json.items()) {
            if (key != "type" && key != "method" && key != "sequence") {
                if (value.is_string()) {
                    cmdData[key] = value.get<std::string>();
                } else if (value.is_number()) {
                    cmdData[key] = value.dump();
                } else {
                    cmdData[key] = value.dump();
                }
            }
        }

        // 执行命令回调并获取实际执行结果
        CommandResult result = commandCallback_(method, cmdData);

        // 构造响应（使用 callback 返回的实际成功/失败状态）
        nlohmann::json response = {
            {"success", result.success},
            {"method", method}
        };

        // 如果有错误信息，添加到响应中
        if (!result.success && !result.message.empty()) {
            response["error"] = result.message;
        } else if (result.success && !result.message.empty()) {
            response["message"] = result.message;
        }

        if (!result.data.empty()) {
            try {
                response["data"] = nlohmann::json::parse(result.data);
            } catch (const std::exception&) {
                response["data"] = result.data;
            }
        }

        // 发送 Response（使用相同的 sequence）
        auto responseMsg = std::make_shared<transport::Message>();
        responseMsg->header.type = transport::MessageType::Response;
        responseMsg->header.sequence = msg->header.sequence;  // 保持相同的 sequence
        responseMsg->header.reserved = 0;
        responseMsg->body = response.dump();

        if (deliverMessage(responseMsg, false)) {
            spdlog::debug("Sent Response for method={}, seq={}", method, msg->header.sequence);
        } else {
            spdlog::warn("Failed to send Response for method={}, seq={}", method, msg->header.sequence);
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to handle Request message: {}", e.what());

        // 发送错误响应
        auto responseMsg = std::make_shared<transport::Message>();
        responseMsg->header.type = transport::MessageType::Response;
        responseMsg->header.sequence = msg->header.sequence;
        responseMsg->header.reserved = 0;
        responseMsg->body = nlohmann::json{
            {"success", false},
            {"error", e.what()}
        }.dump();

        deliverMessage(responseMsg, false);
    }
}

void RemoteClient::onEvent(ConnectionState state, const std::string& message) {
    const char* stateName =
        state == ConnectionState::Connected ? "connected" :
        state == ConnectionState::Connecting ? "connecting" :
        state == ConnectionState::Disconnected ? "disconnected" :
        state == ConnectionState::Reconnecting ? "reconnecting" : "error";
    spdlog::info("RemoteClient event: {} - {}", stateName, message);

    // 推送到 EventBuffer，GUI 经 events.drain 拉取后更新远程连接指示器。
    nlohmann::json payload = {
        {"state", stateName},
        {"message", message},
    };
    EventBuffer::instance().push("connection.state_changed", std::move(payload));

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

// deliverMessage 投递出站消息：已连接则即时发送；失败且 queueable 时入断线缓冲。
// Response/Error 类（queueable=false）不缓冲——重连后已陈旧，直接丢弃。
bool RemoteClient::deliverMessage(const transport::MessagePtr& msg, bool queueable) {
    if (isConnected() && impl_->client && impl_->client->send(msg)) {
        return true;
    }
    if (queueable) {
        std::lock_guard<std::mutex> lock(impl_->outboxMutex);
        if (impl_->outbox.size() < impl_->kMaxOutbox) {
            impl_->outbox.push(msg);
        } else {
            spdlog::warn("Outbox full ({}), dropping outbound message", impl_->kMaxOutbox);
        }
    }
    return false;
}

// flushOutbox 重连成功后冲刷缓冲的 Notify/事件消息。
void RemoteClient::flushOutbox() {
    std::queue<transport::MessagePtr> drained;
    {
        std::lock_guard<std::mutex> lock(impl_->outboxMutex);
        drained.swap(impl_->outbox);
    }
    if (drained.empty()) {
        return;
    }
    spdlog::info("Flushing {} buffered outbound message(s)", drained.size());
    while (!drained.empty()) {
        if (impl_->client) {
            impl_->client->send(drained.front());
        }
        drained.pop();
    }
}

} // namespace wingman::runtime
