#include "wingman/server/agent_manager.hpp"
#include "wingman/server/server.hpp"  // For Connection forward declaration
#include <spdlog/spdlog.h>
#include <algorithm>

namespace wingman::server {

// ========== 注册/注销 ==========

bool AgentManager::registerAgent(const std::string& agentId, const AgentInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);

    bool isNew = agents_.find(agentId) == agents_.end();
    agents_[agentId] = info;

    // 触发回调
    if (isNew && connectCallback_) {
        connectCallback_(agentId, info);
    }

    spdlog::info("Agent registered: {} ({})", agentId, info.hostname);
    return true;
}

bool AgentManager::unregisterAgent(const std::string& agentId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = agents_.find(agentId);
    if (it == agents_.end()) {
        return false;
    }

    spdlog::info("Agent unregistered: {}", agentId);

    // 触发回调
    if (disconnectCallback_) {
        disconnectCallback_(agentId);
    }

    agents_.erase(it);
    connections_.erase(agentId);
    return true;
}

bool AgentManager::updateHeartbeat(const std::string& agentId, const nlohmann::json& currentTask) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = agents_.find(agentId);
    if (it == agents_.end()) {
        return false;
    }

    it->second.lastSeen = std::chrono::system_clock::now();
    if (!currentTask.is_null()) {
        it->second.currentTask = currentTask;
    }

    return true;
}

// ========== 查询 ==========

bool AgentManager::hasAgent(const std::string& agentId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return agents_.find(agentId) != agents_.end();
}

std::optional<AgentInfo> AgentManager::getAgent(const std::string& agentId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = agents_.find(agentId);
    if (it != agents_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<AgentInfo> AgentManager::getOnlineAgents() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AgentInfo> result;
    result.reserve(agents_.size());

    for (const auto& [id, info] : agents_) {
        result.push_back(info);
    }

    return result;
}

size_t AgentManager::getOnlineCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return agents_.size();
}

// ========== 连接关联 ==========

void AgentManager::setConnection(const std::string& agentId, std::shared_ptr<Connection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_[agentId] = connection;
}

std::shared_ptr<Connection> AgentManager::getConnection(const std::string& agentId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = connections_.find(agentId);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

void AgentManager::removeConnection(const std::string& agentId) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(agentId);
}

// ========== 超时检测 ==========

std::vector<std::string> AgentManager::checkTimeout(std::chrono::seconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> timeoutAgents;
    auto now = std::chrono::system_clock::now();

    for (auto it = agents_.begin(); it != agents_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastSeen
        );

        if (elapsed > timeout) {
            spdlog::warn("Agent timeout: {} (last seen {} seconds ago)",
                it->first, elapsed.count());

            timeoutAgents.push_back(it->first);

            // 触发回调
            if (disconnectCallback_) {
                disconnectCallback_(it->first);
            }

            it = agents_.erase(it);
            connections_.erase(it->first);
        } else {
            ++it;
        }
    }

    return timeoutAgents;
}

} // namespace wingman::server
