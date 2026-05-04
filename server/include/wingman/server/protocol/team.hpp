#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace wingman::server::protocol {

// 玩家信息
struct PlayerInfo {
    uint64_t playerId;
    std::string nickname;
    int level;
    int avatarId;
    bool isOnline;
    bool isReady;       // 准备状态
    uint64_t teamId;    // 所属队伍ID，0表示无队伍

    PlayerInfo() : playerId(0), level(0), avatarId(0),
                   isOnline(false), isReady(false), teamId(0) {}
};

// 队伍状态
enum class TeamState {
    Waiting,       // 等待中
    Matching,      // 匹配中
    Ready,         // 准备阶段
    InGame,        // 游戏中
    Disbanded,     // 已解散
};

// 队伍成员角色
enum class TeamRole {
    Leader,     // 队长
    Member,     // 普通队员
    Invited,    // 已邀请待确认
};

// 队伍成员信息
struct TeamMember {
    uint64_t playerId;
    std::string nickname;
    TeamRole role;
    int heroId;        // 选择的英雄
    bool isReady;
    uint64_t joinTime;
    int ping;          // 延迟

    TeamMember() : playerId(0), role(TeamRole::Member),
                   heroId(0), isReady(false),
                   joinTime(0), ping(0) {}
};

// 队伍信息
struct TeamInfo {
    uint64_t teamId;
    uint64_t leaderId;
    std::string teamName;
    int maxSize;               // 最大人数，通常 2-5
    int minStart;              // 最少开战人数
    TeamState state;
    std::vector<TeamMember> members;
    uint64_t createTime;
    uint64_t lastActivity;

    // 匹配相关
    int targetRank;
    std::string gameMode;      // 排位/普通/自定义

    TeamInfo() : teamId(0), leaderId(0), maxSize(5), minStart(2),
                 state(TeamState::Waiting), createTime(0), lastActivity(0),
                 targetRank(0) {}
};

// 投票类型
enum class VoteType {
    KickPlayer,    // 踢人
    Surrender,     // 投降
    ChangeLeader,  // 转让队长
    Pause,         // 暂停游戏
};

// 投票选项
struct VoteOption {
    uint64_t voterId;
    bool agreed;        // true 同意, false 不同意
    uint64_t voteTime;
};

// 投票信息
struct VoteInfo {
    uint64_t voteId;
    uint64_t teamId;
    uint64_t initiatorId;    // 发起人
    VoteType type;
    uint64_t targetId;       // 被投票玩家ID（踢人时使用）
    std::string reason;      // 原因
    std::vector<VoteOption> options;
    int requiredVotes;       // 需要的票数
    int agreeCount;          // 同意票数
    uint64_t createTime;
    uint64_t expireTime;     // 过期时间
    bool isActive;

    VoteInfo() : voteId(0), teamId(0), initiatorId(0),
                 type(VoteType::KickPlayer), targetId(0),
                 requiredVotes(0), agreeCount(0),
                 createTime(0), expireTime(0), isActive(false) {}
};

// 语音频道信息
struct VoiceChannel {
    uint64_t channelId;
    uint64_t teamId;
    std::string serverUrl;    // WebRTC/语音服务器地址
    std::string token;        // 认证令牌
    uint64_t expireTime;
    std::vector<uint64_t> participantIds;

    VoiceChannel() : channelId(0), teamId(0), expireTime(0) {}
};

// 协议消息类型
enum class TeamMessageType : uint16_t {
    // 队伍管理
    CreateTeam = 0x1001,
    LeaveTeam = 0x1002,
    InvitePlayer = 0x1003,
    AcceptInvite = 0x1004,
    DeclineInvite = 0x1005,
    KickPlayer = 0x1006,
    TransferLeader = 0x1007,

    // 队伍状态
    TeamInfoNotify = 0x1100,
    MemberJoined = 0x1101,
    MemberLeft = 0x1102,
    MemberReady = 0x1103,
    TeamDisbanded = 0x1104,

    // 投票系统
    StartVote = 0x2000,
    VoteNotify = 0x2001,
    CastVote = 0x2002,
    VoteResult = 0x2003,
    VoteCancelled = 0x2004,

    // 语音
    VoiceChannelJoin = 0x3000,
    VoiceChannelLeave = 0x3001,
    VoiceChannelNotify = 0x3002,
    VoiceSpeaking = 0x3003,
    VoiceMute = 0x3004,
};

// 消息基类
struct TeamMessage {
    TeamMessageType type;
    uint64_t playerId;
    uint64_t teamId;
    uint32_t timestamp;
    std::string data;  // JSON 序列化的具体数据

    TeamMessage() : type(static_cast<TeamMessageType>(0)),
                    playerId(0), teamId(0), timestamp(0) {}
};

} // namespace wingman::server::protocol
