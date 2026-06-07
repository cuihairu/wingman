# API: wingman.event

事件订阅和发布模块，提供进程内和跨进程事件通信。

## 模块概述

event 模块提供发布-订阅模式的事件系统：
- **进程内事件** - 本地事件总线，用于模块间通信
- **跨进程事件** - 基于 Redis Stream 的远程事件（需 hiredis 支持）
- **订阅事件** - 持久订阅或一次性订阅
- **发布事件** - 触发事件并传递数据
- **取消订阅** - 取消指定订阅或清空全部
- **事件对象** - 标准化的事件消息结构

### 进程内 vs 跨进程

| 特性 | 进程内事件 | 跨进程事件 |
|------|-----------|-----------|
| **通信范围** | 单进程内 | 多进程/多机器 |
| **实现方式** | 内存事件总线 | Redis Stream |
| **持久化** | ❌ 无 | ✅ 消息持久化 |
| **离线消息** | ❌ 无 | ✅ 重连可获取 |
| **性能** | 极高 | 高（网络 I/O） |

---

## 订阅事件

### on(type, callback, name?) / on(type, callback, name?)

**说明**：订阅事件，每次事件触发时都会调用回调。

**函数签名**：

```python
on(type: str, callback: Callable, name: str = "") -> str
```

```lua
on(type: string, callback: function, name: string = "") -> string
```

**参数**：
- `type` - 事件名（支持点号分隔的命名空间，如 `"combat.enemy_found"`）
- `callback` - 回调函数，接收 `EventMessage` 对象
- `name` - 可选，订阅名称，可用于按名称取消订阅

**返回**：
- 订阅 ID

:::tabs

== Python

```python:line-numbers
from wingman import event

# 定义事件处理函数
def on_enemy(e):
    print(f"Found enemy at {e['payload']}")

# 订阅事件
sub_id = event.on("combat.enemy_found", on_enemy, name="my-handler")
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 定义事件处理函数
local function onEnemy(e)
    print("Found enemy at " .. e.payload.x)
end

-- 订阅事件
local id = event.on("combat.enemy_found", onEnemy, "my-handler")
```

:::

---

## 一次性订阅

### once(type, callback) / once(type, callback)

**说明**：订阅事件，仅触发一次后自动取消订阅。

**函数签名**：

```python
once(type: str, callback: Callable) -> str
```

```lua
once(type: string, callback: function) -> string
```

**参数**：
- `type` - 事件名
- `callback` - 回调函数，接收 `EventMessage` 对象

**返回**：
- 订阅 ID

:::tabs

== Python

```python:line-numbers
from wingman import event

# 一次性订阅
event.once("task.done", lambda e: print("Task done!"))
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 一次性订阅
event.once("task.done", function(e)
    print("Task done!")
end)
```

:::

---

## 发布事件

### emit(type, payload?, meta?) / emit(type, payload?, meta?)

**说明**：触发事件，通知所有订阅者。

**函数签名**：

```python
emit(type: str, payload: Any = None, meta: dict = None) -> bool
```

```lua
emit(type: string, payload: any = nil, meta: table = nil) -> boolean
```

**参数**：
- `type` - 事件名
- `payload` - 可选，事件载荷（任意 JSON 兼容对象）
- `meta` - 可选，元数据
  - `source` - 事件来源
  - `correlationId` - 关联 ID（用于追踪事件链）
  - `priority` - 优先级

**返回**：
- 是否成功

:::tabs

== Python

```python:line-numbers
from wingman import event

# 触发事件
event.emit("combat.enemy_found", {"x": 100, "y": 200}, {"source": "vision"})

# 带关联 ID
event.emit("combat.enemy_found", enemy, {"correlationId": "session-123"})
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 触发事件
event.emit("combat.enemy_found", { x = 100, y = 200 }, { source = "vision" })

-- 带关联 ID
event.emit("combat.enemy_found", enemy, { correlationId = "session-123" })
```

:::

---

## 取消订阅

### off(subscription) / off(subscription)

**说明**：取消指定的订阅。

**函数签名**：

```python
off(subscription: str) -> bool
```

```lua
off(subscription: string) -> boolean
```

**参数**：
- `subscription` - 订阅 ID 或名称

**返回**：
- 是否成功

---

### clear() / clear()

**说明**：清空全部事件监听。

**函数签名**：

```python
clear() -> None
```

```lua
clear() -> nil
```

**返回**：
- 无

:::tabs

== Python

```python:line-numbers
from wingman import event

# 取消指定订阅
event.off(sub_id)

# 清空所有监听
event.clear()
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 取消指定订阅
event.off(id)

-- 清空所有监听
event.clear()
```

:::

---

## 构造事件对象

### message(type, payload?, meta?) / message(type, payload?, meta?)

**说明**：构造标准事件对象，供调试或测试使用。

**函数签名**：

```python
message(type: str, payload: Any = None, meta: dict = None) -> dict
```

```lua
message(type: string, payload: any = nil, meta: table = nil) -> table
```

**参数**：
- `type` - 事件名
- `payload` - 可选，事件载荷
- `meta` - 可选，元数据

**返回**：
- `EventMessage` 对象

---

## 事件对象结构

标准 `EventMessage` 对象包含以下字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| id | string | 事件唯一 ID |
| type | string | 事件类型名 |
| payload | any | 事件载荷 |
| source | string | 事件来源（可选） |
| correlationId | string | 关联 ID（可选） |
| timestamp | number | 时间戳（毫秒） |

---

## 跨进程事件（远程频道）

跨进程事件基于 Redis Stream 实现，用于多进程或多机器之间的事件通信。使用全局频道注册模式：先注册频道，然后随处获取。

:::tip 注意
跨进程事件需要 hiredis 库支持。如果编译时未检测到 hiredis，远程频道功能将不可用。
:::

### 注册频道

#### registerChannel(name, config) / register_channel(name, config)

**说明**：注册一个远程频道，连接到 Redis Stream 并启动后台监听线程。

**函数签名**：

```python
register_channel(name: str, config: dict) -> bool
```

```lua
registerChannel(name: string, config: table) -> boolean
```

**参数**：
- `name` - 频道名称（全局唯一标识符）
- `config` - 配置对象
  - `redis` - Redis 连接配置
    - `host` - Redis 服务器地址，默认 "localhost"
    - `port` - Redis 端口，默认 6379
  - `stream` - Stream 名称，默认 "<name>:events"
  - `consumerGroup` - 消费者组名称，默认 "wingman"
  - `pollInterval` - 轮询间隔（毫秒），默认 100
  - `blockingTimeout` - 阻塞超时（毫秒），默认 5000

**返回**：是否注册成功

:::tabs

== Python

```python:line-numbers
from wingman import event

# 注册远程频道
success = event.register_channel("game-events", {
    "redis": {
        "host": "localhost",
        "port": 6379
    },
    "stream": "game:events",
    "consumerGroup": "wingman"
})

if success:
    print("频道注册成功")
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 注册远程频道
local success = event.registerChannel("game-events", {
    redis = {
        host = "localhost",
        port = 6379
    },
    stream = "game:events",
    consumerGroup = "wingman"
})

if success then
    print("频道注册成功")
end
```

:::

---

### 获取频道

#### getChannel(name) / get_channel(name)

**说明**：获取已注册的频道对象。

**函数签名**：

```python
get_channel(name: str) -> dict
```

```lua
getChannel(name: string) -> table
```

**参数**：
- `name` - 频道名称

**返回**：
- `success` - 是否成功
- `name` - 频道名称（成功时）
- `error` - 错误信息（失败时）

---

### 订阅远程事件

#### channelOn(channelName, eventType, callback) / channel_on(channel_name, event_type, callback)

**说明**：订阅指定频道的远程事件。

**函数签名**：

```python
channel_on(channel_name: str, event_type: str, callback: Callable) -> bool
```

```lua
channelOn(channelName: string, eventType: string, callback: function) -> boolean
```

**参数**：
- `channelName` / `channel_name` - 频道名称
- `eventType` / `event_type` - 事件类型名
- `callback` - 回调函数，接收事件消息对象

**返回**：是否订阅成功

:::tabs

== Python

```python:line-numbers
from wingman import event

# 先注册频道
event.register_channel("game-events", {"redis": {"host": "localhost"}})

# 订阅远程事件
def on_player_join(data):
    print(f"玩家加入: {data}")

event.channel_on("game-events", "player.join", on_player_join)
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 先注册频道
event.registerChannel("game-events", { redis = { host = "localhost" } })

-- 订阅远程事件
local function onPlayerJoin(data)
    print("玩家加入:", data.payload)
end

event.channelOn("game-events", "player.join", onPlayerJoin)
```

:::

---

### 发布远程事件

#### channelEmit(channelName, eventType, payload?) / channel_emit(channel_name, event_type, payload?)

**说明**：向指定频道发布远程事件。

**函数签名**：

```python
channel_emit(channel_name: str, event_type: str, payload: Any = None) -> bool
```

```lua
channelEmit(channelName: string, eventType: string, payload: any) -> boolean
```

**参数**：
- `channelName` / `channel_name` - 频道名称
- `eventType` / `event_type` - 事件类型名
- `payload` - 可选，事件载荷

**返回**：是否发布成功

:::tabs

== Python

```python:line-numbers
from wingman import event

# 发布远程事件
event.channel_emit("game-events", "player.join", {
    "playerId": 123,
    "name": "Alice",
    "timestamp": 1234567890
})
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 发布远程事件
event.channelEmit("game-events", "player.join", {
    playerId = 123,
    name = "Alice",
    timestamp = 1234567890
})
```

:::

---

### 取消远程订阅

#### channelOff(channelName, eventType) / channel_off(channel_name, event_type)

**说明**：取消指定频道的远程事件订阅。

**函数签名**：

```python
channel_off(channel_name: str, event_type: str) -> bool
```

```lua
channelOff(channelName: string, eventType: string) -> boolean
```

**参数**：
- `channelName` / `channel_name` - 频道名称
- `eventType` / `event_type` - 事件类型名

**返回**：是否取消成功

:::tabs

== Python

```python:line-numbers
from wingman import event

# 取消订阅
event.channel_off("game-events", "player.join")
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 取消订阅
event.channelOff("game-events", "player.join")
```

:::

---

### 完整示例

#### 多进程事件通信

:::tabs

== Python

```python:line-numbers
from wingman import event
import time

# 进程 A：事件发布者
def producer():
    # 注册频道
    event.register_channel("game-events", {
        "redis": {"host": "localhost", "port": 6379}
    })
    
    # 发布事件
    for i in range(5):
        event.channel_emit("game-events", "game.tick", {"tick": i})
        time.sleep(1)

# 进程 B：事件消费者
def consumer():
    # 注册同一个频道
    event.register_channel("game-events", {
        "redis": {"host": "localhost", "port": 6379}
    })
    
    # 订阅事件
    def on_tick(data):
        print(f"收到游戏 tick: {data}")
    
    event.channel_on("game-events", "game.tick", on_tick)
    
    # 保持运行
    while True:
        time.sleep(1)
```

== Lua

```lua:line-numbers
local event = require("wingman.event")

-- 进程 A：事件发布者
local function producer()
    -- 注册频道
    event.registerChannel("game-events", {
        redis = { host = "localhost", port = 6379 }
    })
    
    -- 发布事件
    for i = 0, 4 do
        event.channelEmit("game-events", "game.tick", { tick = i })
        os.execute("sleep 1")
    end
end

-- 进程 B：事件消费者
local function consumer()
    -- 注册同一个频道
    event.registerChannel("game-events", {
        redis = { host = "localhost", port = 6379 }
    })
    
    -- 订阅事件
    local function onTick(data)
        print("收到游戏 tick:", data.payload.tick)
    end
    
    event.channelOn("game-events", "game.tick", onTick)
    
    -- 保持运行
    while true do
        os.execute("sleep 1")
    end
end
```

:::

---

## 可用接口

### 进程内事件

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `on(type, callback, name?)` | `on(type, callback, name?)` | 订阅事件 | type: 事件名<br>callback: 回调函数<br>name: 订阅名称(可选)<br>返回: 订阅 ID |
| `once(type, callback)` | `once(type, callback)` | 一次性订阅 | type: 事件名<br>callback: 回调函数<br>返回: 订阅 ID |
| `emit(type, payload?, meta?)` | `emit(type, payload?, meta?)` | 发布事件 | type: 事件名<br>payload: 载荷(可选)<br>meta: 元数据(可选)<br>返回: 是否成功 |
| `off(subscription)` | `off(subscription)` | 取消订阅 | subscription: 订阅 ID 或名称<br>返回: 是否成功 |
| `clear()` | `clear()` | 清空全部监听 | 无返回值 |
| `message(type, payload?, meta?)` | `message(type, payload?, meta?)` | 构造事件对象 | 返回: EventMessage 对象 |

### 跨进程事件（远程频道）

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `register_channel(name, config)` | `registerChannel(name, config)` | 注册远程频道 | name: 频道名<br>config: {redis?, stream?, consumerGroup?}<br>返回: 是否成功 |
| `get_channel(name)` | `getChannel(name)` | 获取频道 | name: 频道名<br>返回: {success, name?, error?} |
| `channel_on(channel_name, event_type, callback)` | `channelOn(channelName, eventType, callback)` | 订阅远程事件 | channelName: 频道名<br>eventType: 事件类型<br>callback: 回调函数<br>返回: 是否成功 |
| `channel_emit(channel_name, event_type, payload?)` | `channelEmit(channelName, eventType, payload?)` | 发布远程事件 | channelName: 频道名<br>eventType: 事件类型<br>payload: 载荷(可选)<br>返回: 是否成功 |
| `channel_off(channel_name, event_type)` | `channelOff(channelName, eventType)` | 取消远程订阅 | channelName: 频道名<br>eventType: 事件类型<br>返回: 是否成功 |
