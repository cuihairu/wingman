# API: wingman.kv

持久化键值存储模块，提供类 Redis 的数据存储功能，支持 String、Hash、List 等数据结构。

## 模块概述

kv 模块提供了完整的键值存储功能，用于在脚本运行期间保存和读取数据。主要功能包括：

- **String 操作**：存储和获取字符串值，支持过期时间
- **Hash 操作**：存储键值对集合
- **List 操作**：存储有序列表
- **持久化**：支持将数据保存到文件，下次运行时加载

### 使用场景

- **状态保存**：保存脚本运行状态，支持中断后恢复
- **数据缓存**：缓存计算结果，避免重复计算
- **队列管理**：使用 List 实现任务队列
- **配置存储**：存储用户配置或偏好设置

---

## String 操作

### set(key, value, options?)

**说明**：设置键值对。支持设置过期时间（TTL）和条件设置。

**函数签名**：

```python
set(key: str, value: str, options: dict = None) -> None
```

```lua
set(key: string, value: string, options: table | nil) -> nil
```

**参数**：
- `key` - 键名
- `value` - 键值（字符串）
- `options` - 可选，设置选项：
  - `ttl` - 过期时间（秒），0 表示永不过期
  - `nx` - 仅当键不存在时设置（Python: bool, Lua: boolean）

**使用场景**：
- 临时数据存储（设置 TTL）
- 避免覆盖已有数据（使用 nx 选项）

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 基本设置
kv.set("token", "abc123")

# 设置过期时间（1小时后过期）
kv.set("token", "abc123", {"ttl": 3600})

# 仅当键不存在时设置
success = kv.set("counter", "1", {"nx": True})
if success:
    print("counter 键已创建")
else:
    print("counter 键已存在")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 基本设置
kv.set("token", "abc123")

-- 设置过期时间（1小时后过期）
kv.set("token", "abc123", {ttl = 3600})

-- 仅当键不存在时设置
local success = kv.set("counter", "1", {nx = true})
if success then
    print("counter 键已创建")
else
    print("counter 键已存在")
end
```

:::

---

### get(key)

**说明**：获取键对应的值。

**函数签名**：

```python
get(key: str) -> str | None
```

```lua
get(key: string) -> string | nil
```

**参数**：
- `key` - 键名

**返回**：键对应的值，键不存在时返回 None/nil

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 获取值
value = kv.get("token")
if value:
    print(f"Token: {value}")
else:
    print("Token 不存在")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 获取值
local value = kv.get("token")
if value then
    print("Token:", value)
else
    print("Token 不存在")
end
```

:::

---

### delete(keys)

**说明**：删除一个或多个键。

**函数签名**：

```python
delete(keys: str | list[str]) -> int
```

```lua
delete(keys: string | table) -> number
```

**参数**：
- `keys` - 键名或键名列表

**返回**：删除的键数量

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 删除单个键
kv.delete("token")

# 批量删除
count = kv.delete(["key1", "key2", "key3"])
print(f"删除了 {count} 个键")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 删除单个键
kv.delete("token")

-- 批量删除
local count = kv.delete({"key1", "key2", "key3"})
print("删除了 " .. count .. " 个键")
```

:::

---

### exists(key)

**说明**：检查键是否存在。

**函数签名**：

```python
exists(key: str) -> bool
```

```lua
exists(key: string) -> boolean
```

**返回**：键存在返回 true，否则返回 false

:::tabs

== Python

```python:line-numbers
from wingman import kv

if kv.exists("token"):
    print("Token 已存在")
else:
    print("Token 不存在，需要重新获取")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

if kv.exists("token") then
    print("Token 已存在")
else
    print("Token 不存在，需要重新获取")
end
```

:::

---

### ttl(key)

**说明**：获取键的剩余过期时间。

**函数签名**：

```python
ttl(key: str) -> int
```

```lua
ttl(key: string) -> number
```

**返回**：
- `> 0` - 剩余秒数
- `-1` - 键存在但无过期时间
- `-2` - 键不存在

:::tabs

== Python

```python:line-numbers
from wingman import kv

remaining = kv.ttl("token")
if remaining > 0:
    print(f"Token 将在 {remaining} 秒后过期")
elif remaining == -1:
    print("Token 永不过期")
else:
    print("Token 不存在")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

local remaining = kv.ttl("token")
if remaining > 0 then
    print("Token 将在 " .. remaining .. " 秒后过期")
elseif remaining == -1 then
    print("Token 永不过期")
else
    print("Token 不存在")
end
```

:::

---

### incr(key, delta?)

**说明**：将键对应的数字值增加指定量。如果键不存在，会先创建并设置为 0。

**函数签名**：

```python
incr(key: str, delta: int = 1) -> int
```

```lua
incr(key: string, delta: number = 1) -> number
```

**参数**：
- `key` - 键名
- `delta` - 可选，增加量，默认 1

**返回**：增加后的值

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 初始化计数器
kv.set("counter", "10")

# 增加计数
new_value = kv.incr("counter")      # 返回 11
new_value = kv.incr("counter", 5)   # 返回 16

# 减少计数（使用负数）
new_value = kv.incr("counter", -3)  # 返回 13
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 初始化计数器
kv.set("counter", "10")

-- 增加计数
local new = kv.incr("counter")      -- 返回 11
local new = kv.incr("counter", 5)   -- 返回 16

-- 减少计数（使用负数）
local new = kv.incr("counter", -3)  -- 返回 13
```

:::

---

## Hash 操作

### hset(hash, field, value)

**说明**：在 Hash 中设置字段值。

**函数签名**：

```python
hset(hash: str, field: str, value: str) -> None
```

```lua
hset(hash: string, field: string, value: string) -> nil
```

**参数**：
- `hash` - Hash 键名
- `field` - 字段名
- `value` - 字段值

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 存储团队信息
kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
kv.hset("team:123", "member_count", "4")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 存储团队信息
kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
kv.hset("team:123", "member_count", "4")
```

:::

---

### hget(hash, field)

**说明**：获取 Hash 中指定字段的值。

**函数签名**：

```python
hget(hash: str, field: str) -> str | None
```

```lua
hget(hash: string, field: string) -> string | nil
```

:::tabs

== Python

```python:line-numbers
from wingman import kv

leader = kv.hget("team:123", "leader")
if leader:
    print(f"队长: {leader}")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

local leader = kv.hget("team:123", "leader")
if leader then
    print("队长:", leader)
end
```

:::

---

### hgetall(hash)

**说明**：获取 Hash 中所有字段和值。

**函数签名**：

```python
hgetall(hash: str) -> dict
```

```lua
hgetall(hash: string) -> table
```

**返回**：包含所有字段和值的字典/表格

:::tabs

== Python

```python:line-numbers
from wingman import kv

info = kv.hgetall("team:123")
print(f"队长: {info['leader']}")
print(f"状态: {info['state']}")
print(f"成员数: {info['member_count']}")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

local info = kv.hgetall("team:123")
print("队长:", info.leader)
print("状态:", info.state)
print("成员数:", info.member_count)
```

:::

---

### hdel(hash, field)

**说明**：删除 Hash 中的字段。

**函数签名**：

```python
hdel(hash: str, field: str) -> bool
```

```lua
hdel(hash: string, field: string) -> boolean
```

**返回**：字段存在并删除返回 true，否则返回 false

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 删除字段
if kv.hdel("team:123", "leader"):
    print("队长字段已删除")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 删除字段
if kv.hdel("team:123", "leader") then
    print("队长字段已删除")
end
```

:::

---

### hexists(hash, field)

**说明**：检查 Hash 中的字段是否存在。

**函数签名**：

```python
hexists(hash: str, field: str) -> bool
```

```lua
hexists(hash: string, field: string) -> boolean
```

:::tabs

== Python

```python:line-numbers
from wingman import kv

if kv.hexists("team:123", "leader"):
    print("团队已有队长")
else:
    print("需要设置队长")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

if kv.hexists("team:123", "leader") then
    print("团队已有队长")
else
    print("需要设置队长")
end
```

:::

---

### hkeys(hash)

**说明**：获取 Hash 中所有字段名。

**函数签名**：

```python
hkeys(hash: str) -> list[str]
```

```lua
hkeys(hash: string) -> table
```

**返回**：字段名列表

:::tabs

== Python

```python:line-numbers
from wingman import kv

fields = kv.hkeys("team:123")
print(f"团队字段: {', '.join(fields)}")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

local fields = kv.hkeys("team:123")
print("团队字段:", table.concat(fields, ", "))
```

:::

---

## List 操作

### lpush(list, value)

**说明**：在列表左端（头部）推入元素。

**函数签名**：

```python
lpush(list: str, value: str) -> int
```

```lua
lpush(list: string, value: string) -> number
```

**返回**：列表长度

**使用场景**：
- 日志记录（最新的在前面）
- 任务队列（后进先出）

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 添加日志条目
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")
kv.lpush("log", "info: connected")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 添加日志条目
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")
kv.lpush("log", "info: connected")
```

:::

---

### rpush(list, value)

**说明**：在列表右端（尾部）推入元素。

**函数签名**：

```python
rpush(list: str, value: str) -> int
```

```lua
rpush(list: string, value: string) -> number
```

**使用场景**：
- 消息队列（先进先出）
- 事件记录（按时间顺序）

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 添加事件（按时间顺序）
kv.rpush("events", "login")
kv.rpush("events", "action")
kv.rpush("events", "logout")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 添加事件（按时间顺序）
kv.rpush("events", "login")
kv.rpush("events", "action")
kv.rpush("events", "logout")
```

:::

---

### lpop(list)

**说明**：从列表左端（头部）弹出元素。

**函数签名**：

```python
lpop(list: str) -> str | None
```

```lua
lpop(list: string) -> string | nil
```

**返回**：弹出的元素，列表为空时返回 None/nil

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 处理日志（最新的先处理）
log_entry = kv.lpop("log")
if log_entry:
    print(f"处理日志: {log_entry}")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 处理日志（最新的先处理）
local logEntry = kv.lpop("log")
if logEntry then
    print("处理日志:", logEntry)
end
```

:::

---

### rpop(list)

**说明**：从列表右端（尾部）弹出元素。

**函数签名**：

```python
rpop(list: str) -> str | None
```

```lua
rpop(list: string) -> string | nil
```

**返回**：弹出的元素，列表为空时返回 None/nil

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 处理事件（按时间顺序）
event = kv.rpop("events")
if event:
    print(f"处理事件: {event}")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 处理事件（按时间顺序）
local event = kv.rpop("events")
if event then
    print("处理事件:", event)
end
```

:::

---

### llen(list)

**说明**：获取列表长度。

**函数签名**：

```python
llen(list: str) -> int
```

```lua
llen(list: string) -> number
```

:::tabs

== Python

```python:line-numbers
from wingman import kv

length = kv.llen("log")
print(f"日志条数: {length}")
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

local length = kv.llen("log")
print("日志条数:", length)
```

:::

---

### lrange(list, start, stop)

**说明**：获取列表中指定范围的元素。

**函数签名**：

```python
lrange(list: str, start: int, stop: int) -> list[str]
```

```lua
lrange(list: string, start: number, stop: number) -> table
```

**参数**：
- `start` - 起始索引（0 表示第一个元素）
- `stop` - 结束索引（-1 表示最后一个元素）

**返回**：元素列表

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 获取全部日志
all_logs = kv.lrange("log", 0, -1)

# 获取前 10 条
recent = kv.lrange("log", 0, 9)

# 获取最新 5 条（从末尾往前）
latest = kv.lrange("log", -5, -1)
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 获取全部日志
local allLogs = kv.lrange("log", 0, -1)

-- 获取前 10 条
local recent = kv.lrange("log", 0, 9)

-- 获取最新 5 条（从末尾往前）
local latest = kv.lrange("log", -5, -1)
```

:::

---

## 持久化

### save(file_path) / save(filePath)

**说明**：将当前所有数据保存到文件。

**函数签名**：

```python
save(file_path: str) -> bool
```

```lua
save(filePath: string) -> boolean
```

**参数**：
- `file_path` / `filePath` - 保存文件路径

---

### load(file_path) / load(filePath)

**说明**：从文件加载数据。

**函数签名**：

```python
load(file_path: str) -> bool
```

```lua
load(filePath: string) -> boolean
```

---

### enable_auto_save(file_path, interval) / enableAutoSave(filePath, interval)

**说明**：启用自动保存，每隔指定秒数自动保存数据。

**函数签名**：

```python
enable_auto_save(file_path: str, interval: int) -> None
```

```lua
enableAutoSave(filePath: string, interval: number) -> nil
```

**参数**：
- `file_path` / `filePath` - 保存文件路径
- `interval` - 保存间隔（秒）

:::tabs

== Python

```python:line-numbers
from wingman import kv

# 手动保存
kv.save("data.db")

# 手动加载
kv.load("data.db")

# 启用自动保存（每 60 秒保存一次）
kv.enable_auto_save("data.db", 60)
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

-- 手动保存
kv.save("data.db")

-- 手动加载
kv.load("data.db")

-- 启用自动保存（每 60 秒保存一次）
kv.enableAutoSave("data.db", 60)
```

:::

---

## 完整示例

### 任务队列管理

这个示例展示了如何使用 List 实现一个简单的任务队列：

:::tabs

== Python

```python:line-numbers
from wingman import kv

class TaskQueue:
    """任务队列示例"""

    def __init__(self, name):
        self.queue_name = f"queue:{name}"

    def add_task(self, task):
        """添加任务到队列"""
        kv.rpush(self.queue_name, task)
        print(f"已添加任务: {task}")

    def get_task(self):
        """从队列获取任务"""
        task = kv.lpop(self.queue_name)
        if task:
            print(f"执行任务: {task}")
        return task

    def get_pending_count(self):
        """获取待处理任务数"""
        return kv.llen(self.queue_name)

# 使用示例
queue = TaskQueue("tasks")

# 添加任务
queue.add_task("task1")
queue.add_task("task2")
queue.add_task("task3")

print(f"待处理任务: {queue.get_pending_count()}")

# 处理任务
while True:
    task = queue.get_task()
    if not task:
        break
    # 执行任务...
```

== Lua

```lua:line-numbers
local kv = require("wingman.kv")

local TaskQueue = {}
TaskQueue.__index = TaskQueue

function TaskQueue.new(name)
    local self = setmetatable({}, TaskQueue)
    self.queueName = "queue:" .. name
    return self
end

function TaskQueue:addTask(task)
    kv.rpush(self.queueName, task)
    print("已添加任务:", task)
end

function TaskQueue:getTask()
    local task = kv.lpop(self.queueName)
    if task then
        print("执行任务:", task)
    end
    return task
end

function TaskQueue:getPendingCount()
    return kv.llen(self.queueName)
end

-- 使用示例
local queue = TaskQueue.new("tasks")

-- 添加任务
queue:addTask("task1")
queue:addTask("task2")
queue:addTask("task3")

print("待处理任务:", queue:getPendingCount())

-- 处理任务
while true do
    local task = queue:getTask()
    if not task then
        break
    end
    -- 执行任务...
end
```

:::

---

## 可用接口

### String 操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `set(key, val, opts?)` | `set(key, val, opts?)` | 设置键值 | key: 键名<br>val: 值<br>opts: {ttl, nx} |
| `get(key)` | `get(key)` | 获取值 | key: 键名 |
| `delete(keys)` | `delete(keys)` | 删除键 | keys: 键名或列表 |
| `exists(key)` | `exists(key)` | 检查键是否存在 | key: 键名 |
| `ttl(key)` | `ttl(key)` | 获取过期时间 | key: 键名<br>返回: 剩余秒数 |
| `incr(key, delta?)` | `incr(key, delta?)` | 数字自增 | key: 键名<br>delta: 增量(默认1) |

### Hash 操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `hset(hash, field, val)` | `hset(hash, field, val)` | 设置字段 | hash: Hash名<br>field: 字段名<br>val: 值 |
| `hget(hash, field)` | `hget(hash, field)` | 获取字段 | hash: Hash名<br>field: 字段名 |
| `hgetall(hash)` | `hgetall(hash)` | 获取全部 | hash: Hash名 |
| `hdel(hash, field)` | `hdel(hash, field)` | 删除字段 | hash: Hash名<br>field: 字段名 |
| `hexists(hash, field)` | `hexists(hash, field)` | 检查字段 | hash: Hash名<br>field: 字段名 |
| `hkeys(hash)` | `hkeys(hash)` | 获取所有字段名 | hash: Hash名 |

### List 操作

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `lpush(list, val)` | `lpush(list, val)` | 左端推入 | list: 列表名<br>val: 值 |
| `rpush(list, val)` | `rpush(list, val)` | 右端推入 | list: 列表名<br>val: 值 |
| `lpop(list)` | `lpop(list)` | 左端弹出 | list: 列表名 |
| `rpop(list)` | `rpop(list)` | 右端弹出 | list: 列表名 |
| `llen(list)` | `llen(list)` | 列表长度 | list: 列表名 |
| `lrange(list, start, stop)` | `lrange(list, start, stop)` | 获取范围 | list: 列表名<br>start: 起始<br>stop: 结束 |

### 持久化

| Python 函数 | Lua 函数 | 说明 | 参数 |
|------------|---------|------|-----|
| `save(path)` | `save(path)` | 保存到文件 | path: 文件路径 |
| `load(path)` | `load(path)` | 从文件加载 | path: 文件路径 |
| `enable_auto_save(path, interval)` | `enableAutoSave(path, interval)` | 自动保存 | path: 文件路径<br>interval: 间隔(秒) |
