#include "wingman/runtime/controllers/websocket.hpp"
#include "wingman/runtime/controllers/script.hpp"
#include "wingman/runtime/controllers/system.hpp"
#include <nlohmann/json.hpp>

namespace wingman::runtime::controllers {

// ========== 静态成员初始化 ==========
std::unordered_map<std::string, drogon::WebSocketConnectionPtr> WebSocketCtrl::connections_;
std::unordered_map<std::string, std::string> WebSocketCtrl::connectionIds_;
std::mutex WebSocketCtrl::mutex_;
std::atomic<uint64_t> WebSocketCtrl::connectionCounter_{0};

// ========== WebSocket 控制器实现 ==========

void WebSocketCtrl::handleNewConnection(const drogon::HttpRequestPtr& req,
                                       const drogon::WebSocketConnectionPtr& wsConnPtr) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string connId = generateConnectionId();
    connections_[connId] = wsConnPtr;
    connectionIds_[wsConnPtr.get()] = connId;

    spdlog::info("[WS] New connection: {} (total: {})", connId, connections_.size());

    // 发送欢迎消息
    nlohmann::json welcome;
    welcome["type"] = "connected";
    welcome["connectionId"] = connId;
    welcome["server"] = "wingman";
    welcome["version"] = WINGMAN_VERSION;

    wsConnPtr->send(welcome.dump());
}

void WebSocketCtrl::handleNewMessage(const drogon::WebSocketConnectionPtr& wsConnPtr,
                                    std::string&& message,
                                    const drogon::WebSocketMessageType& type) {
    if (type != drogon::WebSocketMessageType::Text) {
        return;
    }

    std::string connId = getConnectionId(wsConnPtr);
    if (connId.empty()) {
        spdlog::warn("[WS] Message from unknown connection");
        return;
    }

    try {
        auto j = nlohmann::json::parse(message);
        std::string msgType = j.value("type", "");

        spdlog::debug("[WS] Message from {}: type={}", connId, msgType);

        if (msgType == "call") {
            // RPC 调用
            RpcRequest req;
            req.id = j.value("id", "");
            req.method = j.value("method", "");
            req.params = j.value("params", nlohmann::json::object());

            handleRpcCall(wsConnPtr, req);

        } else if (msgType == "subscribe") {
            // 订阅事件
            handleSubscribe(wsConnPtr, j.value("params", nlohmann::json::object()));

        } else if (msgType == "unsubscribe") {
            // 取消订阅
            // TODO: 实现取消订阅

        } else if (msgType == "ping") {
            // 心跳
            nlohmann::json pong;
            pong["type"] = "pong";
            pong["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
            wsConnPtr->send(pong.dump());

        } else {
            spdlog::warn("[WS] Unknown message type: {}", msgType);
        }

    } catch (const std::exception& e) {
        spdlog::error("[WS] Message parse error: {}", e.what());

        nlohmann::json error;
        error["type"] = "error";
        error["message"] = e.what();
        wsConnPtr->send(error.dump());
    }
}

void WebSocketCtrl::handleConnectionClosed(const drogon::WebSocketConnectionPtr& wsConnPtr) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string connId = getConnectionId(wsConnPtr);
    if (!connId.empty()) {
        connections_.erase(connId);
        connectionIds_.erase(wsConnPtr.get());

        spdlog::info("[WS] Connection closed: {} (total: {})", connId, connections_.size());
    }
}

void WebSocketCtrl::handleRpcCall(const drogon::WebSocketConnectionPtr& wsConnPtr,
                                 const RpcRequest& req) {
    RpcResponse resp;
    resp.id = req.id;

    try {
        // 路由到对应的控制器
        if (req.method == "script.start") {
            resp = ScriptCtrl::start(req);
        } else if (req.method == "script.stop") {
            resp = ScriptCtrl::stop(req);
        } else if (req.method == "script.list") {
            resp = ScriptCtrl::list(req);
        } else if (req.method == "script.getContent") {
            resp = ScriptCtrl::getContent(req);
        } else if (req.method == "script.save") {
            resp = ScriptCtrl::save(req);
        } else if (req.method == "system.getStatus") {
            resp = SystemCtrl::getStatus(req);
        } else if (req.method == "system.getVersion") {
            resp = SystemCtrl::getVersion(req);
        } else {
            resp.success = false;
            resp.error = "Unknown method: " + req.method;
        }

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    // 发送响应
    nlohmann::json response;
    response["type"] = "result";
    response["data"] = resp.toJson();

    wsConnPtr->send(response.dump());
}

void WebSocketCtrl::handleSubscribe(const drogon::WebSocketConnectionPtr& wsConnPtr,
                                   const nlohmann::json& params) {
    // TODO: 实现事件订阅
    spdlog::debug("[WS] Subscribe request: {}", params.dump());

    nlohmann::json response;
    response["type"] = "subscribed";
    response["events"] = nlohmann::json::array();
    // response["events"] = {"script.*", "system.*"};

    wsConnPtr->send(response.dump());
}

void WebSocketCtrl::broadcast(const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string msgStr = message.dump();
    for (auto& [id, conn] : connections_) {
        if (conn->connected()) {
            conn->send(msgStr);
        }
    }
}

void WebSocketCtrl::send(const std::string& connId, const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(connId);
    if (it != connections_.end() && it->second->connected()) {
        it->second->send(message.dump());
    }
}

std::string WebSocketCtrl::generateConnectionId() {
    return "conn_" + std::to_string(++connectionCounter_);
}

std::string WebSocketCtrl::getConnectionId(const drogon::WebSocketConnectionPtr& wsConnPtr) {
    auto it = connectionIds_.find(wsConnPtr.get());
    if (it != connectionIds_.end()) {
        return it->second;
    }
    return "";
}

} // namespace wingman::runtime::controllers
