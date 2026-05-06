#pragma once

#include <crow.h>
#include <crow/websocket.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_set>
#include <mutex>
#include <thread>
#include "wingman/auth.hpp"

namespace wingman {

// WebSocket 消息类型
enum class WSMessageType {
    // Agent 事件
    AgentConnected,
    AgentDisconnected,
    AgentStatusChanged,
    // 工作流事件
    WorkflowSubmitted,
    WorkflowStatusChanged,
    WorkflowProgress,
    // 系统事件
    SystemStatus,
    Error
};

// WebSocket 消息
struct WSMessage {
    WSMessageType type;
    std::string data;  // JSON string

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["type"] = static_cast<int>(type);
        j["data"] = data;
        return j;
    }

    std::string toString() const {
        return toJson().dump();
    }
};

// WebSocket 连接包装
class WSConnection {
public:
    std::shared_ptr<crow::websocket::connection> connection;
    std::string id;
    std::chrono::system_clock::time_point lastPing;

    WSConnection(std::shared_ptr<crow::websocket::connection> conn, const std::string& id)
        : connection(conn), id(id), lastPing(std::chrono::system_clock::now()) {}

    void send(const std::string& msg) {
        if (connection && connection->is_open()) {
            connection->send_text(msg);
        }
    }

    void close() {
        if (connection && connection->is_open()) {
            connection->close();
        }
    }

    bool isOpen() const {
        return connection && connection->is_open();
    }
};

class HTTPServer {
public:
    HTTPServer(const std::string& dbPath = "wingman.db", int port = 9527, const std::string& staticDir = "dist");
    ~HTTPServer();

    void start();
    void stop();

    void setPort(int port) { port_ = port; }
    void setStaticDir(const std::string& dir) { staticDir_ = dir; }

    // ========== WebSocket 广播 ==========

    // 广播消息到所有连接
    void broadcast(const WSMessage& message);

    // 广播 Agent 事件
    void broadcastAgentEvent(const std::string& eventType, const nlohmann::json& agentData);

    // 广播工作流事件
    void broadcastWorkflowEvent(const std::string& eventType, const nlohmann::json& workflowData);

private:
    int port_;
    std::string staticDir_;
    std::unique_ptr<AuthManager> authManager_;
    crow::SimpleApp app_;

    // WebSocket 连接管理
    std::unordered_set<std::shared_ptr<WSConnection>> wsConnections_;
    std::mutex wsMutex_;
    std::atomic<uint64_t> wsConnectionIdCounter_{0};

    // WebSocket 心跳检测
    std::thread wsHeartbeatThread_;
    std::atomic<bool> wsHeartbeatRunning_{false};

    void setupRoutes();

    // WebSocket 路由设置
    void setupWebSocketRoutes();

    // WebSocket 事件处理
    void onWSOpen(std::shared_ptr<crow::websocket::connection> conn);
    void onWSMessage(std::shared_ptr<crow::websocket::connection> conn, const std::string& message, bool is_binary);
    void onWSClose(std::shared_ptr<crow::websocket::connection> conn, const std::string& reason);

    // Helper to add CORS headers to responses
    void addCorsHeaders(crow::response& resp);

    // Middleware for authentication
    bool authenticate(const crow::request& req, User& user);

    // Route handlers
    crow::response handleLogin(const crow::request& req);
    crow::response handleLogout(const crow::request& req);
    crow::response handleStatus(const crow::request& req);
    crow::response handleScripts(const crow::request& req);
    crow::response handleWindows(const crow::request& req);
    crow::response handleSettings(const crow::request& req);

    // API response helpers
    static crow::response jsonResponse(const nlohmann::json& data);
    static crow::response errorResponse(const std::string& message);
};

} // namespace wingman
