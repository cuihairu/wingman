# API: wingman.team

多 Client 协同组队、投票协调功能。

## 模块概述

team 模块提供多 Client 协同功能：
- **组队分配** - 请求队伍分配、获取队友信息
- **投票协调** - 汇报投票事件、获取投票建议
- **状态汇报** - 注册 Client、汇报登录状态

---

## 架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                        账号池服务 (独立)                             │
│                   REST API: /alloc, /release                        │
└─────────────────────────────────────────────────────────────────────┘
                              ▲
                              │ HTTP
                              │
┌─────────────────────────────────────────────────────────────────────┐
│                        Wingman Server                              │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌─────────────────────────┐  │
│  │ HTTP Server  │  │ 编排引擎     │  │   KeyValueStore         │  │
│  │              │  │              │  │   (类 Redis)            │  │
│  │ - 接收汇报   │←→│ - 队伍分配   │←→│   - team:{id}          │  │
│  │ - 下发指令   │  │ - 投票协调   │  │   - client:{id}        │  │
│  └──────────────┘  └──────────────┘  │   - vote:{id}          │  │
│                                        │   - teammate:{name}    │  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 注册 Client

### register() / register()

**说明**：注册新 Client。

**函数签名**：

```python
register() -> str
```

```lua
register() -> string
```

**返回**：
- Client ID

---

## 汇报状态

### heartbeat(client_id, status, game_id?, username?) / heartbeat(clientId, status, gameId?, username?)

**说明**：Client 心跳汇报。

**函数签名**：

```python
heartbeat(client_id: str, status: str, game_id: str = "", username: str = "") -> dict
```

```lua
heartbeat(clientId: string, status: string, gameId: string = "", username: string = "") -> table
```

**参数**：
- `client_id` / `clientId` - Client ID
- `status` - 状态：`"logged_in"`, `"in_team"` 等
- `game_id` / `gameId` - 可选，游戏账号 ID
- `username` - 可选，玩家名称

**返回**：
- HTTP 响应对象

:::tabs

== Python

```python:line-numbers
from wingman import http, json

# 向 Server 汇报状态
resp = http.post("http://server/api/client/heartbeat", json.encode({
    "clientId": client_id,
    "status": "logged_in",
    "gameId": game_id,
    "username": player_name
}))
```

== Lua

```lua:line-numbers
local http = require("wingman.http")
local json = require("wingman.json")

-- 向 Server 汇报状态
local resp = http.post("http://server/api/client/heartbeat", json.encode({
    clientId = clientId,
    status = "logged_in",
    gameId = gameId,
    username = playerName
}))
```

:::

---

## 请求组队分配

### allocate(client_id, username, preferred_size) / allocate(clientId, username, preferredSize)

**说明**：请求组队分配。

**函数签名**：

```python
allocate(client_id: str, username: str, preferred_size: int) -> dict
```

```lua
allocate(clientId: string, username: string, preferredSize: number) -> table
```

**参数**：
- `client_id` / `clientId` - Client ID
- `username` - 玩家名称
- `preferred_size` / `preferredSize` - 期望队伍大小

**返回**：
- 分配结果对象：
  - `teamId` / `teamId` - 队伍 ID
  - `isLeader` / `isLeader` - 是否是队长
  - `teammates` / `teammates` - 队友名称列表

:::tabs

== Python

```python:line-numbers
from wingman import http, json

resp = http.post("http://server/api/team/allocate", json.encode({
    "clientId": client_id,
    "username": player_name,
    "preferredSize": 3
}))

if resp['success']:
    result = json.decode(resp['body'])
    print(f"队伍ID: {result['teamId']}")
    print(f"是否队长: {result['isLeader']}")
    print(f"队友: {', '.join(result['teammates'])}")
```

== Lua

```lua:line-numbers
local http = require("wingman.http")
local json = require("wingman.json")

local resp = http.post("http://server/api/team/allocate", json.encode({
    clientId = clientId,
    username = playerName,
    preferredSize = 3
}))

if resp.success then
    local result = json.decode(resp.body)
    print("队伍ID:", result.teamId)
    print("是否队长:", tostring(result.isLeader))
    print("队友:", table.concat(result.teammates, ", "))
end
```

:::

---

## 汇报投票事件

### report_vote(team_id, vote_type, target, initiator) / reportVote(teamId, voteType, target, initiator)

**说明**：汇报投票事件。

**函数签名**：

```python
report_vote(team_id: str, vote_type: str, target: str, initiator: str) -> dict
```

```lua
reportVote(teamId: string, voteType: string, target: string, initiator: string) -> table
```

**参数**：
- `team_id` / `teamId` - 队伍 ID
- `vote_type` / `voteType` - 投票类型（如 `"kick"`）
- `target` - 投票目标
- `initiator` - 发起人

**返回**：
- HTTP 响应对象

---

## 获取待处理投票

### get_pending_votes(client_id) / getPendingVotes(clientId)

**说明**：获取待处理的投票动作。

**函数签名**：

```python
get_pending_votes(client_id: str) -> list[dict]
```

```lua
getPendingVotes(clientId: string) -> table
```

**参数**：
- `client_id` / `clientId` - Client ID

**返回**：
- 待处理投票动作列表，每个动作包含：
  - `voteId` / `voteId` - 投票 ID
  - `agree` / `agree` - 是否建议同意

:::tabs

== Python

```python:line-numbers
from wingman import http, json, util

# 轮询待处理的投票动作
while True:
    resp = http.get(f"http://server/api/vote/pending?clientId={client_id}")
    if resp['success']:
        actions = json.decode(resp['body'])
        for action in actions:
            # Server 建议同意
            if action['agree']:
                click_vote_button("agree")
            else:
                click_vote_button("disagree")
    util.sleep(1000)
```

== Lua

```lua:line-numbers
local http = require("wingman.http")
local json = require("wingman.json")
local util = require("wingman.util")

-- 轮询待处理的投票动作
while true do
    local resp = http.get("http://server/api/vote/pending?clientId=" .. clientId)
    if resp.success then
        local actions = json.decode(resp.body)
        for _, action in ipairs(actions) do
            -- Server 建议同意
            if action.agree then
                clickVoteButton("agree")
            else
                clickVoteButton("disagree")
            end
        end
    end
    util.sleep(1000)
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `register()` | `register()` | 注册Client | 返回: Client ID |
| `heartbeat(clientId, status, gameId?, username?)` | `heartbeat(clientId, status, gameId?, username?)` | 汇报状态 | clientId: ClientID<br>status: 状态<br>gameId: 游戏ID(可选)<br>username: 玩家名(可选)<br>返回: HTTP响应 |
| `allocate(clientId, username, preferredSize)` | `allocate(clientId, username, preferredSize)` | 请求组队 | clientId: ClientID<br>username: 玩家名<br>preferredSize: 期望队伍大小<br>返回: 分配结果 |
| `report_vote(teamId, voteType, target, initiator)` | `reportVote(teamId, voteType, target, initiator)` | 汇报投票 | teamId: 队伍ID<br>voteType: 投票类型<br>target: 目标<br>initiator: 发起人<br>返回: HTTP响应 |
| `get_pending_votes(clientId)` | `getPendingVotes(clientId)` | 获取待处理投票 | clientId: ClientID<br>返回: 投票动作列表 |

---

## Server API 接口

### POST /api/client/register

注册新 Client。

**请求：**
```json
{}
```

**响应：**
```json
{
  "clientId": "client_1234567890"
}
```

### POST /api/client/heartbeat

Client 心跳汇报。

**请求：**
```json
{
  "clientId": "client_123",
  "status": "logged_in",
  "gameId": "game_account_1",
  "username": "PlayerName"
}
```

### POST /api/team/allocate

请求组队分配。

**请求：**
```json
{
  "clientId": "client_123",
  "username": "PlayerName",
  "preferredSize": 3
}
```

**响应：**
```json
{
  "success": true,
  "teamId": "team_456",
  "message": "Created new team",
  "isLeader": true,
  "teammates": ["PlayerName"]
}
```

### POST /api/vote/report

汇报投票事件。

**请求：**
```json
{
  "teamId": "team_456",
  "type": "kick",
  "target": "RandomPlayer",
  "initiator": "TeammateA"
}
```

**响应：**
```json
{
  "voteId": "vote_789",
  "recommendAction": "agree"
}
```

### GET /api/vote/pending?clientId=xxx

获取待处理的投票动作。

**响应：**
```json
[
  {
    "voteId": "vote_789",
    "agree": true
  }
]
```

---

## KV 数据结构

### 队伍信息

```
team:{id}
  ├─ leader: client_id
  ├─ state: matching | ready | active
  └─ maxSize: number
```

### 队伍成员

```
team:{id}:members (list)
  └─ [client_id, ...]
```

### Client 状态

```
client:{id}
  ├─ status: idle | in_team
  ├─ teamId: team_id
  └─ username: player_name
```

### 队友映射

```
teammate:{player_name}
  └─ client_id
```

### 投票状态

```
vote:{id}
  ├─ target: player_name
  ├─ initiator: player_name
  └─ recommendAction: agree | disagree
```
