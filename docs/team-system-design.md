# 游戏组队协同系统架构设计

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
│  │ - WebSocket  │  │              │  │   - vote:{id}          │  │
│  └──────────────┘  └──────────────┘  │   - teammate:{name}    │  │
│                                        │   (内存 + SQLite持久化)│  │
│  ┌──────────────┐                     └─────────────────────────┘  │
│  │   SQLite     │                                                     │
│  │ 历史数据     │                                                     │
│  └──────────────┘                                                     │
└─────────────────────────────────────────────────────────────────────┘
         ▲ │
         │ │ HTTP + WebSocket
         │ │
┌────────┴────────┐  ┌────────┴────────┐  ┌────────┴────────┐
│    Client A     │  │    Client B     │  │    Client C     │
│                 │  │                 │  │                 │
│ Lua 脚本        │  │ Lua 脚本        │  │ Lua 脚本        │
│   ↓             │  │   ↓             │  │   ↓             │
│ 1. http.post()  │  │ 1. http.post()  │  │ 1. http.post()  │
│    → 账号池      │  │    → 账号池     │  │    → 账号池     │
│ 2. 登录游戏     │  │ 2. 登录游戏     │  │ 2. 登录游戏     │
│ 3. http.post()  │  │ 3. http.post()  │  │ 3. http.post()  │
│    → Server汇报 │  │    → Server汇报 │  │    → Server汇报 │
│ 4. kv.get()     │  │ 4. kv.get()     │  │ 4. kv.get()     │
│    → 获取队伍    │  │    → 获取队伍   │  │    → 获取队伍   │
│ 5. 游戏内组队    │  │ 5. 游戏内组队   │  │ 5. 游戏内组队   │
│ 6. 监听投票     │  │ 6. 监听投票     │  │ 6. 监听投票     │
│ 7. 执行指令     │  │ 7. 执行指令     │  │ 7. 执行指令     │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

## 数据流

### 1. 组队编排流程

```
Client A                    Server                   Client B
   │                          │                          │
   │  POST /alloc            │                          │
   │─────────────────────> 账号池                       │
   │  ← 账号 info             │                          │
   │                          │                          │
   │  登录游戏                │                          │
   │                          │                          │
   │  POST /report            │                          │
   │  {status: "logged_in"}   │                          │
   │─────────────────────────>│                          │
   │                          │ 保存状态到 KV              │
   │                          │ client:A → {status, team} │
   │                          │                          │
   │                          │  足够玩家就绪               │
   │                          │  → 编排队伍                 │
   │                          │                          │
   │  POST /team/assign        │  POST /team/assign        │
   │<─────────────────────────│─────────────────────────>│
   │  {teamId, members: [...] │  {teamId, members: [...]   │
   │   leader: true}          │   leader: false}          │
   │                          │                          │
   │  游戏内组队               │  游戏内组队               │
```

### 2. 投票踢人流程

```
游戏界面                     Client A                Server                 Client B
   │                           │                       │                       │
   │  [投票弹窗出现]             │                       │                       │
   │  被投: 路人X               │                       │                       │
   ├──────────────────────────>│                       │                       │
   │  识别投票事件               │                       │                       │
   │                           │  POST /vote/report     │                       │
   │                           │───────────────────────>|                       │
   │                           │  {type: "kick",        │                       │
   │                           │   target: "路人X",     │                       │
   │                           │   initiator: "A"}      │                       │
   │                           │                       │  检查 KV                │
   │                           │                       │  - 是自己人发起?       │
   │                           │                       │  - 被投的是路人?       │
   │                           │                       │  → 决定: 同意          │
   │                           │  POST /vote/agree     │  POST /vote/agree     │
   │                           │<───────────────────────│<───────────────────────│
   │                           │  ↓                      │  ↓                     │
   │  点击"同意"                │                       │  点击"同意"           │
   │<──────────────────────────│                       │<──────────────────────│
```

## KeyValueStore 数据结构

```cpp
// 队伍信息
kv.hset("team:123", "leader", "client_a");
kv.hset("team:123", "state", "matching");
kv.hset("team:123", "createdAt", "1234567890");

// 队伍成员 (JSON 数组)
kv.set("team:123:members", json.encode([
    {clientId: "a", username: "PlayerA", role: "leader"},
    {clientId: "b", username: "PlayerB", role: "member"},
    {clientId: "c", username: "PlayerC", role: "member"}
]));

// Client 状态
kv.hset("client:a", "status", "in_team");
kv.hset("client:a", "teamId", "123");
kv.hset("client:a", "username", "PlayerA");
kv.hset("client:a", "lastHeartbeat", "1234567890");

// 游戏名 → Wingman Client 映射 (用于判断是不是自己人)
kv.set("teammate:PlayerA", "client_a");
kv.set("teammate:PlayerB", "client_b");
kv.set("teammate:PlayerC", "client_c");

// 投票状态
kv.hset("vote:123", "target", "路人X");
kv.hset("vote:123", "initiator", "PlayerA");
kv.hset("vote:123", "state", "active");
kv.lpush("vote:123:agree", "client_a");
kv.lpush("vote:123:agree", "client_b");
```

## Lua API 示例

```lua
-- ========== HTTP 客户端 ==========
local http = require("http")

-- GET 请求
local resp = http.get("http://account-pool/alloc?count=1")
if resp.status == 200 then
    local account = json.decode(resp.body)
    login(account.username, account.password)
end

-- POST JSON
local resp = http.post(server_url .. "/report", {
    headers = {["Content-Type"] = "application/json"},
    body = json.encode({
        clientId = client_id,
        status = "logged_in",
        username = player_name
    })
})

-- POST 表单
local resp = http.postForm(login_url, {
    username = "user",
    password = "pass"
})

-- ========== JSON ==========
local json = require("json")

local data = json.decode(resp.body)
print(data.status)
print(data.teamId)

local encoded = json.encode({
    members = {"A", "B", "C"},
    count = 3
})

-- ========== KV 存储 (本地缓存) ==========
local kv = require("kv")

-- 字符串操作
kv.set("last_update", util.getTime())
kv.set("token", auth_token, {ttl = 3600})  -- 1小时过期
local token = kv.get("token")

-- Hash 操作
kv.hset("team_info", "leader", "PlayerA")
kv.hset("team_info", "state", "ready")
local leader = kv.hget("team_info", "leader")
local all = kv.hgetall("team_info")

-- List 操作
kv.lpush("vote_log", util.getTime() .. ": agree")
kv.lpush("vote_log", util.getTime() .. ": agree")
local logs = kv.lrange("vote_log", 0, -1)
```

## Server API 设计

```
POST /api/client/register
  - 注册新 Client
  - 返回: {clientId}

POST /api/client/heartbeat
  - 心跳汇报
  - body: {clientId, status, gameId, username}

POST /api/team/create
  - 创建队伍
  - body: {clientId, name, maxSize}
  - 返回: {teamId}

POST /api/team/join
  - 加入队伍
  - body: {clientId, teamId}

GET /api/team/info/:teamId
  - 获取队伍信息

POST /api/vote/report
  - 汇报投票事件
  - body: {teamId, type, target, initiator}

POST /api/vote/action
  - Server 下发投票指令
  - Client 轮询此接口接收指令

WebSocket /api/ws
  - 实时推送
  - 消息: {type: "vote_agree", target: "xxx"}
```

## 依赖更新

```json
// vcpkg.json 新增
{
  "name": "curl",
  "platform": "windows"
}
```

```cmake
# CMakeLists.txt
find_package(CURL CONFIG REQUIRED)
target_link_libraries(wingman PRIVATE CURL::libcurl)
```
