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
#include "wingman/script_manager.hpp"

namespace wingman {

// WebSocket message types
enum class WSMessageType {
    // Agent events
    AgentConnected,
    AgentDisconnected,
    AgentStatusChanged,
    // Workflow events
    WorkflowSubmitted,
    WorkflowStatusChanged,
    WorkflowProgress,
    // Team Room events
    RoomJoined,
    RoomLeft,
    RoomMessage,
    RoomBroadcast,
    // System events
    SystemStatus,
    Error
};

// WebSocket message
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

// WebSocket connection wrapper
class WSConnection {
public:
    std::shared_ptr<crow::websocket::connection> connection;
    std::string id;
    std::string currentRoom;  // Currently joined room ID
    std::chrono::system_clock::time_point lastPing;

    WSConnection(std::shared_ptr<crow::websocket::connection> conn, const std::string& id)
        : connection(conn), id(id), currentRoom(""), lastPing(std::chrono::system_clock::now()) {}

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

// Room structure - used for Team Room and other multicast scenarios
struct Room {
    std::string roomId;
    std::unordered_set<std::string> connectionIds;  // Connection IDs in the room
    nlohmann::json metadata;  // Room metadata

    size_t size() const { return connectionIds.size(); }
    bool empty() const { return connectionIds.empty(); }
};

class HTTPServer {
public:
    HTTPServer(const std::string& dbPath = "wingman.db", int port = 9527, const std::string& staticDir = "dist");
    ~HTTPServer();

    void start();
    void stop();

    void setPort(int port) { port_ = port; }
    void setStaticDir(const std::string& dir) { staticDir_ = dir; }

    // ========== WebSocket broadcast ==========

    // Broadcast message to all connections
    void broadcast(const WSMessage& message);

    // Broadcast agent event
    void broadcastAgentEvent(const std::string& eventType, const nlohmann::json& agentData);

    // Broadcast workflow event
    void broadcastWorkflowEvent(const std::string& eventType, const nlohmann::json& workflowData);

    // Broadcast debugger event
    void broadcastDebuggerEvent(const std::string& eventType, const nlohmann::json& data);

    // ========== Room management ==========

    // Join room
    void joinRoom(const std::string& connId, const std::string& roomId);

    // Leave room
    void leaveRoom(const std::string& connId);

    // Send message to specified room
    void sendToRoom(const std::string& roomId, const nlohmann::json& message);

    // Broadcast message to specified room (excluding sender)
    void broadcastToRoom(const std::string& roomId, const std::string& excludeConnId, const nlohmann::json& message);

    // Get room information
    std::vector<std::string> getRoomConnections(const std::string& roomId);

private:
    int port_;
    std::string staticDir_;
    std::unique_ptr<AuthManager> authManager_;
    std::unique_ptr<ScriptManager> scriptManager_;
    crow::SimpleApp app_;

    // WebSocket connection management
    std::unordered_set<std::shared_ptr<WSConnection>> wsConnections_;
    std::mutex wsMutex_;
    std::atomic<uint64_t> wsConnectionIdCounter_{0};

    // Room management
    std::unordered_map<std::string, Room> rooms_;
    std::mutex roomMutex_;

    // WebSocket heartbeat detection
    std::thread wsHeartbeatThread_;
    std::atomic<bool> wsHeartbeatRunning_{false};

    void setupRoutes();

    // WebSocket route setup
    void setupWebSocketRoutes();

    // WebSocket event handling
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
    crow::response handleScriptContent(const crow::request& req);
    crow::response handleScriptSave(const crow::request& req);
    crow::response handleScriptRun(const crow::request& req);
    crow::response handleScriptStop(const crow::request& req);
    crow::response handleScriptLogs(const crow::request& req);
    crow::response handleWindows(const crow::request& req);
    crow::response handleSettings(const crow::request& req);

    // Debugger routes
    crow::response handleDebuggerConnect(const crow::request& req);
    crow::response handleDebuggerCommand(const crow::request& req);
    crow::response handleDebuggerBreakpoints(const crow::request& req);
    crow::response handleDebuggerStackTrace(const crow::request& req);
    crow::response handleDebuggerVariables(const crow::request& req);
    crow::response handleDebuggerEvaluate(const crow::request& req);
    crow::response handleDebuggerSetVariable(const crow::request& req);

    // API response helpers
    static crow::response jsonResponse(const nlohmann::json& data);
    static crow::response errorResponse(const std::string& message);
};

} // namespace wingman
