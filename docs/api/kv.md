# API: wingman.kv

类 Redis 的键值存储，支持 String、Hash、List 操作。

## 字符串操作

### 设置键值

::: code-group

```python [Python]
from wingman import kv

# 设置键值
kv.set("token", "abc123", {"ttl": 3600})  # 1小时后过期

# 仅当键不存在时设置
kv.set("counter", "1", {"nx": True})
```

```lua [Lua]
local kv = require("wingman.kv")

-- 设置键值
kv.set("token", "abc123", {ttl = 3600})  -- 1小时后过期

-- 仅当键不存在时设置
kv.set("counter", "1", {nx = true})
```

:::

### 获取值

::: code-group

```python [Python]
value = kv.get("token")
```

```lua [Lua]
local value = kv.get("token")
```

:::

### 删除键

::: code-group

```python [Python]
# 删除单个键
kv.delete("token")

# 批量删除
kv.delete(["key1", "key2", "key3"])
```

```lua [Lua]
-- 删除单个键
kv.delete("token")

-- 批量删除
kv.delete({"key1", "key2", "key3"})
```

:::

### 检查键是否存在

::: code-group

```python [Python]
exists = kv.exists("token")
```

```lua [Lua]
local exists = kv.exists("token")
```

:::

### 获取过期时间

::: code-group

```python [Python]
# 返回剩余秒数，-1 表示无过期，-2 表示已过期
ttl = kv.ttl("token")
```

```lua [Lua]
-- 返回剩余秒数，-1 表示无过期，-2 表示已过期
local ttl = kv.ttl("token")
```

:::

### 自增

::: code-group

```python [Python]
kv.set("counter", "10")
new_value = kv.incr("counter", 5)  # 返回 15
```

```lua [Lua]
kv.set("counter", "10")
local new = kv.incr("counter", 5)  -- 返回 15
```

:::

## Hash 操作

### 设置字段

::: code-group

```python [Python]
from wingman import kv

kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

```lua [Lua]
local kv = require("wingman.kv")

kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

:::

### 获取字段

::: code-group

```python [Python]
value = kv.hget("team:123", "leader")
```

```lua [Lua]
local value = kv.hget("team:123", "leader")
```

:::

### 获取所有字段

::: code-group

```python [Python]
info = kv.hgetall("team:123")
print(info["leader"])  # "PlayerA"
print(info["state"])   # "ready"
```

```lua [Lua]
local info = kv.hgetall("team:123")
print(info.leader)  -- "PlayerA"
print(info.state)   -- "ready"
```

:::

### 删除字段

::: code-group

```python [Python]
kv.hdel("team:123", "leader")
```

```lua [Lua]
kv.hdel("team:123", "leader")
```

:::

### 检查字段是否存在

::: code-group

```python [Python]
exists = kv.hexists("team:123", "leader")
```

```lua [Lua]
local exists = kv.hexists("team:123", "leader")
```

:::

### 获取所有字段名

::: code-group

```python [Python]
keys = kv.hkeys("team:123")
```

```lua [Lua]
local keys = kv.hkeys("team:123")
```

:::

## List 操作

### 推入元素

::: code-group

```python [Python]
from wingman import kv

# 左端推入
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")

# 右端推入
kv.rpush("log", "warning: high latency")
```

```lua [Lua]
local kv = require("wingman.kv")

-- 左端推入
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")

-- 右端推入
kv.rpush("log", "warning: high latency")
```

:::

### 弹出元素

::: code-group

```python [Python]
# 左端弹出
value = kv.lpop("log")

# 右端弹出
value = kv.rpop("log")
```

```lua [Lua]
-- 左端弹出
local value = kv.lpop("log")

-- 右端弹出
local value = kv.rpop("log")
```

:::

### 获取列表长度

::: code-group

```python [Python]
length = kv.llen("log")
```

```lua [Lua]
local length = kv.llen("log")
```

:::

### 获取范围元素

::: code-group

```python [Python]
# 获取全部
logs = kv.lrange("log", 0, -1)

# 获取前10条
recent = kv.lrange("log", 0, 9)
```

```lua [Lua]
-- 获取全部
local logs = kv.lrange("log", 0, -1)

-- 获取前10条
local recent = kv.lrange("log", 0, 9)
```

:::

---

## 持久化

KV 存储支持持久化到 SQLite（C++ 层调用）：

::: code-group

```python [Python]
# 这些函数在 C++ 层调用
kv.save("data.db")           # 保存
kv.load("data.db")           # 加载
kv.enable_auto_save("data.db", 60)  # 每60秒自动保存
```

```lua [Lua]
-- 这些函数在 C++ 层调用
kv.save("data.db")           -- 保存
kv.load("data.db")           -- 加载
kv.enableAutoSave("data.db", 60)  -- 每60秒自动保存
```

:::
