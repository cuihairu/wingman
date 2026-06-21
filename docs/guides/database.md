# 数据库使用指南

本指南详细介绍如何使用 Wingman 的数据库功能进行数据持久化。

## 📋 目录

- [概述](#概述)
- [快速开始](#快速开始)
- [基础操作](#基础操作)
- [高级查询](#高级查询)
- [ORM 使用](#orm-使用)
- [事务处理](#事务处理)
- [最佳实践](#最佳实践)
- [实战案例](#实战案例)

---

## 概述

Wingman 提供了完整的 SQLite 数据库支持，包括：

- **原生 SQL**: 执行原始 SQL 查询
- **ORM 风格**: 面向对象的数据库操作
- **事务支持**: 保证数据一致性
- **参数化查询**: 防止 SQL 注入

### 何时使用数据库

- 需要存储大量结构化数据
- 需要复杂的查询和关系
- 需要事务支持
- 需要持久化游戏进度、用户数据等

### 何时使用键值存储

- 数据结构简单（键值对）
- 不需要复杂查询
- 数据量较小

---

## 快速开始

### 连接数据库

#### Lua

```lua
local wingman = require("wingman")

-- 连接到命名数据库（会自动创建）
local conn = wingman.db.connect("game_data")

-- 或使用内存数据库（重启后数据丢失）
local mem_conn = wingman.db.connect(":memory:")
```

#### Python

```python
from wingman import db

# 连接到命名数据库（会自动创建）
conn = db.connect("game_data")

# 或使用内存数据库
mem_conn = db.connect(":memory:")
```

### 创建表

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("game_data")

conn:execute([[
    CREATE TABLE IF NOT EXISTS players (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        level INTEGER DEFAULT 1,
        experience INTEGER DEFAULT 0,
        gold INTEGER DEFAULT 0,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
]])
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")

conn.execute("""
    CREATE TABLE IF NOT EXISTS players (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        level INTEGER DEFAULT 1,
        experience INTEGER DEFAULT 0,
        gold INTEGER DEFAULT 0,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
""")
```

### 插入数据

#### Lua

```lua
-- 插入单条数据
conn:execute("INSERT INTO players (name, level, gold) VALUES (?, ?, ?)", {
    "Hero", 1, 1000
})

-- 获取最后插入的 ID
local last_id = conn:lastInsertId()
print("New player ID:", last_id)
```

#### Python

```python
# 插入单条数据
conn.execute("INSERT INTO players (name, level, gold) VALUES (?, ?, ?)",
            ["Hero", 1, 1000])

# 获取最后插入的 ID
last_id = conn.lastInsertId()
print(f"New player ID: {last_id}")
```

### 查询数据

#### Lua

```lua
-- 查询所有数据
local rows = conn:query("SELECT * FROM players")
for _, row in ipairs(rows) do
    print(row.name, row.level, row.gold)
end

-- 查询单个值
local count = conn:scalar("SELECT COUNT(*) FROM players")
print("Total players:", count)
```

#### Python

```python
# 查询所有数据
rows = conn.query("SELECT * FROM players")
for row in rows:
    print(row["name"], row["level"], row["gold"])

# 查询单个值
count = conn.scalar("SELECT COUNT(*) FROM players")
print(f"Total players: {count}")
```

---

## 基础操作

### CRUD 操作

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("game_data")

-- CREATE: 创建表
conn:execute([[
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        price INTEGER,
        quantity INTEGER DEFAULT 0
    )
]])

-- READ: 读取数据
local items = conn:query("SELECT * FROM items WHERE quantity > 0")

-- UPDATE: 更新数据
conn:execute("UPDATE items SET price = ? WHERE name = ?", {
    150, "Sword"
})

-- DELETE: 删除数据
conn:execute("DELETE FROM items WHERE quantity = 0")
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")

# CREATE
conn.execute("""
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        price INTEGER,
        quantity INTEGER DEFAULT 0
    )
""")

# READ
items = conn.query("SELECT * FROM items WHERE quantity > 0")

# UPDATE
conn.execute("UPDATE items SET price = ? WHERE name = ?",
            [150, "Sword"])

# DELETE
conn.execute("DELETE FROM items WHERE quantity = 0")
```

### 参数化查询

#### Lua

```lua
-- ✅ 正确：使用参数化查询
local name = "Hero's Sword"
conn:execute("INSERT INTO items (name) VALUES (?)", {name})

-- ❌ 错误：字符串拼接（SQL 注入风险）
conn:execute("INSERT INTO items (name) VALUES ('" .. name .. "')")

-- 多个参数
conn:execute([[
    INSERT INTO players (name, level, gold)
    VALUES (?, ?, ?)
]], {"Player1", 10, 5000})
```

#### Python

```python
# ✅ 正确：使用参数化查询
name = "Hero's Sword"
conn.execute("INSERT INTO items (name) VALUES (?)", [name])

# ❌ 错误：字符串拼接
conn.execute(f"INSERT INTO items (name) VALUES ('{name}')")

# 多个参数
conn.execute("""
    INSERT INTO players (name, level, gold)
    VALUES (?, ?, ?)
""", ["Player1", 10, 5000])
```

---

## 高级查询

### 条件查询

#### Lua

```lua
-- WHERE 条件
local high_level = conn:query("SELECT * FROM players WHERE level > ?", {10})

-- 多条件
local specific = conn:query([[
    SELECT * FROM players
    WHERE level > ? AND gold > ?
]], {[5, 1000]})

-- 模糊查询
local matching = conn:query("SELECT * FROM players WHERE name LIKE ?", {"%Admin%"})
```

#### Python

```python
# WHERE 条件
high_level = conn.query("SELECT * FROM players WHERE level > ?", [10])

# 多条件
specific = conn.query("""
    SELECT * FROM players
    WHERE level > ? AND gold > ?
""", [5, 1000])

# 模糊查询
matching = conn.query("SELECT * FROM players WHERE name LIKE ?", ["%Admin%"])
```

### 排序和分页

#### Lua

```lua
-- 排序
local ranked = conn:query([[
    SELECT * FROM players
    ORDER BY level DESC, gold DESC
]])

-- 限制结果数量
local top10 = conn:query([[
    SELECT * FROM players
    ORDER BY level DESC
    LIMIT 10
]])

-- 分页
local page = 2
local per_page = 20
local offset = (page - 1) * per_page
local paged = conn:query([[
    SELECT * FROM players
    ORDER BY id
    LIMIT ? OFFSET ?
]], {[per_page, offset]})
```

#### Python

```python
# 排序
ranked = conn.query("""
    SELECT * FROM players
    ORDER BY level DESC, gold DESC
""")

# 限制结果数量
top10 = conn.query("""
    SELECT * FROM players
    ORDER BY level DESC
    LIMIT 10
""")

# 分页
page = 2
per_page = 20
offset = (page - 1) * per_page
paged = conn.query("""
    SELECT * FROM players
    ORDER BY id
    LIMIT ? OFFSET ?
""", [per_page, offset])
```

### 聚合查询

#### Lua

```lua
-- 统计
local count = conn:scalar("SELECT COUNT(*) FROM players")
local total_gold = conn:scalar("SELECT SUM(gold) FROM players")
local avg_level = conn:scalar("SELECT AVG(level) FROM players")

-- 分组统计
local stats = conn:query([[
    SELECT level, COUNT(*) as count, AVG(gold) as avg_gold
    FROM players
    GROUP BY level
    ORDER BY level
]])

for _, row in ipairs(stats) do
    print(string.format("Level %d: %d players, avg gold: %.1f",
        row.level, row.count, row.avg_gold))
end
```

#### Python

```python
# 统计
count = conn.scalar("SELECT COUNT(*) FROM players")
total_gold = conn.scalar("SELECT SUM(gold) FROM players")
avg_level = conn.scalar("SELECT AVG(level) FROM players")

# 分组统计
stats = conn.query("""
    SELECT level, COUNT(*) as count, AVG(gold) as avg_gold
    FROM players
    GROUP BY level
    ORDER BY level
""")

for row in stats:
    print(f"Level {row['level']}: {row['count']} players, "
          f"avg gold: {row['avg_gold']:.1f}")
```

### JOIN 查询

#### Lua

```lua
-- INNER JOIN
local results = conn:query([[
    SELECT
        p.name as player_name,
        i.name as item_name,
        pi.quantity
    FROM player_items pi
    INNER JOIN players p ON pi.player_id = p.id
    INNER JOIN items i ON pi.item_id = i.id
    WHERE pi.quantity > 0
]])
```

#### Python

```python
# INNER JOIN
results = conn.query("""
    SELECT
        p.name as player_name,
        i.name as item_name,
        pi.quantity
    FROM player_items pi
    INNER JOIN players p ON pi.player_id = p.id
    INNER JOIN items i ON pi.item_id = i.id
    WHERE pi.quantity > 0
""")
```

---

## ORM 使用

ORM（对象关系映射）提供了更直观的数据库操作方式。

### 创建表

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("game_data")

local players = conn:table("players")

-- 定义表结构
players:create({
    id = "INTEGER PRIMARY KEY",
    name = "TEXT NOT NULL",
    level = "INTEGER DEFAULT 1",
    experience = "INTEGER DEFAULT 0",
    gold = "INTEGER DEFAULT 0"
})
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")
players = conn.table("players")

# 定义表结构
players.create({
    "id": "INTEGER PRIMARY KEY",
    "name": "TEXT NOT NULL",
    "level": "INTEGER DEFAULT 1",
    "experience": "INTEGER DEFAULT 0",
    "gold": "INTEGER DEFAULT 0"
})
```

### 插入数据

#### Lua

```lua
-- 插入单条数据
players:insert({
    name = "Hero",
    level = 5,
    gold = 1000
})

-- 插入多条数据
for i = 1, 10 do
    players:insert({
        name = "Player" .. i,
        level = i,
        gold = i * 100
    })
end
```

#### Python

```python
# 插入单条数据
players.insert({
    "name": "Hero",
    "level": 5,
    "gold": 1000
})

# 插入多条数据
for i in range(1, 11):
    players.insert({
        "name": f"Player{i}",
        "level": i,
        "gold": i * 100
    })
```

### 查询数据

#### Lua

```lua
-- 查询所有数据
local all = players:all()

-- 根据 ID 查询
local player = players:get("1")
print(player.name, player.level)

-- 条件查询
local high_level = players:where("level", ">", "5"):all()

-- 链式查询
local rich_players = players:where("gold", ">", "500")
                          :where("level", ">", "3")
                          :orderBy("gold", "desc")
                          :limit(10)
                          :all()

-- 获取单条记录
local first = players:where("name", "=", "Hero"):first()

-- 计数
local count = players:count()
local rich_count = players:where("gold", ">", "1000"):count()
```

#### Python

```python
# 查询所有数据
all = players.all()

# 根据 ID 查询
player = players.get("1")
print(player["name"], player["level"])

# 条件查询
high_level = players.where("level", ">", "5").all()

# 链式查询
rich_players = players.where("gold", ">", "500")\
                      .where("level", ">", "3")\
                      .orderBy("gold", "desc")\
                      .limit(10)\
                      .all()

# 获取单条记录
first = players.where("name", "=", "Hero").first()

# 计数
count = players.count()
rich_count = players.where("gold", ">", "1000").count()
```

### 更新数据

#### Lua

```lua
-- 更新单条记录
players:where("id", "=", "1")
       :update({
           level = 10,
           gold = 5000
       })

-- 批量更新
players:where("level", "<", "5")
       :update({level = 5})
```

#### Python

```python
# 更新单条记录
players.where("id", "=", "1")\
       .update({
           "level": 10,
           "gold": 5000
       })

# 批量更新
players.where("level", "<", "5")\
       .update({"level": 5})
```

### 删除数据

#### Lua

```lua
-- 删除单条记录
players:where("id", "=", "1"):delete()

-- 条件删除
local deleted = players:where("level", "<", "3"):delete()
print("Deleted", deleted, "rows")
```

#### Python

```python
# 删除单条记录
players.where("id", "=", "1").delete()

# 条件删除
deleted = players.where("level", "<", "3").delete()
print(f"Deleted {deleted} rows")
```

---

## 事务处理

事务确保一组数据库操作的原子性，要么全部成功，要么全部失败。

### 基本事务

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("game_data")

-- 执行事务
local success = conn:transaction(function(tx)
    -- 扣除玩家金币
    tx:execute("UPDATE players SET gold = gold - 100 WHERE id = 1")

    -- 添加物品到背包
    tx:execute("INSERT INTO inventory (player_id, item_id) VALUES (1, 5)")

    -- 如果抛出错误，事务会自动回滚
    if some_error then
        error("Transaction failed")
    end
end)

if success then
    print("Transaction committed successfully")
else
    print("Transaction rolled back")
end
```

#### Python

```python
from wingman import db

conn = db.connect("game_data")

def transaction_func(tx):
    # 扣除玩家金币
    tx.execute("UPDATE players SET gold = gold - 100 WHERE id = 1")

    # 添加物品到背包
    tx.execute("INSERT INTO inventory (player_id, item_id) VALUES (1, 5)")

    # 如果抛出错误，事务会自动回滚
    if some_error:
        raise Exception("Transaction failed")

success = conn.transaction(transaction_func)

if success:
    print("Transaction committed successfully")
else:
    print("Transaction rolled back")
```

### 嵌套事务

#### Lua

```lua
-- 外层事务
conn:transaction(function()
    conn:execute("INSERT INTO players (name) VALUES (?)", {"Player1"})

    -- 内层事务
    local success = conn:transaction(function(tx)
        tx:execute("INSERT INTO inventory (player_id, item_id) VALUES (1, 1)")
    end)

    if not success then
        error("Inner transaction failed")
    end
end)
```

### 批量操作事务

#### Lua

```lua
-- 批量插入
local data = {
    {name = "Player1", level = 1},
    {name = "Player2", level = 2},
    {name = "Player3", level = 3}
}

local success = conn:transaction(function(tx)
    for _, player in ipairs(data) do
        tx:execute("INSERT INTO players (name, level) VALUES (?, ?)", {
            player.name, player.level
        })
    end
end)

if success then
    print("Inserted " .. #data .. " players")
end
```

#### Python

```python
# 批量插入
data = [
    {"name": "Player1", "level": 1},
    {"name": "Player2", "level": 2},
    {"name": "Player3", "level": 3}
]

def batch_insert(tx):
    for player in data:
        tx.execute("INSERT INTO players (name, level) VALUES (?, ?)",
                  [player["name"], player["level"]])

success = conn.transaction(batch_insert)

if success:
    print(f"Inserted {len(data)} players")
```

---

## 最佳实践

### 1. 连接管理

```lua
local wingman = require("wingman")

-- ✅ 好的做法：重用连接
local conn = wingman.db.connect("my_database")

-- 在整个脚本中使用同一个 conn
function savePlayer(name, level)
    conn:execute("INSERT INTO players (name, level) VALUES (?, ?)", {name, level})
end

-- ❌ 避免：频繁创建新连接
function badSavePlayer(name, level)
    local temp_conn = wingman.db.connect("my_database")
    temp_conn:execute("INSERT INTO players (name, level) VALUES (?, ?)", {name, level})
end
```

### 2. 错误处理

```lua
-- 使用 pcall 处理错误
local ok, err = pcall(function()
    conn:execute("INSERT INTO players (name) VALUES (?)", {"Player1"})
end)

if not ok then
    print("Database error:", err)
    -- 处理错误
end
```

### 3. 数据验证

```lua
-- 插入前验证数据
function addPlayer(name, level, gold)
    -- 验证输入
    if not name or name == "" then
        error("Invalid name")
    end

    if not level or level < 1 or level > 100 then
        error("Invalid level")
    end

    if not gold or gold < 0 then
        error("Invalid gold amount")
    end

    -- 验证通过后插入
    conn:execute("INSERT INTO players (name, level, gold) VALUES (?, ?, ?)", {
        name, level, gold
    })
end
```

### 4. 索引优化

```lua
-- 为常用查询字段创建索引
conn:execute("CREATE INDEX IF NOT EXISTS idx_players_level ON players(level)")
conn:execute("CREATE INDEX IF NOT EXISTS idx_players_name ON players(name)")

-- 复合索引
conn:execute("CREATE INDEX IF NOT EXISTS idx_players_level_gold ON players(level, gold)")
```

### 5. 定期清理

```lua
-- 清理旧数据
local one_week_ago = os.time() - 7 * 24 * 3600
conn:execute("DELETE FROM logs WHERE timestamp < ?", {one_week_ago})

-- 清理空记录
conn:execute("DELETE FROM inventory WHERE quantity = 0")
```

### 6. 数据备份

```lua
-- 备份数据库
function backupDatabase()
    local db_path = conn:getPath()
    local backup_path = db_path .. ".backup"

    -- 读取原数据库
    local source = io.open(db_path, "rb")
    local content = source:read("*all")
    source:close()

    -- 写入备份
    local backup = io.open(backup_path, "wb")
    backup:write(content)
    backup:close()

    print("Database backed up to:", backup_path)
end
```

---

## 实战案例

### 游戏进度保存

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("game_saves")

-- 创建表结构
conn:execute([[
    CREATE TABLE IF NOT EXISTS game_saves (
        id INTEGER PRIMARY KEY,
        player_name TEXT NOT NULL,
        level INTEGER,
        position_x REAL,
        position_y REAL,
        health INTEGER,
        save_time DATETIME DEFAULT CURRENT_TIMESTAMP
    )
]])

-- 保存游戏进度
function saveGame(player)
    local success = conn:transaction(function(tx)
        -- 检查是否已有存档
        local existing = tx:scalar("SELECT id FROM game_saves WHERE player_name = ?", {
            player.name
        })

        if existing then
            -- 更新现有存档
            tx:execute([[
                UPDATE game_saves
                SET level = ?, position_x = ?, position_y = ?, health = ?
                WHERE player_name = ?
           ]], {[player.level, player.x, player.y, player.health, player.name]})
        else
            -- 创建新存档
            tx:execute([[
                INSERT INTO game_saves (player_name, level, position_x, position_y, health)
                VALUES (?, ?, ?, ?, ?)
            ]], {[player.name, player.level, player.x, player.y, player.health]})
        end
    end)

    return success
end

-- 加载游戏进度
function loadGame(playerName)
    local save = conn:query([[
        SELECT * FROM game_saves
        WHERE player_name = ?
        ORDER BY save_time DESC
        LIMIT 1
    ]], {[playerName]})

    if #save > 0 then
        return {
            name = save[1].player_name,
            level = save[1].level,
            x = save[1].position_x,
            y = save[1].position_y,
            health = save[1].health
        }
    else
        return nil
    end
end
```

### 排行榜系统

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("leaderboard")

-- 创建表
conn:execute([[
    CREATE TABLE IF NOT EXISTS leaderboard (
        id INTEGER PRIMARY KEY,
        player_name TEXT NOT NULL UNIQUE,
        score INTEGER,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
]])

-- 更新分数
function updateScore(playerName, score)
    local success = conn:transaction(function(tx)
        -- 插入或更新分数
        tx:execute([[
            INSERT INTO leaderboard (player_name, score)
            VALUES (?, ?)
            ON CONFLICT(player_name) DO UPDATE SET
                score = MAX(score, ?),
                updated_at = CURRENT_TIMESTAMP
        ]], {[playerName, score, score]})
    end)

    return success
end

-- 获取排行榜
function getLeaderboard(limit)
    limit = limit or 10

    local rankings = conn:query([[
        SELECT player_name, score,
               RANK() OVER (ORDER BY score DESC) as rank
        FROM leaderboard
        ORDER BY score DESC
        LIMIT ?
    ]], {[limit]})

    return rankings
end

-- 获取玩家排名
function getPlayerRank(playerName)
    local rank = conn:scalar([[
        SELECT rank FROM (
            SELECT player_name,
                   RANK() OVER (ORDER BY score DESC) as rank
            FROM leaderboard
        ) WHERE player_name = ?
    ]], {[playerName]})

    return rank
end
```

### 库存管理

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.connect("inventory")

-- 创建表
conn:execute([[
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        max_stack INTEGER DEFAULT 99
    )
]])

conn:execute([[
    CREATE TABLE IF NOT EXISTS player_inventory (
        id INTEGER PRIMARY KEY,
        player_id INTEGER,
        item_id INTEGER,
        quantity INTEGER DEFAULT 0,
        FOREIGN KEY (item_id) REFERENCES items(id)
    )
]])

local inventory = conn:table("player_inventory")

-- 添加物品
function addItem(playerId, itemId, quantity)
    local success = conn:transaction(function(tx)
        -- 检查是否已有该物品
        local existing = tx:query([[
            SELECT quantity FROM player_inventory
            WHERE player_id = ? AND item_id = ?
        ]], {[playerId, itemId]})

        if #existing > 0 then
            -- 更新数量
            tx:execute([[
                UPDATE player_inventory
                SET quantity = quantity + ?
                WHERE player_id = ? AND item_id = ?
            ]], {[quantity, playerId, itemId]})
        else
            -- 添加新物品
            tx:execute([[
                INSERT INTO player_inventory (player_id, item_id, quantity)
                VALUES (?, ?, ?)
            ]], {[playerId, itemId, quantity]})
        end
    end)

    return success
end

-- 使用物品
function useItem(playerId, itemId)
    local success = conn:transaction(function(tx)
        -- 减少数量
        tx:execute([[
            UPDATE player_inventory
            SET quantity = quantity - 1
            WHERE player_id = ? AND item_id = ? AND quantity > 0
        ]], {[playerId, itemId]})

        -- 删除数量为 0 的物品
        tx:execute([[
            DELETE FROM player_inventory
            WHERE player_id = ? AND item_id = ? AND quantity = 0
        ]], {[playerId, itemId]})
    end)

    return success
end

-- 获取玩家物品
function getPlayerInventory(playerId)
    local items = conn:query([[
        SELECT i.name, pi.quantity
        FROM player_inventory pi
        INNER JOIN items i ON pi.item_id = i.id
        WHERE pi.player_id = ? AND pi.quantity > 0
        ORDER BY i.name
    ]], {[playerId]})

    return items
end
```

---

## 🔗 相关文档

- [数据持久化 API](../api/storage.md)
- [配置管理指南](configuration.md)
- [核心 API](../api/core.md)

---

**返回**: [文档首页](../README.md) | [使用指南](../README.md#使用指南)
