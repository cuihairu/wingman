# API: wingman.team

Team 模块提供多 Runtime 协同组队、投票协调和群体通信功能。

## 模块概述

team 模块基于 inbox 消息队列实现分布式协同功能：

- **组队管理**：加入/离开队伍，获取队友信息
- **投票协调**：发起投票、参与投票、获取投票结果
- **状态汇报**：上报自身状态，获取队友状态
- **群体通信**：向所有队友广播消息
- **事件订阅**：监听队伍相关事件

### 架构设计

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           Runtime A                                       │
│                                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                      Team Module                                 │  │
│  │                                                                   │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────────┐  │  │
│  │  │joinTeam  │  │leaveTeam │  │broadcast │  │ on(event, cb)    │  │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────────────────┘  │  │
│  │                                                                   │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────────────────┐   │  │
│  │  │createVote│  │castVote  │      │ Local State (members, votes)│   │  │
│  │  └──────────┘  └──────────┘      └──────────────────────────────┘   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                  ↑                                       │
│                                  │ EventHub                              │
│                                  │ team.outbound                         │
│                                  ↓                                       │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                      Inbox Module                                 │  │
│  │                     (consume/ack/report)                          │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                  ↑                                       │
│                                  │ TCP                                   │
│                                  ↓                                       │
└─────────────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────────────┐
│                              Go Server                                   │
│                                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                         TeamManager                                │  │
│  │                                                                   │  │
│  │  ┌────────────────────────────────────────────────────────────┐  │  │
│  │  │  Teams: teamId → {leaderId, members[], state}             │  │  │
│  │  └────────────────────────────────────────────────────────────┘  │  │
│  │                                                                   │  │
│  │  ┌────────────────────────────────────────────────────────────┐  │  │
│  │  │  Votes: voteId → {teamId, subject, responses{}, deadline}  │  │  │
│  │  └────────────────────────────────────────────────────────────┘  │  │
│  │                                                                   │  │
│  │  ┌────────────────────────────────────────────────────────────┐  │  │
│  │  │  Inboxes: agentId → {msgId → {type, payload, acked}}       │  │  │
│  │  └────────────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
                                   ▲
                                   │ TCP
                                   │
┌─────────────────────────────────────────────────────────────────────────┐
│                           Runtime B                                       │
│                           (相同架构)                                       │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 加入队伍

### joinTeam(teamId, memberId?)

**说明**：加入指定队伍。

**函数签名**：

```python
join_team(team_id: str, member_id: str = None) -> bool
```

```lua
joinTeam(teamId: str, memberId: str = nil) -> boolean
```

**参数**：
- `team_id` / `teamId` - 队伍 ID
- `member_id` / `memberId` - 可选，成员 ID（默认自动生成）

**返回**：是否加入成功

:::tabs

== Python

```python:line-numbers
from wingman import team

# 加入队伍
if team.join_team("team_001", "player_alice"):
    print("已加入队伍")
    
    # 获取队伍信息
    status = team.get_team_status()
    print(f"队长: {status['leaderId']}")
    print(f"队友: {status['members']}")
```

== Lua

```lua:line-numbers
local team = require("wingman.team")

-- 加入队伍
if team.joinTeam("team_001", "player_alice") then
    print("已加入队伍")
    
    -- 获取队伍信息
    local status = team.getTeamStatus()
    print("队长:", status.leaderId)
    print("队友:", unpack(status.members))
end
```

:::

---

## 离开队伍

### leaveTeam()

**说明**：离开当前队伍。

**函数签名**：

```python
leave_team() -> bool
```

```lua
leaveTeam() -> boolean
```

**返回**：是否离开成功

:::tabs

== Python

```python:line-numbers
from wingman import team

# 离开队伍
if team.leave_team():
    print("已离开队伍")
```

== Lua

```lua:line-numbers
local team = require("wingman.team")

-- 离开队伍
if team.leaveTeam() then
    print("已离开队伍")
end
```

:::

---

## 投票功能

### createVote(subject, timeout?)

**说明**：发起投票。

**函数签名**：

```python
create_vote(subject: str, timeout: int = 30000) -> bool
```

```lua
createVote(subject: str, timeout: number = 30000) -> boolean
```

**参数**：
- `subject` - 投票主题
- `timeout` - 超时时间（毫秒，默认 30000）

**返回**：是否发起成功

---

### castVote(voteId, response)

**说明**：对指定投票进行投票。

**函数签名**：

```python
cast_vote(vote_id: str, response: str) -> bool
```

```lua
castVote(voteId: str, response: str) -> boolean
```

**参数**：
- `vote_id` / `voteId` - 投票 ID
- `response` - 投票响应内容

**返回**：是否投票成功

---

### getVoteResult(voteId)

**说明**：获取投票结果。

**函数签名**：

```python
get_vote_result(vote_id: str) -> dict
```

```lua
getVoteResult(voteId: str) -> string
```

**参数**：
- `vote_id` / `voteId` - 投票 ID

**返回**：投票结果对象（JSON 字符串）

:::tabs

== Python

```python:line-numbers
from wingman import team, json

# 发起投票
if team.create_vote("是否开始副本", 30000):
    print("投票已发起")

# 获取投票结果
result_str = team.get_vote_result("vote_123")
result = json.decode(result_str)
print(f"投票主题: {result['subject']}")
print(f"是否活跃: {result['active']}")
print(f"投票响应: {result['responses']}")
```

== Lua

```lua:line-numbers
local team = require("wingman.team")
local json = require("wingman.json")

-- 发起投票
if team.createVote("是否开始副本", 30000) then
    print("投票已发起")
end

-- 获取投票结果
local resultStr = team.getVoteResult("vote_123")
local result = json.decode(resultStr)
print("投票主题:", result.subject)
print("是否活跃:", tostring(result.active))
print("投票响应:", json.encode(result.responses))
```

:::

---

## 状态管理

### reportStatus(status)

**说明**：上报自身状态到队伍。

**函数签名**：

```python
report_status(status: dict) -> bool
```

```lua
reportStatus(status: table) -> boolean
```

**参数**：
- `status` - 状态对象

**返回**：是否上报成功

:::tabs

== Python

```python:line-numbers
from wingman import team

# 汇报状态
team.report_status({
    "hp": 100,
    "mp": 50,
    "location": "town",
    "ready": True
})
```

== Lua

```lua:line-numbers
local team = require("wingman.team")

-- 汇报状态
team.reportStatus({
    hp = 100,
    mp = 50,
    location = "town",
    ready = true
})
```

:::

---

### getTeamStatus()

**说明**：获取当前队伍状态。

**函数签名**：

```python
get_team_status() -> dict
```

```lua
getTeamStatus() -> string
```

**返回**：队伍状态对象（JSON 字符串）
- `teamId` - 队伍 ID
- `leaderId` - 队长 ID
- `members` - 成员列表
- `state` - 队伍状态（idle/voting/working）
- `lastUpdate` - 最后更新时间

:::tabs

== Python

```python:line-numbers
from wingman import team, json

status_str = team.get_team_status()
status = json.decode(status_str)

print(f"队伍 ID: {status['teamId']}")
print(f"队长: {status['leaderId']}")
print(f"成员: {status['members']}")
print(f"状态: {status['state']}")
```

== Lua

```lua:line-numbers
local team = require("wingman.team")
local json = require("wingman.json")

local statusStr = team.getTeamStatus()
local status = json.decode(statusStr)

print("队伍 ID:", status.teamId)
print("队长:", status.leaderId)
print("成员:", unpack(status.members))
print("状态:", status.state)
```

:::

---

## 群体通信

### broadcast(message)

**说明**：向所有队友广播消息。

**函数签名**：

```python
broadcast(message: dict | str) -> bool
```

```lua
broadcast(message: table | string) -> boolean
```

**参数**：
- `message` - 消息内容（对象或字符串）

**返回**：是否发送成功

:::tabs

== Python

```python:line-numbers
from wingman import team

# 广播消息
team.broadcast({
    "type": "coordination",
    "action": "move_to",
    "target": "boss_location",
    "timestamp": 1234567890
})
```

== Lua

```lua:line-numbers
local team = require("wingman.team")

-- 广播消息
team.broadcast({
    type = "coordination",
    action = "move_to",
    target = "boss_location",
    timestamp = 1234567890
})
```

:::

---

## 事件订阅

### on(event, callback)

**说明**：订阅队伍事件。

**函数签名**：

```python
on(event: str, callback: Callable) -> bool
```

```lua
on(event: str, callback: function) -> boolean
```

**参数**：
- `event` - 事件类型（无需 "team." 前缀）
- `callback` - 回调函数

**可用事件**：
- `vote_started` - 投票开始
- `vote_ended` - 投票结束
- `broadcast_received` - 收到广播消息
- `member_joined` - 队友加入
- `member_left` - 队友离开

:::tabs

== Python

```python:line-numbers
from wingman import team, json

# 监听投票开始
team.on("vote_started", lambda data: print(f"新投票: {json.decode(data)['subject']}"))

# 监听投票结束
team.on("vote_ended", lambda data: print(f"投票结束: {json.decode(data)['result']}"))

# 监听广播消息
team.on("broadcast_received", lambda data: print(f"收到广播: {json.decode(data)['message']}"))
```

== Lua

```lua:line-numbers
local team = require("wingman.team")
local json = require("wingman.json")

-- 监听投票开始
team.on("vote_started", function(data)
    local msg = json.decode(data)
    print("新投票:", msg.subject)
end)

-- 监听投票结束
team.on("vote_ended", function(data)
    local msg = json.decode(data)
    print("投票结束:", json.encode(msg.result))
end)

-- 监听广播消息
team.on("broadcast_received", function(data)
    local msg = json.decode(data)
    print("收到广播:", json.encode(msg.message))
end)
```

:::

---

## 查询接口

### isJoined()

**说明**：检查是否已加入队伍。

**函数签名**：

```python
is_joined() -> bool
```

```lua
isJoined() -> boolean
```

**返回**：是否已加入队伍

---

### getMemberId()

**说明**：获取当前成员 ID。

**函数签名**：

```python
get_member_id() -> str
```

```lua
getMemberId() -> string
```

**返回**：成员 ID

---

## 完整示例

### 队伍协同

:::tabs

== Python

```python:line-numbers
from wingman import team, json, util

# 1. 加入队伍
if not team.join_team("dungeon_party", "warrior_01"):
    print("加入队伍失败")
    exit()

print("已加入队伍")

# 2. 监听队伍事件
team.on("vote_started", lambda data: print(f"投票开始: {json.decode(data)['subject']}"))
team.on("broadcast_received", lambda data: handle_broadcast(json.decode(data)))

def handle_broadcast(msg):
    print(f"收到 {msg['senderId']} 的广播")
    if msg['message']['type'] == 'attack':
        coordinate_attack(msg['message'])

# 3. 汇报状态
team.report_status({
    "role": "warrior",
    "level": 50,
    "ready": True
})

# 4. 等待并处理消息
while team.is_joined():
    util.sleep(1000)
    
    # 检查投票
    status = json.decode(team.get_team_status())
    if status['state'] == 'voting':
        print("队伍正在投票...")
    
    # 定期汇报状态
    team.report_status({"hp": get_current_hp()})
```

== Lua

```lua:line-numbers
local team = require("wingman.team")
local json = require("wingman.json")
local util = require("wingman.util")

-- 1. 加入队伍
if not team.joinTeam("dungeon_party", "warrior_01") then
    print("加入队伍失败")
    return
end

print("已加入队伍")

-- 2. 监听队伍事件
team.on("vote_started", function(data)
    local msg = json.decode(data)
    print("投票开始:", msg.subject)
end)

team.on("broadcast_received", function(data)
    local msg = json.decode(data)
    print("收到", msg.senderId, "的广播")
    if msg.message.type == "attack" then
        coordinateAttack(msg.message)
    end
end)

-- 3. 汇报状态
team.reportStatus({
    role = "warrior",
    level = 50,
    ready = true
})

-- 4. 等待并处理消息
while team.isJoined() do
    util.sleep(1000)
    
    -- 检查投票
    local status = json.decode(team.getTeamStatus())
    if status.state == "voting" then
        print("队伍正在投票...")
    end
    
    -- 定期汇报状态
    team.reportStatus({hp = getCurrentHp()})
end
```

:::

---

### 投票流程

:::tabs

== Python

```python:line-numbers
from wingman import team, json, util

# 监听投票事件
team.on("vote_started", lambda data: handle_vote_start(json.decode(data)))
team.on("vote_ended", lambda data: print(f"投票结果: {json.decode(data)['result']}"))

def handle_vote_start(vote):
    print(f"投票: {vote['subject']}")
    print(f"截止时间: {vote['deadline']}")
    
    # 自动决策（示例）
    if vote['subject'] == "是否休息":
        team.cast_vote(vote['voteId'], "agree")
    else:
        team.cast_vote(vote['voteId'], "disagree")

# 发起投票
team.create_vote("是否前往BOSS区域", 30000)

# 等待投票结果
util.sleep(35000)

result = json.decode(team.get_vote_result("vote_xxx"))
print(f"最终结果: {result['responses']}")
```

== Lua

```lua:line-numbers
local team = require("wingman.team")
local json = require("wingman.json")
local util = require("wingman.util")

-- 监听投票事件
team.on("vote_started", function(data)
    local vote = json.decode(data)
    print("投票:", vote.subject)
    print("截止时间:", vote.deadline)
    
    -- 自动决策（示例）
    if vote.subject == "是否休息" then
        team.castVote(vote.voteId, "agree")
    else
        team.castVote(vote.voteId, "disagree")
    end
end)

team.on("vote_ended", function(data)
    local msg = json.decode(data)
    print("投票结果:", json.encode(msg.result))
end)

-- 发起投票
team.createVote("是否前往BOSS区域", 30000)

-- 等待投票结果
util.sleep(35000)

local result = json.decode(team.getVoteResult("vote_xxx"))
print("最终结果:", json.encode(result.responses))
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `join_team(teamId, memberId?)` | `joinTeam(teamId, memberId?)` | 加入队伍 | teamId: 队伍ID<br>memberId: 成员ID<br>返回: bool |
| `leave_team()` | `leaveTeam()` | 离开队伍 | 返回: bool |
| `create_vote(subject, timeout?)` | `createVote(subject, timeout?)` | 发起投票 | subject: 主题<br>timeout: 超时(ms)<br>返回: bool |
| `cast_vote(voteId, response)` | `castVote(voteId, response)` | 投票 | voteId: 投票ID<br>response: 响应<br>返回: bool |
| `get_vote_result(voteId)` | `getVoteResult(voteId)` | 获取投票结果 | voteId: 投票ID<br>返回: JSON字符串 |
| `report_status(status)` | `reportStatus(status)` | 汇报状态 | status: 状态对象<br>返回: bool |
| `broadcast(message)` | `broadcast(message)` | 广播消息 | message: 消息对象<br>返回: bool |
| `get_team_status()` | `getTeamStatus()` | 获取队伍状态 | 返回: JSON字符串 |
| `is_joined()` | `isJoined()` | 是否已加入 | 返回: bool |
| `get_member_id()` | `getMemberId()` | 获取成员ID | 返回: string |
| `on(event, callback)` | `on(event, callback)` | 订阅事件 | event: 事件名<br>callback: 回调<br>返回: bool |

---

## 协议消息格式

### Runtime → Server

**加入队伍**：
```json
{
  "type": "team.join",
  "teamId": "team_001",
  "memberId": "player_01",
  "timestamp": 1234567890
}
```

**离开队伍**：
```json
{
  "type": "team.leave",
  "teamId": "team_001",
  "memberId": "player_01",
  "timestamp": 1234567890
}
```

**发起投票**：
```json
{
  "type": "team.vote_create",
  "teamId": "team_001",
  "proposerId": "player_01",
  "subject": "是否开始副本",
  "timeout": 30000,
  "timestamp": 1234567890
}
```

**投票**：
```json
{
  "type": "team.vote_cast",
  "teamId": "team_001",
  "voteId": "vote_123",
  "memberId": "player_01",
  "response": "agree",
  "timestamp": 1234567890
}
```

**广播**：
```json
{
  "type": "team.broadcast",
  "teamId": "team_001",
  "memberId": "player_01",
  "message": {"type": "coordination", "action": "move"},
  "timestamp": 1234567890
}
```

**状态汇报**：
```json
{
  "type": "team.status_report",
  "teamId": "team_001",
  "memberId": "player_01",
  "status": {"hp": 100, "ready": true},
  "timestamp": 1234567890
}
```

### Server → Runtime

**加入确认**：
```json
{
  "type": "team.joined",
  "teamId": "team_001",
  "leaderId": "player_leader",
  "memberId": "player_01",
  "members": ["player_leader", "player_01"],
  "timestamp": 1234567890
}
```

**队友加入**：
```json
{
  "type": "team.member_joined",
  "teamId": "team_001",
  "memberId": "player_02",
  "timestamp": 1234567890
}
```

**队友离开**：
```json
{
  "type": "team.member_left",
  "teamId": "team_001",
  "memberId": "player_02",
  "timestamp": 1234567890
}
```

**投票开始**：
```json
{
  "type": "team.vote_started",
  "voteId": "vote_123",
  "teamId": "team_001",
  "proposerId": "player_leader",
  "subject": "是否开始副本",
  "deadline": 1234599999,
  "timestamp": 1234567890
}
```

**投票结束**：
```json
{
  "type": "team.vote_ended",
  "voteId": "vote_123",
  "result": {
    "voteId": "vote_123",
    "subject": "是否开始副本",
    "responses": {"player_01": "agree", "player_02": "agree"},
    "active": false
  },
  "timestamp": 1234567890
}
```

**收到广播**：
```json
{
  "type": "team.broadcast_received",
  "senderId": "player_leader",
  "teamId": "team_001",
  "message": {"type": "coordination", "action": "attack"},
  "timestamp": 1234567890
}
```

---

## 注意事项

1. **依赖 inbox 模块**：
   - team 模块通过 inbox 接收服务器消息
   - 需要先连接 inbox 才能使用 team 功能

2. **本地状态同步**：
   - `getTeamStatus()` 返回本地缓存的状态
   - 实际状态以服务器为准，通过事件更新

3. **事件订阅时机**：
   - 建议在 `joinTeam()` 之前订阅事件
   - 确保不会错过任何事件通知

4. **投票超时**：
   - 投票有超时时间，超时后自动结束
   - 超时后的投票结果仍可通过 `getVoteResult()` 获取

5. **广播消息**：
   - 广播消息由服务器转发给所有队友
   - 发送者不会收到自己的广播（避免重复）

6. **线程安全**：
   - 所有函数都是线程安全的
   - 事件回调可能在独立线程中执行
