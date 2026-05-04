# KV 模块

类 Redis 的键值存储，支持 String、Hash、List 操作。

## Lua API

### 字符串操作

#### kv.set(key, value, options)

设置键值。

**参数：**
- `key` (string) - 键名
- `value` (string) - 值
- `options` (table, 可选)
  - `ttl` (number) - 过期时间（秒）
  - `nx` (boolean) - 仅当键不存在时设置
  - `xx` (boolean) - 仅当键存在时设置

**示例：**
```lua
kv.set("token", "abc123", {ttl = 3600})  -- 1小时后过期
kv.set("counter", "1", {nx = true})      -- 仅当不存在时设置
```

#### kv.get(key)

获取值。

**返回：**
- `value` (string) - 值，不存在返回空字符串

#### kv.del(key)

删除键。

```lua
kv.del("token")
kv.del({"key1", "key2", "key3"})  -- 批量删除
```

#### kv.exists(key)

检查键是否存在。

**返回：**
- `boolean` - 是否存在

#### kv.ttl(key)

获取剩余过期时间。

**返回：**
- `number` - 剩余秒数，-1 表示无过期，-2 表示已过期

#### kv.incr(key, delta)

自增。

**参数：**
- `key` (string) - 键名
- `delta` (number, 可选) - 增量，默认 1

**返回：**
- `number` - 新值

```lua
kv.set("counter", "10")
local new = kv.incr("counter", 5)  -- 返回 15
```

### Hash 操作

#### kv.hset(hash, field, value)

设置 hash 字段。

```lua
kv.hset("team:123", "leader", "PlayerA")
kv.hset("team:123", "state", "ready")
```

#### kv.hget(hash, field)

获取 hash 字段值。

#### kv.hgetall(hash)

获取所有字段。

**返回：**
- `table` - 字段键值对

```lua
local info = kv.hgetall("team:123")
print(info.leader)  -- "PlayerA"
print(info.state)   -- "ready"
```

#### kv.hdel(hash, field)

删除 hash 字段。

#### kv.hexists(hash, field)

检查字段是否存在。

#### kv.hkeys(hash)

获取所有字段名。

### List 操作

#### kv.lpush(list, value)

左端推入元素。

```lua
kv.lpush("log", "error: connection failed")
kv.lpush("log", "info: retrying...")
```

#### kv.rpush(list, value)

右端推入元素。

#### kv.lpop(list)

左端弹出元素。

#### kv.rpop(list)

右端弹出元素。

#### kv.llen(list)

获取列表长度。

#### kv.lrange(list, start, stop)

获取范围元素。

**参数：**
- `start` (number) - 起始索引，0 表示开头，-1 表示末尾
- `stop` (number) - 结束索引

```lua
local logs = kv.lrange("log", 0, -1)  -- 获取全部
local recent = kv.lrange("log", 0, 9)  -- 获取前10条
```

## 持久化

KV 存储支持持久化到 SQLite：

```lua
-- C++ 中调用
kv.save("data.db")           -- 保存
kv.load("data.db")           -- 加载
kv.enableAutoSave("data.db", 60)  -- 每60秒自动保存
```
