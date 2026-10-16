> [中文](team.md) | **English**

# Team

This document is lengthy (931 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [team.md](team.md) for the original.

- 模块概述
  - 架构设计
- 加入队伍
  - joinTeam(teamId, memberId?)
- 离开队伍
  - leaveTeam()
- 投票功能
  - createVote(subject, timeout?)
  - castVote(voteId, response)
  - getVoteResult(voteId)
- 状态管理
  - reportStatus(status)
  - getTeamStatus()
- 群体通信
  - broadcast(message)
- 事件订阅
  - on(event, callback)
- 查询接口
  - isJoined()
  - getMemberId()
- 完整示例
  - 队伍协同
  - 投票流程
- 可用接口
- 协议消息格式
  - Runtime [Server]
  - Server [Runtime]
- 注意事项
