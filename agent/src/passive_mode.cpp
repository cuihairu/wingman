#include "wingman/agent/passive_mode.hpp"
#include "wingman/transport/transport_server.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace wingman::agent {

// ========== PassiveMode 实现 ==========

class PassiveMode::Impl {
public:
    PassiveModeConfig config;
    std::unique_ptr<transport::TcpServer> server;
};

PassiveMode::PassiveMode(const PassiveModeConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {
    impl_->config = config;
    impl_->server = transport::createTcpServer();
}

PassiveMode::~PassiveMode() {
    stop();
}

bool PassiveMode::start() {
    if (running_.load()) {
        spdlog::warn("PassiveMode already running");
        return true;
    }

    spdlog::info("Starting PassiveMode - listening on {}:{}",
        config_.listenIp, config_.listenPort);

    // 设置消息处理回调
    impl_->server->setMessageHandler([this](const transport::MessagePtr& msg) {
        // 获取会话 ID
        auto sessionIds = impl_->server->getSessionIds();
        if (!sessionIds.empty()) {
            std::string sessionId = "session_" + std::to_string(sessionIds[0]);
            onMessage(sessionId, std::vector<uint8_t>(msg->body.begin(), msg->body.end()));
        }
    });

    // 设置事件处理回调
    impl_->server->setEventHandler([this](transport::Session* session, transport::SessionEvent event) {
        if (!session) return;

        std::string sessionId = "session_" + std::to_string(session->getId());

        switch (event) {
            case transport::SessionEvent::Connected:
                onNewSession(sessionId);
                onSessionEvent(sessionId, SessionEventType::Connected, "Client connected");
                break;

            case transport::SessionEvent::Disconnected:
                onSessionEvent(sessionId, SessionEventType::Disconnected, "Client disconnected");
                break;

            case transport::SessionEvent::Error:
                onSessionEvent(sessionId, SessionEventType::Error, "Session error");
                break;
        }
    });

    // 启动服务器
    if (!impl_->server->start()) {
        spdlog::error("Failed to start server");
        return false;
    }

    // 开始监听
    if (!impl_->server->listen(config_.listenIp, config_.listenPort)) {
        spdlog::error("Failed to listen on {}:{}", config_.listenIp, config_.listenPort);
        impl_->server->stop();
        return false;
    }

    running_.store(true);
    spdlog::info("PassiveMode started - listening on {}:{}", config_.listenIp, config_.listenPort);
    return true;
}

void PassiveMode::stop() {
    if (!running_.load()) {
        return;
    }

    spdlog::info("Stopping PassiveMode");
    running_.load(false);

    if (impl_->server) {
        impl_->server->stop();
    }

    // 清空会话
    {
        std::lock_guard lock(sessionsMutex_);
        sessions_.clear();
    }

    spdlog::info("PassiveMode stopped");
}

size_t PassiveMode::getSessionCount() const {
    return impl_->server ? impl_->server->getSessionCount() : 0;
}

std::vector<std::string> PassiveMode::getSessionIds() const {
    std::vector<std::string> result;
    if (!impl_->server) return result;

    auto ids = impl_->server->getSessionIds();
    result.reserve(ids.size());
    for (auto id : ids) {
        result.push_back("session_" + std::to_string(id));
    }
    return result;
}

SessionInfo PassiveMode::getSession(const std::string& id) {
    std::lock_guard lock(sessionsMutex_);
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        return it->second;
    }

    // 如果本地没有，尝试从服务器获取
    if (impl_->server) {
        // 解析 session ID
        if (id.size() > 8 && id.compare(0, 8, "session_") == 0) {
            try {
                auto numericId = std::stoull(id.substr(8));
                auto* session = impl_->server->getSession(numericId);
                if (session) {
                    SessionInfo info;
                    info.id = id;
                    info.connectTime = std::chrono::system_clock::now();
                    return info;
                }
            } catch (...) {
                // 忽略解析错误
            }
        }
    }

    return SessionInfo{};
}

bool PassiveMode::broadcast(const std::vector<uint8_t>& data) {
    if (!impl_->server || !running_.load()) {
        return false;
    }

    auto message = std::make_shared<transport::Message>();
    message->header.type = transport::MessageType::Notify;
    message->body.assign(data.begin(), data.end());

    impl_->server->broadcast(message);
    return true;
}

bool PassiveMode::send(const std::string& sessionId, const std::vector<uint8_t>& data) {
    if (!impl_->server || !running_.load()) {
        return false;
    }

    // 解析 session ID
    if (sessionId.size() > 8 && sessionId.compare(0, 8, "session_") == 0) {
        try {
            auto numericId = std::stoull(sessionId.substr(8));

            auto message = std::make_shared<transport::Message>();
            message->header.type = transport::MessageType::Notify;
            message->body.assign(data.begin(), data.end());

            return impl_->server->send(numericId, message);
        } catch (...) {
            return false;
        }
    }

    return false;
}

void PassiveMode::onNewSession(const std::string& sessionId) {
    spdlog::info("New session: {}", sessionId);

    SessionInfo info;
    info.id = sessionId;
    info.connectTime = std::chrono::system_clock::now();

    std::lock_guard lock(sessionsMutex_);
    sessions_[sessionId] = info;
}

void PassiveMode::onSessionEvent(const std::string& sessionId, SessionEventType type, const std::string& message) {
    spdlog::info("Session event: {} - type={}, message={}", sessionId,
        type == SessionEventType::Connected ? "Connected" :
        type == SessionEventType::Disconnected ? "Disconnected" : "Error",
        message);

    if (sessionCallback_) {
        SessionEvent event;
        event.sessionId = sessionId;
        event.type = type;
        event.message = message;
        sessionCallback_(event);
    }

    // 移除断开的会话
    if (type == SessionEventType::Disconnected) {
        std::lock_guard lock(sessionsMutex_);
        sessions_.erase(sessionId);
    }
}

void PassiveMode::onMessage(const std::string& sessionId, const std::vector<uint8_t>& data) {
    spdlog::trace("Received message from {}: {} bytes", sessionId, data.size());

    // 如果有注册的消息处理器，使用它
    if (messageHandler_) {
        auto response = messageHandler_(sessionId, data);
        if (!response.empty()) {
            // 发送响应（使用 Response 类型而非 Notify）
            if (impl_->server && running_.load()) {
                if (sessionId.size() > 8 && sessionId.compare(0, 8, "session_") == 0) {
                    try {
                        auto numericId = std::stoull(sessionId.substr(8));

                        auto message = std::make_shared<transport::Message>();
                        message->header.type = transport::MessageType::Response;
                        message->body.assign(response.begin(), response.end());

                        impl_->server->send(numericId, message);
                    } catch (...) {
                        spdlog::error("Failed to send response to {}: invalid session id", sessionId);
                    }
                }
            }
        }
    }
}

} // namespace wingman::agent
