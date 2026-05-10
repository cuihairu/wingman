#pragma once

#include "wingman/server/protocol.hpp"
#include <mutex>
#include <unordered_map>
#include <functional>
#include <memory>

namespace wingman::server {

// 前向声明
class Connection;

// 连接事件回调
using ConnectCallback = std::function<void(const std::string& agentId, const AgentInfo& info)>;
using DisconnectCallback = std::function<void(const std::string& agentId)>;

// Client 管理器 - 负责服务端客户端会话管理
class AgentManager {
public:
    AgentManager() = default;
    ~AgentManager() = default;

    // 禁止拷贝
    AgentManager(const AgentManager&) = delete;
    AgentManager& operator=(const AgentManager&) = delete;

    // ========== 注册/注销 ==========

    // 注册新客户端
    bool registerAgent(const std::string& agentId, const AgentInfo& info);

    // 注销客户端
    bool unregisterAgent(const std::string& agentId);

    // 更新客户端心跳
    bool updateHeartbeat(const std::string& agentId, const nlohmann::json& currentTask = nullptr);

    // ========== 查询 ==========

    // 检查客户端是否存在
    bool hasAgent(const std::string& agentId) const;

    // 获取客户端信息
    std::optional<AgentInfo> getAgent(const std::string& agentId) const;

    // 获取所有在线客户端
    std::vector<AgentInfo> getOnlineAgents() const;

    // 获取在线客户端数量
    size_t getOnlineCount() const;

    // ========== 连接关联 ==========

    // 关联 Connection 指针
    void setConnection(const std::string& agentId, std::shared_ptr<Connection> connection);

    // 获取客户端的 Connection
    std::shared_ptr<Connection> getConnection(const std::string& agentId) const;

    // 移除 Connection
    void removeConnection(const std::string& agentId);

    // ========== 超时检测 ==========

    // 检查并移除超时客户端
    std::vector<std::string> checkTimeout(std::chrono::seconds timeout);

    // ========== 事件回调 ==========

    void setConnectCallback(ConnectCallback callback) { connectCallback_ = std::move(callback); }
    void setDisconnectCallback(DisconnectCallback callback) { disconnectCallback_ = std::move(callback); }

private:
    mutable std::mutex mutex_;

    // agentId -> AgentInfo
    std::unordered_map<std::string, AgentInfo> agents_;

    // agentId -> Connection
    std::unordered_map<std::string, std::shared_ptr<Connection>> connections_;

    // 事件回调
    ConnectCallback connectCallback_;
    DisconnectCallback disconnectCallback_;
};

} // namespace wingman::server
