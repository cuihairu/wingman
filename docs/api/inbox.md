# API: wingman.inbox

Inbox 模块提供基于 TCP 的消息收件箱功能，用于接收和处理来自服务器的消息。

## 模块概述

inbox 模块封装了消息队列模式，提供可靠的消息传递机制：

- **消息消费**：从服务器拉取待处理消息
- **消息确认**：确认消息已接收
- **任务报告**：上报任务处理结果
- **心跳机制**：保持连接活跃
- **自动重连**：连接断开时自动重连

### 架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│                        Runtime Client                            │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                     Inbox Module                          │  │
│  │                                                          │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐  │  │
│  │  │ connect  │→│ consume  │→│   ack    │→│ report  │  │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └─────────┘  │  │
│  │                                                          │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │           Message Queue                             │  │  │
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐              │  │  │
│  │  │  │ Msg 1   │ │ Msg 2   │ │ Msg 3   │ ...         │  │  │
│  │  │  └─────────┘ └─────────┘ └─────────┘              │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  │                                                          │  │
│  │  ┌────────────────────────────────────────────────────┐  │  │
│  │  │           Pending Messages                         │  │  │
│  │  │  (已消费但未 report 的消息)                         │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                  ↑                               │
│                                  │ TCP                            │
│                                  ↓                               │
└─────────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────────┐
│                        Go Server                                │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              TeamManager (Inboxes)                       │  │
│  │                                                          │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐              │  │
│  │  │ Agent A  │  │ Agent B  │  │ Agent C  │  ...         │  │
│  │  │ Inbox    │  │ Inbox    │  │ Inbox    │              │  │
│  │  └──────────┘  └──────────┘  └──────────┘              │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 消息生命周期

```
┌─────────┐   consume   ┌─────────┐   ack    ┌─────────┐   report   ┌─────────┐
│ Server  │────────────→│  Queue  │─────────→│Pending  │───────────→│ Deleted │
│ Inbox   │             │(Runtime)│          │ Messages│            │         │
└─────────┘             └─────────┘          └─────────┘            └─────────┘
```

---

## 连接服务器

### connect(url, config?)

**说明**：连接到服务器并注册收件箱。

**函数签名**：

```python
connect(url: str, config: dict = None) -> dict
```

```lua
connect(url: string, config: table) -> table
```

**参数**：
- `url` - 服务器 URL，格式 `tcp://host:port`
- `config` - 可选配置对象：
  - `agentId` / `agentId` - Agent ID（默认自动生成）
  - `heartbeatInterval` / `heartbeatInterval` - 心跳间隔（毫秒，默认 30000）
  - `consumeTimeout` / `consumeTimeout` - consume 超时（毫秒，默认 5000）
  - `maxPendingMessages` / `maxPendingMessages` - 最大待处理消息数（默认 100）

**返回**：连接结果对象
- `success` - 是否连接成功
- `handle` - 收件箱句柄（用于后续操作）
- `error` - 错误信息（失败时）

:::tabs

== Python

```python:line-numbers
from wingman import inbox

# 连接到服务器
result = inbox.connect("tcp://192.168.1.100:9000", {
    "agentId": "runtime_001",
    "heartbeatInterval": 30000,
    "consumeTimeout": 5000
})

if result["success"]:
    print(f"连接成功，句柄: {result['handle']}")
    handle = result["handle"]
else:
    print(f"连接失败: {result['error']}")
```

== Lua

```lua:line-numbers
local inbox = require("wingman.inbox")

-- 连接到服务器
local result = inbox.connect("tcp://192.168.1.100:9000", {
    agentId = "runtime_001",
    heartbeatInterval = 30000,
    consumeTimeout = 5000
})

if result.success then
    print("连接成功，句柄:", result.handle)
    handle = result.handle
else
    print("连接失败:", result.error)
end
```

:::

---

## 消费消息

### consume(handle?, timeout?)

**说明**：从服务器拉取待处理消息，阻塞直到有消息或超时。

**函数签名**：

```python
consume(handle: int = 1, timeout: int = 5000) -> dict | None
```

```lua
consume(handle: number = 1, timeout: number = 5000) -> table | nil
```

**参数**：
- `handle` - 收件箱句柄（默认 1）
- `timeout` - 超时时间（毫秒，默认 5000）

**返回**：消息对象或 `null`（超时）
- `msgId` - 消息 ID
- `type` - 消息类型
- `payload` - 消息内容
- `timestamp` - 时间戳

:::tabs

== Python

```python:line-numbers
from wingman import inbox

# 持续消费消息
while True:
    msg = inbox.consume(handle, 5000)
    
    if msg is None:
        print("超时，没有消息")
        continue
    
    print(f"收到消息: {msg['msgId']}")
    print(f"类型: {msg['type']}")
    print(f"内容: {msg['payload']}")
    
    # 处理消息...
    
    # 确认并报告完成
    inbox.ack(handle, msg['msgId'])
    inbox.report(handle, msg['msgId'], {"status": "done"})
```

== Lua

```lua:line-numbers
local inbox = require("wingman.inbox")

-- 持续消费消息
while true do
    local msg = inbox.consume(handle, 5000)
    
    if msg == nil then
        print("超时，没有消息")
        -- 继续循环
    else
        print("收到消息:", msg.msgId)
        print("类型:", msg.type)
        print("内容:", msg.payload)
        
        -- 处理消息...
        
        -- 确认并报告完成
        inbox.ack(handle, msg.msgId)
        inbox.report(handle, msg.msgId, {status = "done"})
    end
end
```

:::

---

## 确认消息

### ack(handle, msgId)

**说明**：确认已收到消息（防止服务器重发）。

**函数签名**：

```python
ack(handle: int, msgId: str) -> bool
```

```lua
ack(handle: number, msgId: string) -> boolean
```

**参数**：
- `handle` - 收件箱句柄
- `msgId` - 消息 ID

**返回**：是否确认成功

---

## 报告完成

### report(handle, msgId, result?)

**说明**：上报任务处理结果，消息会被服务器删除。

**函数签名**：

```python
report(handle: int, msgId: str, result: dict = None) -> bool
```

```lua
report(handle: number, msgId: string, result: table) -> boolean
```

**参数**：
- `handle` - 收件箱句柄
- `msgId` - 消息 ID
- `result` - 可选，处理结果

**返回**：是否上报成功

:::tabs

== Python

```python:line-numbers
from wingman import inbox

msg = inbox.consume()

if msg:
    # 处理消息
    result = process_message(msg)
    
    # 先确认
    inbox.ack(handle, msg['msgId'])
    
    # 再报告完成
    inbox.report(handle, msg['msgId'], {
        "status": "success",
        "output": result
    })
```

== Lua

```lua:line-numbers
local inbox = require("wingman.inbox")

local msg = inbox.consume()

if msg ~= nil then
    -- 处理消息
    local result = processMessage(msg)
    
    -- 先确认
    inbox.ack(handle, msg.msgId)
    
    -- 再报告完成
    inbox.report(handle, msg.msgId, {
        status = "success",
        output = result
    })
end
```

:::

---

## 心跳

### heartbeat(handle?)

**说明**：手动发送心跳（通常由内部自动处理）。

**函数签名**：

```python
heartbeat(handle: int = 1) -> bool
```

```lua
heartbeat(handle: number = 1) -> boolean
```

**返回**：是否发送成功

---

## 连接管理

### isConnected(handle?)

**说明**：检查是否仍连接到服务器。

**函数签名**：

```python
is_connected(handle: int = 1) -> bool
```

```lua
isConnected(handle: number = 1) -> boolean
```

**返回**：是否已连接

---

### disconnect(handle?)

**说明**：断开连接并释放资源。

**函数签名**：

```python
disconnect(handle: int = 1) -> bool
```

```lua
disconnect(handle: number = 1) -> boolean
```

**返回**：是否成功断开

:::tabs

== Python

```python:line-numbers
from wingman import inbox

# 连接
handle = inbox.connect("tcp://server:9000")["handle"]

# 使用完毕
inbox.disconnect(handle)
```

== Lua

```lua:line-numbers
local inbox = require("wingman.inbox")

-- 连接
local handle = inbox.connect("tcp://server:9000").handle

-- 使用完毕
inbox.disconnect(handle)
```

:::

---

## 完整示例

### 消息消费者

:::tabs

== Python

```python:line-numbers
from wingman import inbox

# 连接
result = inbox.connect("tcp://192.168.1.100:9000", {
    "agentId": "worker_001"
})

if not result["success"]:
    print("连接失败")
    exit()

handle = result["handle"]
print("已连接到服务器")

# 消费消息循环
while True:
    msg = inbox.consume(handle, 5000)
    
    if msg is None:
        continue  # 超时，继续等待
    
    try:
        # 处理消息
        msg_type = msg["type"]
        payload = msg["payload"]
        
        print(f"处理消息: {msg['msgId']} ({msg_type})")
        
        # 根据类型处理
        if msg_type == "task.execute":
            result = execute_task(payload)
        elif msg_type == "team.joined":
            result = handle_team_join(payload)
        else:
            result = {"status": "unknown_type"}
        
        # 确认并报告
        inbox.ack(handle, msg["msgId"])
        inbox.report(handle, msg["msgId"], result)
        
    except Exception as e:
        print(f"处理失败: {e}")
        inbox.ack(handle, msg["msgId"])
        inbox.report(handle, msg["msgId"], {"status": "error", "error": str(e)})
```

== Lua

```lua:line-numbers
local inbox = require("wingman.inbox")

-- 连接
local result = inbox.connect("tcp://192.168.1.100:9000", {
    agentId = "worker_001"
})

if not result.success then
    print("连接失败")
    return
end

local handle = result.handle
print("已连接到服务器")

-- 消费消息循环
while true do
    local msg = inbox.consume(handle, 5000)
    
    if msg == nil then
        -- 超时，继续等待
    else
        -- 处理消息
        local msg_type = msg.type
        local payload = msg.payload
        
        print("处理消息:", msg.msgId, "(" .. msg_type .. ")")
        
        -- 根据类型处理
        local result
        if msg_type == "task.execute" then
            result = executeTask(payload)
        elseif msg_type == "team.joined" then
            result = handleTeamJoin(payload)
        else
            result = {status = "unknown_type"}
        end
        
        -- 确认并报告
        inbox.ack(handle, msg.msgId)
        inbox.report(handle, msg.msgId, result)
    end
end
```

:::

---

## 可用接口

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `connect(url, config?)` | `connect(url, config?)` | 连接服务器 | url: tcp://host:port<br>config: 配置对象<br>返回: {success,handle,error?} |
| `consume(handle?, timeout?)` | `consume(handle?, timeout?)` | 消费消息 | handle: 句柄<br>timeout: 超时(ms)<br>返回: 消息或 null |
| `ack(handle, msgId)` | `ack(handle, msgId)` | 确认消息 | handle: 句柄<br>msgId: 消息ID<br>返回: bool |
| `report(handle, msgId, result?)` | `report(handle, msgId, result?)` | 报告完成 | handle: 句柄<br>msgId: 消息ID<br>result: 结果对象<br>返回: bool |
| `heartbeat(handle?)` | `heartbeat(handle?)` | 发送心跳 | handle: 句柄<br>返回: bool |
| `is_connected(handle?)` | `isConnected(handle?)` | 检查连接 | handle: 句柄<br>返回: bool |
| `disconnect(handle?)` | `disconnect(handle?)` | 断开连接 | handle: 句柄<br>返回: bool |

---

## 协议消息格式

### 客户端 → 服务器

**注册**：
```json
{
  "type": "inbox.register",
  "agentId": "runtime_001"
}
```

**心跳**：
```json
{
  "type": "inbox.heartbeat",
  "agentId": "runtime_001",
  "timestamp": 1234567890
}
```

**确认**：
```json
{
  "type": "inbox.ack",
  "msgId": "msg_123",
  "agentId": "runtime_001"
}
```

**报告**：
```json
{
  "type": "inbox.report",
  "msgId": "msg_123",
  "agentId": "runtime_001",
  "result": {"status": "done"},
  "timestamp": 1234567890
}
```

### 服务器 → 客户端

**消息**：
```json
{
  "type": "inbox.message",
  "msgId": "msg_123",
  "messageType": "team.joined",
  "payload": {"teamId": "team_1"},
  "timestamp": 1234567890
}
```

**心跳确认**：
```json
{
  "type": "inbox.heartbeat_ack",
  "timestamp": 1234567890
}
```

**注册确认**：
```json
{
  "type": "inbox.register_ack",
  "agentId": "runtime_001"
}
```

---

## 注意事项

1. **ack + report 语义**：
   - `ack()` 确认消息已接收，服务器不会重发
   - `report()` 上报任务完成，服务器删除消息
   - 两者都调用才算完整处理

2. **消息重传**：
   - 未 ack 的消息可能会被服务器重发
   - 已 ack 但未 report 的消息保持在 pending 状态

3. **心跳机制**：
   - 模块内部自动发送心跳（默认 30 秒）
   - 长时间未收到心跳确认会触发重连

4. **资源管理**：
   - 使用完毕后调用 `disconnect()` 释放资源
   - 程序退出前务必断开连接

5. **线程安全**：
   - 所有函数都是线程安全的
   - consume 会阻塞调用线程，建议在独立线程中使用
