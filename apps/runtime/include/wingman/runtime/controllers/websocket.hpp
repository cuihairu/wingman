#pragma once

#include <drogon/WebSocketController.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <string>

namespace wingman::runtime::controllers {

/// WebSocket 连接状态
enum class ConnectionState {
    connecting,
    connected,
    disconnected,
    error
};

/// WebSocket 消息类型
enum class MessageType {
    // 客户端 → 服务端
    subscribe,
    unsubscribe,
    call,
    event,
    // 服务端 → 客户端
    subscribed,
    unsubscribed,
    result,
    notification,
    error,
    pong
};

/// RPC 调用请求
struct RpcRequest {
    std::string id;          // 请求 ID
    std::string method;      // 方法名
    nlohmann::json params;   // 参数
};

/// RPC 响应
struct RpcResponse {
    std::string id;          // 请求 ID
    bool success = false;
    nlohmann::json result;   // 结果
    std::string error;       // 错误信息

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["success"] = success;
        if (success) {
            j["result"] = result;
        } else {
            j["error"] = error;
        }
        return j;
    }
};

/// WebSocket 控制器
/// 处理与 Tauri UI 的 WebSocket 通信
class WebSocketCtrl : public drogon::WebSocketController<WebSocketCtrl> {
public:
    WS_PATH_LIST_BEGIN
        // 路径: /ws
    WS_PATH_LIST_END

    /// 连接建立
    void handleNewConnection(const drogon::HttpRequestPtr& req,
                            const drogon::WebSocketConnectionPtr& wsConnPtr) override;

    /// 消息接收
    void handleNewMessage(const drogon::WebSocketConnectionPtr& wsConnPtr,
                         std::string&& message,
                         const drogon::WebSocketMessageType& type) override;

    /// 连接关闭
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& wsConnPtr) override;

    /// 广播消息到所有连接
    static void broadcast(const nlohmann::json& message);

    /// 发送消息到指定连接
    static void send(const std::string& connId, const nlohmann::json& message);

private:
    static std::unordered_map<std::string, drogon::WebSocketConnectionPtr> connections_;
    static std::unordered_map<drogon::WebSocketConnection*, std::string> connectionIds_;  // wsConnPtr → id
    static std::mutex mutex_;
    static std::atomic<uint64_t> connectionCounter_;

    /// 处理 RPC 调用
    void handleRpcCall(const drogon::WebSocketConnectionPtr& wsConnPtr,
                      const RpcRequest& req);

    /// 处理订阅事件
    void handleSubscribe(const drogon::WebSocketConnectionPtr& wsConnPtr,
                       const nlohmann::json& params);

    /// 生成连接 ID
    static std::string generateConnectionId();

    /// 获取连接 ID
    static std::string getConnectionId(const drogon::WebSocketConnectionPtr& wsConnPtr);
};

} // namespace wingman::runtime::controllers
