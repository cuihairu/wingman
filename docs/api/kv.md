# API: wingman.kv

类 Redis 的键值存储，支持 String、Hash、List 操作。

## 字符串操作

### 设置键值

:::tabs

== Python

```python
from wingman import kv

# 设置键值
kv.set("token", "abc123", {"ttl": 3600})  # 1小时后过期

# 仅当键不存在时设置
kv.set("counter", "1", {"nx": True})
```

== Lua

```lua
local kv = require("wingman.kv")

-- 设置键值
kv.set("token", "abc123", {ttl = 3600})  -- 1小时后过期

-- 仅当键不存在时设置
kv.set("counter", "1", {nx = true})
```

:::

### 获取值

:::tabs

== Python

```python
value = kv.get("token")
```

== Lua

```lua
local value = kv.get("token")
```

:::

### 删除键

:::tabs

== Python

```python
# 删除单个键
kv.delete("token")

# 批量删除
kv.delete(["key1", "key2", "key3"])
```

== Lua

```lua
-- 删除单个键
kv.delete("token")

-- 批量删除
kv.delete({"key1", "key2", "key3"})
```

:::

### 检查键是否存在

:::tabs

== Python

```python
exists = kv.exists("token")
```

== Lua

```lua
local exists = kv.exists("token")
```

:::

### 获取过期时间

:::tabs

== Python

```python
# 返回剩余秒数，-1 表示无过期，-2 表示已过期
ttl = kv.ttl("token")
```

== Lua

```lua
-- 返回剩余秒数，-1 表示无过期，-2 表示已过期
local ttl = kv.ttl("token")
```

:::

### 自增

:::tabs

== Python

```python
kv.set("counter", "10")
new_value = kv.incr("counter", 5)  # 返回 15
```

== Lua

```lua
kv.set("counter", "10")
local new = kv.incr("counter", 5)  -- 返回 15
```

:::

## Hash 操作

### 设置字段

:::tabs

== Python

```python
from wingman import kv

kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

== Lua

```lua
local kv = require("wingman.kv")

kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

:::

### 获取字段

:::tabs

== Python

```python
value = kv.hget("team:123", "leader")
```

== Lua

```lua
local value = kv.hget("team:123", "leader")
```

:::

### 获取所有字段

:::tabs

== Python

```python
info = kv.hgetall("team:123")
print(info["leader"])  # "PlayerA"
print(info["state"])   # "ready"
```

== Lua

```lua
local info = kv.hgetall("team:123")
print(info.leader)  -- "PlayerA"
print(info.state)   -- "ready"
```

:::

### 删除字段

:::tabs

== Python

```python
kv.hdel("team:123", "leader")
```

== Lua

```lua
kv.hdel("team:123", "leader")
```

:::

### 检查字段是否存在

:::tabs

== Python

```python
exists = kv.hexists("team:123", "leader")
```

== Lua

```lua
local exists = kv.hexists("team:123", "leader")
```

:::

### 获取所有字段名

:::tabs

== Python

```python
keys = kv.hkeys("team:123")
```

== Lua

```lua
local keys = kv.hkeys("team:123")
```

:::

## List 操作

### 推入元素

:::tabs

== Python

```python
from wingman import kv

# 左端推入
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")

# 右端推入
kv.rpush("log", "warning: high latency")
```

== Lua

```lua
local kv = require("wingman.kv")

-- 左端推入
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")

-- 右端推入
kv.rpush("log", "warning: high latency")
```

:::

### 弹出元素

:::tabs

== Python

```python
# 左端弹出
value = kv.lpop("log")

# 右端弹出
value = kv.rpop("log")
```

== Lua

```lua
-- 左端弹出
local value = kv.lpop("log")

-- 右端弹出
local value = kv.rpop("log")
```

:::

### 获取列表长度

:::tabs

== Python

```python
length = kv.llen("log")
```

== Lua

```lua
local length = kv.llen("log")
```

:::

### 获取范围元素

:::tabs

== Python

```python
# 获取全部
logs = kv.lrange("log", 0, -1)

# 获取前10条
recent = kv.lrange("log", 0, 9)
```

== Lua

```lua
-- 获取全部
local logs = kv.lrange("log", 0, -1)

-- 获取前10条
local recent = kv.lrange("log", 0, 9)
```

:::

---

## 持久化

KV 存储支持持久化到 SQLite（C++ 层调用）：

:::tabs

== Python

```python
# 这些函数在 C++ 层调用
kv.save("data.db")           # 保存
kv.load("data.db")           # 加载
kv.enable_auto_save("data.db", 60)  # 每60秒自动保存
```

== Lua

```lua
-- 这些函数在 C++ 层调用
kv.save("data.db")           -- 保存
kv.load("data.db")           -- 加载
kv.enableAutoSave("data.db", 60)  -- 每60秒自动保存
```

:::
