# API: wingman.kv

类 Redis 的键值存储，支持 String、Hash、List 操作。

## 字符串操作

### 设置键值

<CodeTabs>

:::slot python

```python
from wingman import kv

# 设置键值
kv.set("token", "abc123", {"ttl": 3600})  # 1小时后过期

# 仅当键不存在时设置
kv.set("counter", "1", {"nx": True})
```

:::

:::slot lua

```lua
local kv = require("wingman.kv")

-- 设置键值
kv.set("token", "abc123", {ttl = 3600})  -- 1小时后过期

-- 仅当键不存在时设置
kv.set("counter", "1", {nx = true})
```

:::

</CodeTabs>

### 获取值

<CodeTabs>

:::slot python

```python
value = kv.get("token")
```

:::

:::slot lua

```lua
local value = kv.get("token")
```

:::

</CodeTabs>

### 删除键

<CodeTabs>

:::slot python

```python
# 删除单个键
kv.delete("token")

# 批量删除
kv.delete(["key1", "key2", "key3"])
```

:::

:::slot lua

```lua
-- 删除单个键
kv.delete("token")

-- 批量删除
kv.delete({"key1", "key2", "key3"})
```

:::

</CodeTabs>

### 检查键是否存在

<CodeTabs>

:::slot python

```python
exists = kv.exists("token")
```

:::

:::slot lua

```lua
local exists = kv.exists("token")
```

:::

</CodeTabs>

### 获取过期时间

<CodeTabs>

:::slot python

```python
# 返回剩余秒数，-1 表示无过期，-2 表示已过期
ttl = kv.ttl("token")
```

:::

:::slot lua

```lua
-- 返回剩余秒数，-1 表示无过期，-2 表示已过期
local ttl = kv.ttl("token")
```

:::

</CodeTabs>

### 自增

<CodeTabs>

:::slot python

```python
kv.set("counter", "10")
new_value = kv.incr("counter", 5)  # 返回 15
```

:::

:::slot lua

```lua
kv.set("counter", "10")
local new = kv.incr("counter", 5)  -- 返回 15
```

:::

</CodeTabs>

## Hash 操作

### 设置字段

<CodeTabs>

:::slot python

```python
from wingman import kv

kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

:::

:::slot lua

```lua
local kv = require("wingman.kv")

kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

:::

</CodeTabs>

### 获取字段

<CodeTabs>

:::slot python

```python
value = kv.hget("team:123", "leader")
```

:::

:::slot lua

```lua
local value = kv.hget("team:123", "leader")
```

:::

</CodeTabs>

### 获取所有字段

<CodeTabs>

:::slot python

```python
info = kv.hgetall("team:123")
print(info["leader"])  # "PlayerA"
print(info["state"])   # "ready"
```

:::

:::slot lua

```lua
local info = kv.hgetall("team:123")
print(info.leader)  -- "PlayerA"
print(info.state)   -- "ready"
```

:::

</CodeTabs>

### 删除字段

<CodeTabs>

:::slot python

```python
kv.hdel("team:123", "leader")
```

:::

:::slot lua

```lua
kv.hdel("team:123", "leader")
```

:::

</CodeTabs>

### 检查字段是否存在

<CodeTabs>

:::slot python

```python
exists = kv.hexists("team:123", "leader")
```

:::

:::slot lua

```lua
local exists = kv.hexists("team:123", "leader")
```

:::

</CodeTabs>

### 获取所有字段名

<CodeTabs>

:::slot python

```python
keys = kv.hkeys("team:123")
```

:::

:::slot lua

```lua
local keys = kv.hkeys("team:123")
```

:::

</CodeTabs>

## List 操作

### 推入元素

<CodeTabs>

:::slot python

```python
from wingman import kv

# 左端推入
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")

# 右端推入
kv.rpush("log", "warning: high latency")
```

:::

:::slot lua

```lua
local kv = require("wingman.kv")

-- 左端推入
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")

-- 右端推入
kv.rpush("log", "warning: high latency")
```

:::

</CodeTabs>

### 弹出元素

<CodeTabs>

:::slot python

```python
# 左端弹出
value = kv.lpop("log")

# 右端弹出
value = kv.rpop("log")
```

:::

:::slot lua

```lua
-- 左端弹出
local value = kv.lpop("log")

-- 右端弹出
local value = kv.rpop("log")
```

:::

</CodeTabs>

### 获取列表长度

<CodeTabs>

:::slot python

```python
length = kv.llen("log")
```

:::

:::slot lua

```lua
local length = kv.llen("log")
```

:::

</CodeTabs>

### 获取范围元素

<CodeTabs>

:::slot python

```python
# 获取全部
logs = kv.lrange("log", 0, -1)

# 获取前10条
recent = kv.lrange("log", 0, 9)
```

:::

:::slot lua

```lua
-- 获取全部
local logs = kv.lrange("log", 0, -1)

-- 获取前10条
local recent = kv.lrange("log", 0, 9)
```

:::

</CodeTabs>

---

## 持久化

KV 存储支持持久化到 SQLite（C++ 层调用）：

<CodeTabs>

:::slot python

```python
# 这些函数在 C++ 层调用
kv.save("data.db")           # 保存
kv.load("data.db")           # 加载
kv.enable_auto_save("data.db", 60)  # 每60秒自动保存
```

:::

:::slot lua

```lua
-- 这些函数在 C++ 层调用
kv.save("data.db")           -- 保存
kv.load("data.db")           -- 加载
kv.enableAutoSave("data.db", 60)  -- 每60秒自动保存
```

:::

</CodeTabs>
