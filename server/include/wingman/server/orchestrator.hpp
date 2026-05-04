#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>
#include "wingman/kvstore.hpp"
#include "wingman/server/protocol/team.hpp"

namespace wingman::server {

// Client 信息
struct ClientInfo {
    std::string clientId;
    std::string status;       // idle, logged_in, in_team, disconnected
    std::string gameId;       // 游戏内账号ID
    std::string username;     // 游戏内用户名
    int64_t lastHeartbeat;
    std::string teamId;       // 当前队伍ID，空表示无队伍

    ClientInfo() : lastHeartbeat(0) {}
};

// 队伍分配请求
struct TeamAllocationRequest {
    std::string clientId;
    std::string username;
    int preferredSize;        // 期望队伍大小
    int64_t timeout;          // 超时时间（毫秒）
};

// 队伍分配结果
struct TeamAllocationResult {
    bool success;
    std::string teamId;
    std::string message;
    bool isLeader;            // 是否是队长
    std::vector<std::string> teammates;  // 队友用户名列表

    TeamAllocationResult() : success(false), isLeader(false) {}
};

// 投票事件
struct VoteEvent {
    std::string voteId;
    std::string teamId;
    std::string type;         // kick, surrender, change_leader
    std::string target;       // 被投票目标（用户名）
    std::string initiator;    // 发起人（用户名）
    int64_t timestamp;

    VoteEvent() : timestamp(0) {}
};

// 投票动作
struct VoteAction {
    std::string voteId;
    std::string clientId;
    bool agree;               // true 同意, false 不同意
    int64_t timestamp;

    VoteAction() : agree(false), timestamp(0) {}
};

// 编排引擎 - 负责组队逻辑协调
class Orchestrator {
public:
    Orchestrator();
    ~Orchestrator();

    // ========== Client 管理 ==========

    // 注册新 Client
    std::string registerClient();

    // Client 心跳
    bool heartbeat(const std::string& clientId, const std::string& status,
                   const std::string& gameId, const std::string& username);

    // 获取 Client 信息
    ClientInfo getClientInfo(const std::string& clientId);

    // 移除 Client
    void removeClient(const std::string& clientId);

    // ========== 队伍编排 ==========

    // 请求组队分配
    TeamAllocationResult requestTeamAllocation(const TeamAllocationRequest& request);

    // 创建队伍
    std::string createTeam(const std::string& leaderId, int maxSize);

    // 加入队伍
    bool joinTeam(const std::string& clientId, const std::string& teamId);

    // 离开队伍
    bool leaveTeam(const std::string& clientId);

    // 获取队伍信息
    protocol::TeamInfo getTeamInfo(const std::string& teamId);

    // 解散队伍
    void disbandTeam(const std::string& teamId);

    // ========== 投票协调 ==========

    // 汇报投票事件
    std::string reportVoteEvent(const VoteEvent& event);

    // 处理投票动作
    bool handleVoteAction(const VoteAction& action);

    // 获取待处理的投票动作
    std::vector<VoteAction> getPendingActions(const std::string& clientId);

    // ========== 维护 ==========

    // 清理超时 Client
    size_t cleanupStaleClients(int64_t timeoutMs);

    // 尝试自动组队（将等待中的玩家组成队伍）
    size_t tryFormTeams(int minTeamSize = 2, int maxTeamSize = 5);

private:
    std::mutex mutex_;
    std::unique_ptr<KeyValueStore> kv_;

    // 生成唯一 ID
    std::string generateId(const std::string& prefix);

    // 检查是否应该同意投票
    bool shouldAgreeToVote(const VoteEvent& event, const ClientInfo& client);

    // 获取队伍成员列表
    std::vector<std::string> getTeamMembers(const std::string& teamId);
};

} // namespace wingman::server
