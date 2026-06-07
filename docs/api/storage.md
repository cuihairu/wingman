# 数据持久化 API

Wingman 提供了完整的数据持久化解决方案，包括键值存储和关系型数据库支持。

## 📋 目录

- [概述](#概述)
- [键值存储 (kv)](#键值存储-kv)
- [SQLite 数据库 (db)](#sqlite-数据库-db)
- [使用示例](#使用示例)
- [最佳实践](#最佳实践)

## 概述

### 可用的存储方式

| 方式 | 适用场景 | 模块 |
|------|----------|------|
| **kv 键值存储** | 简单配置、缓存、状态保存 | `kv` 模块 |
| **SQLite 数据库** | 结构化数据、复杂查询、事务 | `db` 模块 |

### 数据位置

所有数据文件都存储在脚本数据目录中：

- **Windows**: `%APPDATA%\wingman\data\`
- **macOS**: `~/Library/Application Support/wingman/data/`
- **Linux**: `~/.local/share/wingman/data/`

## 键值存储 (kv)

键值存储提供简单持久化的字符串键值对。

### Lua API

```lua
local kv = require("wingman.kv")

-- 设置键值
kv.set("username", "player1")
kv.set("level", "10")
kv.set("coins", "9999")

-- 获取值
local username = kv.get("username")      -- "player1"
local level = kv.get("level")            -- "10"
local coins = kv.get("coins")            -- "9999"

-- 检查键是否存在
local hasCoins = kv.has("coins")          -- true
local hasGems = kv.has("gems")            -- false

-- 删除键
kv.delete("coins")

-- 获取所有键
local keys = kv.keys()                    -- {"username", "level"}

-- 清空所有数据
kv.clear()
```

### Python API

```python
from wingman import kv

# 设置键值
kv.set("username", "player1")
kv.set("level", "10")
kv.set("coins", "9999")

# 获取值
username = kv.get("username")  # "player1"
level = kv.get("level")        # "10"
coins = kv.get("coins")        # "9999"

# 检查键是否存在
has_coins = kv.has("coins")    # True
has_gems = kv.has("gems")      # False

# 删除键
kv.delete("coins")

# 获取所有键
keys = kv.keys()               # ["username", "level"]

# 清空所有数据
kv.clear()
```

## SQLite 数据库 (db)

数据库模块提供完整的 SQLite 支持，包括原始 SQL 和 ORM 风格的 API。

### 连接数据库

#### Lua

```lua
local db = require("wingman.db")

-- 连接到命名数据库（自动创建）
local conn = db.connect("game_data")

-- 或使用内存数据库
local mem_conn = db.connect(":memory:")
```

#### Python

```python
from wingman import db

# 连接到命名数据库（自动创建）
conn = db.connect("game_data")

# 或使用内存数据库
mem_conn = db.connect(":memory:")
```

### 原始 SQL 操作

#### Lua

```lua
local db = require("wingman.db")
local conn = db.connect("game_data")

-- 执行 SQL（无返回值）
conn:execute([[
    CREATE TABLE IF NOT EXISTS players (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        level INTEGER DEFAULT 1,
        score INTEGER DEFAULT 0
    )
]])

-- 执行带参数的 SQL
conn:execute("INSERT INTO players (name, level) VALUES (?, ?)", {
    "player1",
    10
})

-- 查询数据
local rows = conn:query("SELECT * FROM players WHERE level > ?", {"5"})
for _, row in ipairs(rows) do
    print(row.name, row.level, row.score)
end

-- 查询单个值
local count = conn:scalar("SELECT COUNT(*) FROM players")

-- 获取最后插入的 ID
local last_id = conn:lastInsertId()

-- 获取受影响的行数
local changes = conn:changes()
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")

# 执行 SQL（无返回值）
conn.execute("""
    CREATE TABLE IF NOT EXISTS players (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        level INTEGER DEFAULT 1,
        score INTEGER DEFAULT 0
    )
""")

# 执行带参数的 SQL
conn.execute("INSERT INTO players (name, level) VALUES (?, ?)",
             ["player1", 10])

# 查询数据
rows = conn.query("SELECT * FROM players WHERE level > ?", ["5"])
for row in rows:
    print(row["name"], row["level"], row["score"])

# 查询单个值
count = conn.scalar("SELECT COUNT(*) FROM players")

# 获取最后插入的 ID
last_id = conn.lastInsertId()

# 获取受影响的行数
changes = conn.changes()
```

### ORM 风格操作

#### Lua

```lua
local db = require("wingman.db")
local conn = db.connect("game_data")

-- 定义表结构
local players = conn:table("players")

-- 创建表
players:create({
    id = "INTEGER PRIMARY KEY",
    name = "TEXT NOT NULL",
    level = "INTEGER DEFAULT 1",
    score = "INTEGER DEFAULT 0"
})

-- 插入数据
players:insert({
    name = "player1",
    level = 10,
    score = 1000
})

-- 查询所有数据
local all = players:all()

-- 根据 ID 查询
local player = players:get("1")

-- 条件查询
local high_level = players:where("level", ">", "5"):all()

-- 链式查询
local top_players = players:where("score", ">", "1000")
                        :orderBy("score", "desc")
                        :limit(10)
                        :all()

-- 更新数据
players:where("name", "=", "player1")
       :update({score = 2000})

-- 删除数据
players:where("level", "<", "5"):delete()

-- 计数
local count = players:count()
local high_level_count = players:where("level", ">", "10"):count()
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")

# 定义表结构
players = conn.table("players")

# 创建表
players.create({
    "id": "INTEGER PRIMARY KEY",
    "name": "TEXT NOT NULL",
    "level": "INTEGER DEFAULT 1",
    "score": "INTEGER DEFAULT 0"
})

# 插入数据
players.insert({
    "name": "player1",
    "level": 10,
    "score": 1000
})

# 查询所有数据
all = players.all()

# 根据 ID 查询
player = players.get("1")

# 条件查询
high_level = players.where("level", ">", "5").all()

# 链式查询
top_players = players.where("score", ">", "1000")\
                    .orderBy("score", "desc")\
                    .limit(10)\
                    .all()

# 更新数据
players.where("name", "=", "player1")\
       .update({"score": 2000})

# 删除数据
players.where("level", "<", "5").delete()

# 计数
count = players.count()
high_level_count = players.where("level", ">", "10").count()
```

### 事务支持

#### Lua

```lua
local db = require("wingman.db")
local conn = db.connect("game_data")

-- 执行事务
local success = conn:transaction(function(tx)
    tx:execute("INSERT INTO players (name, level) VALUES (?, ?)", {"player1", 10})
    tx:execute("INSERT INTO players (name, level) VALUES (?, ?)", {"player2", 20})

    -- 如果抛出错误，事务会回滚
    if some_error then
        error("Something went wrong")
    end
end)

if success then
    print("事务提交成功")
else
    print("事务回滚")
end
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")

# 执行事务
def transaction_func(tx):
    tx.execute("INSERT INTO players (name, level) VALUES (?, ?)",
               ["player1", 10])
    tx.execute("INSERT INTO players (name, level) VALUES (?, ?)",
               ["player2", 20])

    # 如果抛出错误，事务会回滚
    if some_error:
        raise Exception("Something went wrong")

success = conn.transaction(transaction_func)

if success:
    print("事务提交成功")
else:
    print("事务回滚")
```

## 使用示例

### 游戏进度保存

#### Lua

```lua
local kv = require("wingman.kv")
local db = require("wingman.db")

-- 保存简单状态
kv.set("current_level", "5")
kv.set("player_name", "hero")

-- 保存复杂结构
local conn = db.connect("save_data")
local saves = conn:table("saves")

saves:create({
    id = "INTEGER PRIMARY KEY",
    timestamp = "INTEGER",
    data = "TEXT"
})

saves:insert({
    timestamp = os.time(),
    data = json.encode({
        position = {x = 100, y = 200},
        inventory = {"sword", "shield"},
        quests = {1, 2, 3}
    })
})
```

### 配置数据存储

#### Lua

```lua
local db = require("wingman.db")
local conn = db.connect("config")

local settings = conn:table("settings")

settings:create({
    key = "TEXT PRIMARY KEY",
    value = "TEXT",
    section = "TEXT"
})

-- 插入配置
settings:insert({
    key = "resolution",
    value = "1920x1080",
    section = "graphics"
})

-- 查询配置
local graphics_settings = settings:where("section", "=", "graphics"):all()
```

## 最佳实践

### 1. 选择合适的存储方式

- **使用 kv** 当：
  - 数据结构简单（键值对）
  - 不需要复杂查询
  - 数据量较小

- **使用 db** 当：
  - 需要结构化存储
  - 需要复杂查询和关系
  - 需要事务支持
  - 数据量较大

### 2. 数据库连接管理

```lua
-- 好的做法：重用连接
local db = require("wingman.db")
local conn = db.connect("my_database")

-- 在整个脚本中使用同一个 conn
function saveData(data)
    conn:execute("INSERT INTO data (value) VALUES (?)", {data})
end

function loadData()
    return conn:query("SELECT * FROM data")
end
```

### 3. 使用参数化查询

```lua
-- ❌ 不安全：SQL 注入风险
conn:execute("SELECT * FROM users WHERE name = '" .. userName .. "'")

-- ✅ 安全：使用参数化查询
conn:execute("SELECT * FROM users WHERE name = ?", {userName})
```

### 4. 事务使用

```lua
-- ✅ 使用事务保证数据一致性
conn:transaction(function(tx)
    -- 扣除玩家金币
    tx:execute("UPDATE players SET coins = coins - 100 WHERE id = 1")

    -- 添加物品
    tx:execute("INSERT INTO inventory (player_id, item) VALUES (1, 'sword')")

    -- 如果中间出错，自动回滚
end)
```

### 5. 错误处理

```lua
local ok, err = pcall(function()
    conn:execute("CREATE TABLE players (id INTEGER PRIMARY KEY)")
end)

if not ok then
    print("数据库错误:", err)
    -- 处理错误
end
```

### 6. 定期清理

```lua
-- 清理旧数据
local one_week_ago = os.time() - 7 * 24 * 3600
conn:execute("DELETE FROM logs WHERE timestamp < ?", {one_week_ago})
```

## 🔗 相关文档

- [数据库使用指南](../guides/database.md)
- [配置管理指南](../guides/configuration.md)
- [核心 API](core.md)

---

**返回**: [API 概览](overview.md) | [主页](../README.md)
