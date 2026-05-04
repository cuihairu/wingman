#include "wingman/server/orchestrator.hpp"
#include <spdlog/spdlog.h>
#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace wingman::server {

Orchestrator::Orchestrator() : kv_(std::make_unique<KeyValueStore>()) {
    spdlog::info("Orchestrator initialized");
}

Orchestrator::~Orchestrator() {
    spdlog::info("Orchestrator destroyed");
}

std::string Orchestrator::generateId(const std::string& prefix) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << prefix << ms << dis(gen);
    return oss.str();
}

std::string Orchestrator::registerClient() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string clientId = generateId("client_");

    // 保存到 KV
    kv_->hset("client:" + clientId, "status", "idle");
    kv_->hset("client:" + clientId, "registeredAt", std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count()));

    spdlog::info("Client registered: {}", clientId);
    return clientId;
}

bool Orchestrator::heartbeat(const std::string& clientId, const std::string& status,
                             const std::string& gameId, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!kv_->exists("client:" + clientId)) {
        return false;
    }

    int64_t now = std::chrono::system_clock::now().time_since_epoch().count();

    kv_->hset("client:" + clientId, "status", status);
    kv_->hset("client:" + clientId, "lastHeartbeat", std::to_string(now));
    kv_->hset("client:" + clientId, "gameId", gameId);
    kv_->hset("client:" + clientId, "username", username);

    // 更新游戏名到 Client 的映射（用于判断是不是自己人）
    if (!username.empty()) {
        kv_->set("teammate:" + username, clientId);
    }

    return true;
}

ClientInfo Orchestrator::getClientInfo(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(mutex_);

    ClientInfo info;
    info.clientId = clientId;

    if (!kv_->exists("client:" + clientId)) {
        return info;
    }

    auto fields = kv_->hgetall("client:" + clientId);
    info.status = fields["status"];
    info.gameId = fields["gameId"];
    info.username = fields["username"];

    if (!fields["lastHeartbeat"].empty()) {
        info.lastHeartbeat = std::stoll(fields["lastHeartbeat"]);
    }

    info.teamId = fields["teamId"];

    return info;
}

void Orchestrator::removeClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto info = getClientInfo(clientId);

    // 如果在队伍中，离开队伍
    if (!info.teamId.empty()) {
        leaveTeam(clientId);
    }

    // 清理用户名映射
    if (!info.username.empty()) {
        kv_->del("teammate:" + info.username);
    }

    // 删除 client 记录
    kv_->del("client:" + clientId);

    spdlog::info("Client removed: {}", clientId);
}

TeamAllocationResult Orchestrator::requestTeamAllocation(const TeamAllocationRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);

    TeamAllocationResult result;

    // 验证 Client
    if (!kv_->exists("client:" + request.clientId)) {
        result.message = "Client not registered";
        return result;
    }

    auto clientInfo = getClientInfo(request.clientId);

    // 如果已经在队伍中，返回队伍信息
    if (!clientInfo.teamId.empty()) {
        result.success = true;
        result.teamId = clientInfo.teamId;
        result.isLeader = kv_->hexists("team:" + clientInfo.teamId, "leader") &&
                         kv_->hget("team:" + clientInfo.teamId, "leader") == request.clientId;

        auto members = getTeamMembers(clientInfo.teamId);
        for (const auto& memberId : members) {
            auto info = getClientInfo(memberId);
            if (!info.username.empty() && memberId != request.clientId) {
                result.teammates.push_back(info.username);
            }
        }

        result.message = "Already in team";
        return result;
    }

    // 尝试自动组队
    tryFormTeams(request.preferredSize > 0 ? request.preferredSize : 3,
                 request.preferredSize > 0 ? request.preferredSize : 5);

    // 检查是否已被分配到队伍
    clientInfo = getClientInfo(request.clientId);
    if (!clientInfo.teamId.empty()) {
        result.success = true;
        result.teamId = clientInfo.teamId;
        result.isLeader = false;

        auto members = getTeamMembers(clientInfo.teamId);
        for (const auto& memberId : members) {
            auto info = getClientInfo(memberId);
            if (!info.username.empty()) {
                result.teammates.push_back(info.username);
            }
        }

        result.message = "Joined existing team";
        return result;
    }

    // 没有可用的队伍，创建新队伍
    std::string teamId = createTeam(request.clientId, request.preferredSize > 0 ? request.preferredSize : 5);

    result.success = true;
    result.teamId = teamId;
    result.isLeader = true;
    result.teammates.push_back(clientInfo.username);
    result.message = "Created new team";

    spdlog::info("Team allocated: {} for client {}", teamId, request.clientId);
    return result;
}

std::string Orchestrator::createTeam(const std::string& leaderId, int maxSize) {
    std::string teamId = generateId("team_");

    int64_t now = std::chrono::system_clock::now().time_since_epoch().count();

    // 保存队伍信息
    kv_->hset("team:" + teamId, "leader", leaderId);
    kv_->hset("team:" + teamId, "state", "waiting");
    kv_->hset("team:" + teamId, "maxSize", std::to_string(maxSize));
    kv_->hset("team:" + teamId, "createdAt", std::to_string(now));

    // 更新 Client 的 teamId
    kv_->hset("client:" + leaderId, "teamId", teamId);

    // 初始化成员列表
    kv_->lpush("team:" + teamId + ":members", leaderId);

    // 将队伍添加到等待队列
    kv_->lpush("queue:teams", teamId);

    spdlog::info("Team created: {} by leader {}", teamId, leaderId);
    return teamId;
}

bool Orchestrator::joinTeam(const std::string& clientId, const std::string& teamId) {
    // 检查队伍是否存在
    if (!kv_->exists("team:" + teamId)) {
        return false;
    }

    // 检查队伍是否已满
    std::string maxSizeStr = kv_->hget("team:" + teamId, "maxSize");
    int maxSize = maxSizeStr.empty() ? 5 : std::stoi(maxSizeStr);

    size_t currentSize = kv_->llen("team:" + teamId + ":members");
    if (currentSize >= static_cast<size_t>(maxSize)) {
        return false;
    }

    // 检查 Client 是否已在其他队伍
    auto clientInfo = getClientInfo(clientId);
    if (!clientInfo.teamId.empty()) {
        return false;
    }

    // 添加到队伍
    kv_->lpush("team:" + teamId + ":members", clientId);
    kv_->hset("client:" + clientId, "teamId", teamId);

    spdlog::info("Client {} joined team {}", clientId, teamId);
    return true;
}

bool Orchestrator::leaveTeam(const std::string& clientId) {
    auto clientInfo = getClientInfo(clientId);

    if (clientInfo.teamId.empty()) {
        return false;
    }

    std::string teamId = clientInfo.teamId;

    // 检查是否是队长
    std::string leader = kv_->hget("team:" + teamId, "leader");

    // 从队伍成员列表中移除
    std::vector<std::string> members = getTeamMembers(teamId);
    for (const auto& memberId : members) {
        if (memberId == clientId) {
            kv_->lrem("team:" + teamId + ":members", 1, clientId);
            break;
        }
    }

    // 更新 Client
    kv_->hset("client:" + clientId, "teamId", "");

    // 如果是队长离开，解散队伍或转移队长
    if (leader == clientId) {
        members = getTeamMembers(teamId);
        if (members.empty()) {
            disbandTeam(teamId);
        } else {
            // 转移队长给第一个成员
            kv_->hset("team:" + teamId, "leader", members[0]);
            kv_->hset("client:" + members[0], "teamId", teamId);
        }
    }

    spdlog::info("Client {} left team {}", clientId, teamId);
    return true;
}

protocol::TeamInfo Orchestrator::getTeamInfo(const std::string& teamId) {
    protocol::TeamInfo info;

    if (!kv_->exists("team:" + teamId)) {
        return info;
    }

    auto fields = kv_->hgetall("team:" + teamId);

    if (!fields["teamId"].empty()) {
        info.teamId = std::stoull(fields["teamId"]);
    }
    if (!fields["leaderId"].empty()) {
        info.leaderId = std::stoull(fields["leaderId"]);
    }

    info.teamName = fields["teamName"];
    info.maxSize = fields["maxSize"].empty() ? 5 : std::stoi(fields["maxSize"]);
    info.minStart = fields["minStart"].empty() ? 2 : std::stoi(fields["minStart"]);

    std::string stateStr = fields["state"];
    if (stateStr == "waiting") info.state = protocol::TeamState::Waiting;
    else if (stateStr == "matching") info.state = protocol::TeamState::Matching;
    else if (stateStr == "ready") info.state = protocol::TeamState::Ready;
    else if (stateStr == "ingame") info.state = protocol::TeamState::InGame;
    else info.state = protocol::TeamState::Disbanded;

    if (!fields["createTime"].empty()) {
        info.createTime = std::stoull(fields["createTime"]);
    }

    // 获取成员列表
    auto members = getTeamMembers(teamId);
    for (size_t i = 0; i < members.size(); ++i) {
        protocol::TeamMember member;
        member.playerId = std::stoull(members[i]);

        auto clientInfo = getClientInfo(members[i]);
        member.nickname = clientInfo.username;

        if (members[i] == kv_->hget("team:" + teamId, "leader")) {
            member.role = protocol::TeamRole::Leader;
        } else {
            member.role = protocol::TeamRole::Member;
        }

        info.members.push_back(member);
    }

    return info;
}

void Orchestrator::disbandTeam(const std::string& teamId) {
    // 移除所有成员的 teamId
    auto members = getTeamMembers(teamId);
    for (const auto& memberId : members) {
        kv_->hset("client:" + memberId, "teamId", "");
    }

    // 删除队伍数据
    kv_->del("team:" + teamId);
    kv_->del("team:" + teamId + ":members");

    // 从队列中移除
    kv_->lrem("queue:teams", 1, teamId);

    spdlog::info("Team disbanded: {}", teamId);
}

std::string Orchestrator::reportVoteEvent(const VoteEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string voteId = generateId("vote_");

    int64_t now = std::chrono::system_clock::now().time_since_epoch().count();

    // 保存投票信息
    kv_->hset("vote:" + voteId, "teamId", event.teamId);
    kv_->hset("vote:" + voteId, "type", event.type);
    kv_->hset("vote:" + voteId, "target", event.target);
    kv_->hset("vote:" + voteId, "initiator", event.initiator);
    kv_->hset("vote:" + voteId, "createTime", std::to_string(now));
    kv_->hset("vote:" + voteId, "state", "active");

    // 检查是否是我们自己人发起的，被投的是不是路人
    auto initiatorClientId = kv_->get("teammate:" + event.initiator);
    auto targetClientId = kv_->get("teammate:" + event.target);

    bool isOurInitiator = !initiatorClientId.empty();
    bool isOurTarget = !targetClientId.empty();

    // 决定是否同意投票
    // 如果是自己人发起且被投的是路人 -> 同意
    // 如果是路人发起且被投的是自己人 -> 不同意
    bool shouldAgree = isOurInitiator && !isOurTarget;

    if (shouldAgree) {
        kv_->hset("vote:" + voteId, "recommendAction", "agree");
    } else {
        kv_->hset("vote:" + voteId, "recommendAction", "disagree");
    }

    spdlog::info("Vote reported: {} type={}, target={}, initiator={}, recommend={}",
                 voteId, event.type, event.target, event.initiator,
                 shouldAgree ? "agree" : "disagree");

    return voteId;
}

bool Orchestrator::shouldAgreeToVote(const VoteEvent& event, const ClientInfo& client) {
    auto initiatorClientId = kv_->get("teammate:" + event.initiator);
    auto targetClientId = kv_->get("teammate:" + event.target);

    // 如果是自己人发起且被投的是路人 -> 同意
    // 如果是路人发起且被投的是自己人 -> 不同意
    bool isOurInitiator = !initiatorClientId.empty();
    bool isOurTarget = !targetClientId.empty();

    return isOurInitiator && !isOurTarget;
}

std::vector<VoteAction> Orchestrator::getPendingActions(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<VoteAction> actions;

    auto clientInfo = getClientInfo(clientId);
    if (clientInfo.teamId.empty()) {
        return actions;
    }

    // 查找该队伍的活跃投票
    std::vector<std::string> voteKeys = kv_->keys("vote:*");
    for (const auto& key : voteKeys) {
        auto fields = kv_->hgetall(key);

        if (fields["teamId"] == clientInfo.teamId &&
            fields["state"] == "active") {

            // 检查是否已处理过此投票
            std::string voteId = key.substr(5); // 移除 "vote:" 前缀
            if (kv_->hexists("vote:" + voteId + ":responses", clientId)) {
                continue;
            }

            VoteAction action;
            action.voteId = voteId;
            action.clientId = clientId;

            std::string recommend = fields["recommendAction"];
            action.agree = (recommend == "agree");
            action.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

            actions.push_back(action);
        }
    }

    return actions;
}

bool Orchestrator::handleVoteAction(const VoteAction& action) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查投票是否存在
    if (!kv_->exists("vote:" + action.voteId)) {
        return false;
    }

    // 记录响应
    kv_->hset("vote:" + action.voteId + ":responses", action.clientId,
              action.agree ? "agree" : "disagree");

    spdlog::info("Vote action: voteId={}, clientId={}, agree={}",
                 action.voteId, action.clientId, action.agree);

    return true;
}

std::vector<std::string> Orchestrator::getTeamMembers(const std::string& teamId) {
    std::vector<std::string> members;

    auto values = kv_->lrange("team:" + teamId + ":members", 0, -1);
    for (const auto& value : values) {
        members.push_back(value);
    }

    return members;
}

size_t Orchestrator::cleanupStaleClients(int64_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    int64_t now = std::chrono::system_clock::now().time_since_epoch().count();

    // 获取所有 client keys
    std::vector<std::string> keys = kv_->keys("client:*");

    for (const auto& key : keys) {
        auto fields = kv_->hgetall(key);

        if (!fields["lastHeartbeat"].empty()) {
            int64_t lastHeartbeat = std::stoll(fields["lastHeartbeat"]);

            if (now - lastHeartbeat > timeoutMs * 1000000) { // 转换为纳秒
                // 提取 clientId
                std::string clientId = key.substr(8); // 移除 "client:" 前缀
                removeClient(clientId);
                count++;
            }
        }
    }

    if (count > 0) {
        spdlog::info("Cleaned up {} stale clients", count);
    }

    return count;
}

size_t Orchestrator::tryFormTeams(int minTeamSize, int maxTeamSize) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t formedCount = 0;

    // 获取等待中的队伍
    auto teamIds = kv_->lrange("queue:teams", 0, -1);

    // 获取所有空闲且已登录的玩家
    std::vector<std::string> idleClients;
    auto clientKeys = kv_->keys("client:*");

    for (const auto& key : clientKeys) {
        auto fields = kv_->hgetall(key);
        std::string clientId = key.substr(8);

        if (fields["status"] == "logged_in" && fields["teamId"].empty()) {
            idleClients.push_back(clientId);
        }
    }

    if (idleClients.size() < static_cast<size_t>(minTeamSize)) {
        return 0; // 没有足够的玩家
    }

    // 尝试将空闲玩家加入现有的不满队伍
    for (const auto& clientId : idleClients) {
        bool joined = false;

        for (const auto& teamId : teamIds) {
            std::string maxSizeStr = kv_->hget("team:" + teamId, "maxSize");
            int maxSize = maxSizeStr.empty() ? maxTeamSize : std::stoi(maxSizeStr);

            size_t currentSize = kv_->llen("team:" + teamId + ":members");

            if (currentSize < static_cast<size_t>(maxSize)) {
                joinTeam(clientId, teamId);
                joined = true;
                formedCount++;
                break;
            }
        }

        if (!joined && idleClients.size() >= static_cast<size_t>(minTeamSize)) {
            // 创建新队伍
            std::string teamId = createTeam(clientId, maxTeamSize);

            // 添加其他玩家直到队伍满或没有更多玩家
            size_t teamSize = 1;
            for (size_t i = 1; i < idleClients.size() && teamSize < static_cast<size_t>(maxTeamSize); ++i) {
                if (idleClients[i] != clientId) {
                    joinTeam(idleClients[i], teamId);
                    teamSize++;
                }
            }

            formedCount++;
            break;
        }
    }

    if (formedCount > 0) {
        spdlog::info("Formed {} teams", formedCount);
    }

    return formedCount;
}

} // namespace wingman::server
