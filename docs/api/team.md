# 组队编排模块

多 Client 协同组队、投票协调功能。

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

## Lua API 使用示例

### 注册 Client 并汇报状态

```lua
local http = require("http")
local json = require("json")

-- 向账号池请求账号
local resp = http.post("http://account-pool/alloc", json.encode({
    count = 1
}))

if resp.success then
    local account = json.decode(resp.body)
    -- 登录游戏...
    login(account.username, account.password)
end

-- 向 Server 汇报状态
local resp = http.post("http://server/api/client/heartbeat", json.encode({
    clientId = client_id,
    status = "logged_in",
    gameId = game_id,
    username = player_name
}))
```

### 请求组队分配

```lua
local resp = http.post("http://server/api/team/allocate", json.encode({
    clientId = client_id,
    username = player_name,
    preferredSize = 3  -- 期望队伍大小
}))

if resp.success then
    local result = json.decode(resp.body)
    print("队伍ID:", result.teamId)
    print("是否队长:", result.isLeader)
    print("队友:", table.concat(result.teammates, ", "))

    -- 游戏内组队
    for _, teammate in ipairs(result.teammates) do
        inviteToTeam(teammate)
    end
end
```

### 投票协调

```lua
-- 汇报投票事件
local function reportVote(voteType, target, initiator)
    local resp = http.post("http://server/api/vote/report", json.encode({
        teamId = team_id,
        type = voteType,      -- "kick", "surrender", etc.
        target = target,      -- 被投票目标
        initiator = initiator  -- 发起人
    }))
end

-- 轮询待处理的投票动作
while true do
    local resp = http.get("http://server/api/vote/pending?clientId=" .. client_id)
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
    sleep(1000)
end
```

## Server API 接口

### POST /api/client/register
注册新 Client

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
Client 心跳汇报

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
请求组队分配

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
汇报投票事件

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
获取待处理的投票动作

**响应：**
```json
[
    {
        "voteId": "vote_789",
        "agree": true
    }
]
```

## KV 数据结构

```lua
-- 队伍信息
kv.hset("team:123", "leader", "client_a")
kv.hset("team:123", "state", "matching")
kv.hset("team:123", "maxSize", "5")

-- 队伍成员
kv.lpush("team:123:members", "client_a")
kv.lpush("team:123:members", "client_b")

-- Client 状态
kv.hset("client:a", "status", "in_team")
kv.hset("client:a", "teamId", "123")
kv.hset("client:a", "username", "PlayerA")

-- 游戏名 → Client 映射（判断是不是自己人）
kv.set("teammate:PlayerA", "client_a")
kv.set("teammate:PlayerB", "client_b")

-- 投票状态
kv.hset("vote:789", "target", "路人X")
kv.hset("vote:789", "initiator", "PlayerA")
kv.hset("vote:789", "recommendAction", "agree")
```
