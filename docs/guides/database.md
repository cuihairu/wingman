# 数据库使用指南

本指南详细介绍如何使用 Wingman 的数据库功能进行数据持久化。

## 目录

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
- **函数式 ORM**: 通过 `db.table_*` / `db.query_*` 系列函数操作表与查询
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

### 打开数据库

#### Lua

```lua
local wingman = require("wingman")

-- 打开命名数据库（不存在会自动创建）
local conn = wingman.db.open("game_data")

-- 或使用内存数据库（重启后数据丢失）
local mem_conn = wingman.db.open(":memory:")
```

#### Python

```python
from wingman import db

# 打开命名数据库（不存在会自动创建）
conn = db.open("game_data")

# 或使用内存数据库
mem_conn = db.open(":memory:")
```

连接对象、表对象、查询对象都是普通数据对象（没有可调用的方法），所有操作通过 `db.*` 函数完成，对象作为第一个参数传入。

### 创建表

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.open("game_data")

wingman.db.execute(conn, [[
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

conn = db.open("game_data")

db.execute(conn, """
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
wingman.db.execute(conn, "INSERT INTO players (name, level, gold) VALUES (?, ?, ?)", {
    "Hero", 1, 1000
})

-- 获取最后插入的 ID
local last_id = wingman.db.last_insert_id(conn)
print("New player ID:", last_id)
```

#### Python

```python
# 插入单条数据
db.execute(conn, "INSERT INTO players (name, level, gold) VALUES (?, ?, ?)",
           ["Hero", 1, 1000])

# 获取最后插入的 ID
last_id = db.last_insert_id(conn)
print(f"New player ID: {last_id}")
```

### 查询数据

#### Lua

```lua
-- 查询数据（默认最多返回 1000 行）
local rows = wingman.db.query(conn, "SELECT * FROM players")
for _, row in ipairs(rows) do
    print(row.name, row.level, row.gold)
end

-- 查询单个值
local count = wingman.db.scalar(conn, "SELECT COUNT(*) FROM players")
print("Total players:", count)
```

#### Python

```python
# 查询数据（默认最多返回 1000 行）
rows = db.query(conn, "SELECT * FROM players")
for row in rows:
    print(row["name"], row["level"], row["gold"])

# 查询单个值
count = db.scalar(conn, "SELECT COUNT(*) FROM players")
print(f"Total players: {count}")
```

> **注意**：`db.query` 默认最多返回 1000 行；可通过 `db.query(conn, sql, params, maxRows)` 调整，硬顶 10000 行。大数据量请用 SQL 的 `LIMIT`/`OFFSET` 分页。

---

## 基础操作

### CRUD 操作

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.open("game_data")

-- CREATE: 创建表
wingman.db.execute(conn, [[
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        price INTEGER,
        quantity INTEGER DEFAULT 0
    )
]])

-- READ: 读取数据
local items = wingman.db.query(conn, "SELECT * FROM items WHERE quantity > 0")

-- UPDATE: 更新数据
wingman.db.execute(conn, "UPDATE items SET price = ? WHERE name = ?", {
    150, "Sword"
})

-- DELETE: 删除数据
wingman.db.execute(conn, "DELETE FROM items WHERE quantity = 0")
```

#### Python

```python
from wingman import db

conn = db.open("game_data")

# CREATE
db.execute(conn, """
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        price INTEGER,
        quantity INTEGER DEFAULT 0
    )
""")

# READ
items = db.query(conn, "SELECT * FROM items WHERE quantity > 0")

# UPDATE
db.execute(conn, "UPDATE items SET price = ? WHERE name = ?",
           [150, "Sword"])

# DELETE
db.execute(conn, "DELETE FROM items WHERE quantity = 0")
```

### 参数化查询

#### Lua

```lua
-- ✅ 正确：使用参数化查询
local name = "Hero's Sword"
wingman.db.execute(conn, "INSERT INTO items (name) VALUES (?)", {name})

-- ❌ 错误：字符串拼接（SQL 注入风险）
wingman.db.execute(conn, "INSERT INTO items (name) VALUES ('" .. name .. "')")

-- 多个参数
wingman.db.execute(conn, [[
    INSERT INTO players (name, level, gold)
    VALUES (?, ?, ?)
]], {"Player1", 10, 5000})
```

#### Python

```python
# ✅ 正确：使用参数化查询
name = "Hero's Sword"
db.execute(conn, "INSERT INTO items (name) VALUES (?)", [name])

# ❌ 错误：字符串拼接
db.execute(conn, f"INSERT INTO items (name) VALUES ('{name}')")

# 多个参数
db.execute(conn, """
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
local high_level = wingman.db.query(conn, "SELECT * FROM players WHERE level > ?", {10})

-- 多条件
local specific = wingman.db.query(conn, [[
    SELECT * FROM players
    WHERE level > ? AND gold > ?
]], {5, 1000})

-- 模糊查询
local matching = wingman.db.query(conn, "SELECT * FROM players WHERE name LIKE ?", {"%Admin%"})
```

#### Python

```python
# WHERE 条件
high_level = db.query(conn, "SELECT * FROM players WHERE level > ?", [10])

# 多条件
specific = db.query(conn, """
    SELECT * FROM players
    WHERE level > ? AND gold > ?
""", [5, 1000])

# 模糊查询
matching = db.query(conn, "SELECT * FROM players WHERE name LIKE ?", ["%Admin%"])
```

### 排序和分页

#### Lua

```lua
-- 排序
local ranked = wingman.db.query(conn, [[
    SELECT * FROM players
    ORDER BY level DESC, gold DESC
]])

-- 限制结果数量
local top10 = wingman.db.query(conn, [[
    SELECT * FROM players
    ORDER BY level DESC
    LIMIT 10
]])

-- 分页
local page = 2
local per_page = 20
local offset = (page - 1) * per_page
local paged = wingman.db.query(conn, [[
    SELECT * FROM players
    ORDER BY id
    LIMIT ? OFFSET ?
]], {per_page, offset})
```

#### Python

```python
# 排序
ranked = db.query(conn, """
    SELECT * FROM players
    ORDER BY level DESC, gold DESC
""")

# 限制结果数量
top10 = db.query(conn, """
    SELECT * FROM players
    ORDER BY level DESC
    LIMIT 10
""")

# 分页
page = 2
per_page = 20
offset = (page - 1) * per_page
paged = db.query(conn, """
    SELECT * FROM players
    ORDER BY id
    LIMIT ? OFFSET ?
""", [per_page, offset])
```

### 聚合查询

#### Lua

```lua
-- 统计
local count = wingman.db.scalar(conn, "SELECT COUNT(*) FROM players")
local total_gold = wingman.db.scalar(conn, "SELECT SUM(gold) FROM players")
local avg_level = wingman.db.scalar(conn, "SELECT AVG(level) FROM players")

-- 分组统计
local stats = wingman.db.query(conn, [[
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
count = db.scalar(conn, "SELECT COUNT(*) FROM players")
total_gold = db.scalar(conn, "SELECT SUM(gold) FROM players")
avg_level = db.scalar(conn, "SELECT AVG(level) FROM players")

# 分组统计
stats = db.query(conn, """
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
local results = wingman.db.query(conn, [[
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
results = db.query(conn, """
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

ORM（对象关系映射）通过 `db.table_*` / `db.query_*` 系列函数提供更直观的表操作方式。表对象和查询对象都是普通数据对象（没有方法），每个操作都以对象为第一个参数调用对应的函数。

### 创建表

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.open("game_data")

-- 取表对象
local players = wingman.db.table(conn, "players")

-- 定义表结构
wingman.db.table_create(players, {
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

conn = db.open("game_data")

# 取表对象
players = db.table(conn, "players")

# 定义表结构
db.table_create(players, {
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
wingman.db.table_insert(players, {
    name = "Hero",
    level = 5,
    gold = 1000
})

-- 插入多条数据
for i = 1, 10 do
    wingman.db.table_insert(players, {
        name = "Player" .. i,
        level = i,
        gold = i * 100
    })
end
```

#### Python

```python
# 插入单条数据
db.table_insert(players, {
    "name": "Hero",
    "level": 5,
    "gold": 1000
})

# 插入多条数据
for i in range(1, 11):
    db.table_insert(players, {
        "name": f"Player{i}",
        "level": i,
        "gold": i * 100
    })
```

### 查询数据

#### Lua

```lua
-- 查询所有数据（最多返回 1000 行）
local all_rows = wingman.db.table_all(players)

-- 根据 ID 查询
local player = wingman.db.table_get(players, "1")
print(player.name, player.level)

-- 条件查询：table_where 返回查询对象，取结果用 query_all/query_first/query_count
local high_level = wingman.db.query_all(wingman.db.table_where(players, "level", ">", "5"))

-- 排序、限行：查询对象本身没有方法，用函数式逐步传递
local q = wingman.db.table_where(players, "gold", ">", "500")
q = wingman.db.query_order_by(q, "gold", "desc")
q = wingman.db.query_limit(q, 10)
local rich_players = wingman.db.query_all(q)

-- 多个 WHERE 条件请用原生 SQL 组合（table_where 一次只能加一个条件）
local rich_players2 = wingman.db.query(conn,
    "SELECT * FROM players WHERE gold > ? AND level > ?", {500, 3})

-- 获取单条记录
local first = wingman.db.query_first(wingman.db.table_where(players, "name", "=", "Hero"))

-- 计数
local count = wingman.db.table_count(players)
local rich_count = wingman.db.query_count(wingman.db.table_where(players, "gold", ">", "1000"))
```

#### Python

```python
# 查询所有数据（最多返回 1000 行）
all_rows = db.table_all(players)

# 根据 ID 查询
player = db.table_get(players, "1")
print(player["name"], player["level"])

# 条件查询：table_where 返回查询对象，取结果用 query_all/query_first/query_count
high_level = db.query_all(db.table_where(players, "level", ">", "5"))

# 排序、限行：查询对象本身没有方法，用函数式逐步传递
q = db.table_where(players, "gold", ">", "500")
q = db.query_order_by(q, "gold", "desc")
q = db.query_limit(q, 10)
rich_players = db.query_all(q)

# 多个 WHERE 条件请用原生 SQL 组合（table_where 一次只能加一个条件）
rich_players2 = db.query(conn,
    "SELECT * FROM players WHERE gold > ? AND level > ?", [500, 3])

# 获取单条记录
first = db.query_first(db.table_where(players, "name", "=", "Hero"))

# 计数
count = db.table_count(players)
rich_count = db.query_count(db.table_where(players, "gold", ">", "1000"))
```

### 更新数据

#### Lua

```lua
-- 更新单条记录
wingman.db.query_update(wingman.db.table_where(players, "id", "=", "1"), {
    level = 10,
    gold = 5000
})

-- 批量更新
wingman.db.query_update(wingman.db.table_where(players, "level", "<", "5"), {level = 5})
```

#### Python

```python
# 更新单条记录
db.query_update(db.table_where(players, "id", "=", "1"), {
    "level": 10,
    "gold": 5000
})

# 批量更新
db.query_update(db.table_where(players, "level", "<", "5"), {"level": 5})
```

### 删除数据

#### Lua

```lua
-- 删除单条记录
wingman.db.query_delete(wingman.db.table_where(players, "id", "=", "1"))

-- 条件删除
local deleted = wingman.db.query_delete(wingman.db.table_where(players, "level", "<", "3"))
print("Deleted", deleted, "rows")
```

#### Python

```python
# 删除单条记录
db.query_delete(db.table_where(players, "id", "=", "1"))

# 条件删除
deleted = db.query_delete(db.table_where(players, "level", "<", "3"))
print(f"Deleted {deleted} rows")
```

---

## 事务处理

事务确保一组数据库操作的原子性，要么全部成功，要么全部失败。

### 基本事务

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.open("game_data")

-- 执行事务：回调接收连接对象，回调内抛出错误会自动回滚
local success = wingman.db.transaction(conn, function(tx)
    -- 扣除玩家金币
    wingman.db.execute(tx, "UPDATE players SET gold = gold - 100 WHERE id = 1")

    -- 添加物品到背包
    wingman.db.execute(tx, "INSERT INTO inventory (player_id, item_id) VALUES (1, 5)")

    -- 抛出错误即回滚
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

conn = db.open("game_data")

def transaction_func(tx):
    # 扣除玩家金币
    db.execute(tx, "UPDATE players SET gold = gold - 100 WHERE id = 1")

    # 添加物品到背包
    db.execute(tx, "INSERT INTO inventory (player_id, item_id) VALUES (1, 5)")

    # 抛出异常即回滚
    if some_error:
        raise Exception("Transaction failed")

success = db.transaction(conn, transaction_func)

if success:
    print("Transaction committed successfully")
else:
    print("Transaction rolled back")
```

### 嵌套事务

**不支持嵌套事务**：事务进行中再次调用 `db.transaction` 会直接返回 `false`，且内层回调不会执行（连接内部有事务标志位检测）。db 模块未提供 SAVEPOINT 封装，需要"嵌套"语义时请在脚本层自行拆分事务边界，或用原生 SQL 的 `SAVEPOINT`/`RELEASE` 语句自行实现。

#### Lua

```lua
-- 外层事务中再开内层事务：内层返回 false，回调不执行
wingman.db.transaction(conn, function()
    wingman.db.execute(conn, "INSERT INTO players (name) VALUES (?)", {"Player1"})

    local success = wingman.db.transaction(conn, function()
        wingman.db.execute(conn, "INSERT INTO inventory (player_id, item_id) VALUES (1, 1)")
    end)
    -- success == false，上面的 INSERT 不会执行
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

local success = wingman.db.transaction(conn, function(tx)
    for _, player in ipairs(data) do
        wingman.db.execute(tx, "INSERT INTO players (name, level) VALUES (?, ?)", {
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
        db.execute(tx, "INSERT INTO players (name, level) VALUES (?, ?)",
                   [player["name"], player["level"]])

success = db.transaction(conn, batch_insert)

if success:
    print(f"Inserted {len(data)} players")
```

---

## 最佳实践

### 1. 连接管理

```lua
local wingman = require("wingman")

-- ✅ 好的做法：打开一次，整个脚本复用同一个 conn
-- （同名 db.open 会复用已有连接，但脚本级复用更清晰）
local conn = wingman.db.open("my_database")

function savePlayer(name, level)
    wingman.db.execute(conn, "INSERT INTO players (name, level) VALUES (?, ?)", {name, level})
end

-- ❌ 避免：每个函数里都重新 open（虽会命中连接复用，但语义冗长）
function badSavePlayer(name, level)
    local temp_conn = wingman.db.open("my_database")
    wingman.db.execute(temp_conn, "INSERT INTO players (name, level) VALUES (?, ?)", {name, level})
end

-- 脚本结束前显式关闭
-- wingman.db.close(conn)
```

### 2. 错误处理

db 模块的 SQL 失败**不抛异常**，而是返回 `false`（execute/insert/update 类）或 `nil`（查询类）。因此不要依赖 pcall 捕获数据库错误——直接检查返回值；错误详情记录在 runtime 日志里。

```lua
-- ✅ 正确：检查返回值
local ok = wingman.db.execute(conn, "INSERT INTO players (name) VALUES (?)", {"Player1"})
if not ok then
    print("execute failed，详见 runtime 日志")
    -- 处理错误
end

-- ❌ 无效：SQL 失败不会走 pcall 的 not ok 分支
local ok2, err = pcall(function()
    wingman.db.execute(conn, "INSERT INTO players (name) VALUES (?)", {"Player1"})
end)
-- SQL 失败时 ok2 仍为 true、err 为 nil，错误信息拿不到
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
    wingman.db.execute(conn, "INSERT INTO players (name, level, gold) VALUES (?, ?, ?)", {
        name, level, gold
    })
end
```

### 4. 索引优化

```lua
-- 为常用查询字段创建索引
wingman.db.execute(conn, "CREATE INDEX IF NOT EXISTS idx_players_level ON players(level)")
wingman.db.execute(conn, "CREATE INDEX IF NOT EXISTS idx_players_name ON players(name)")

-- 复合索引
wingman.db.execute(conn, "CREATE INDEX IF NOT EXISTS idx_players_level_gold ON players(level, gold)")
```

### 5. 定期清理

```lua
-- 清理旧数据
-- 注意：脚本默认运行在沙箱模式，os 库不可用（没有 os.time）。
-- 时间条件直接在 SQL 里用 SQLite 内置时间函数计算：
wingman.db.execute(conn, [[
    DELETE FROM logs
    WHERE timestamp < datetime('now', '-7 days')
]])

-- 清理空记录
wingman.db.execute(conn, "DELETE FROM inventory WHERE quantity = 0")
```

### 6. 数据备份

db 模块未暴露数据库文件路径，也没有文件读写函数（沙箱模式下 `io` 库不可用）。数据库文件固定存放在脚本数据目录：Windows `%APPDATA%/wingman/scripts/<name>.db`，Unix `~/.local/share/wingman/scripts/<name>.db`。文件级备份请在脚本外（宿主机运维工具）完成；脚本内可改用 SQL 导出数据：

```lua
-- 脚本内导出数据（不依赖 io 库）
function exportPlayers()
    local rows = wingman.db.query(conn, "SELECT * FROM players")
    -- rows 是普通 Lua table，可经 json 模块序列化后存入 kv 模块或上报
    return rows
end
```

---

## 实战案例

### 游戏进度保存

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.open("game_saves")

-- 创建表结构
wingman.db.execute(conn, [[
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
    local success = wingman.db.transaction(conn, function(tx)
        -- 检查是否已有存档
        local existing = wingman.db.scalar(tx,
            "SELECT id FROM game_saves WHERE player_name = ?", {
            player.name
        })

        if existing then
            -- 更新现有存档
            wingman.db.execute(tx, [[
                UPDATE game_saves
                SET level = ?, position_x = ?, position_y = ?, health = ?
                WHERE player_name = ?
           ]], {player.level, player.x, player.y, player.health, player.name})
        else
            -- 创建新存档
            wingman.db.execute(tx, [[
                INSERT INTO game_saves (player_name, level, position_x, position_y, health)
                VALUES (?, ?, ?, ?, ?)
            ]], {player.name, player.level, player.x, player.y, player.health})
        end
    end)

    return success
end

-- 加载游戏进度
function loadGame(playerName)
    local save = wingman.db.query(conn, [[
        SELECT * FROM game_saves
        WHERE player_name = ?
        ORDER BY save_time DESC
        LIMIT 1
    ]], {playerName})

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

local conn = wingman.db.open("leaderboard")

-- 创建表
wingman.db.execute(conn, [[
    CREATE TABLE IF NOT EXISTS leaderboard (
        id INTEGER PRIMARY KEY,
        player_name TEXT NOT NULL UNIQUE,
        score INTEGER,
        updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )
]])

-- 更新分数
function updateScore(playerName, score)
    local success = wingman.db.transaction(conn, function(tx)
        -- 插入或更新分数
        wingman.db.execute(tx, [[
            INSERT INTO leaderboard (player_name, score)
            VALUES (?, ?)
            ON CONFLICT(player_name) DO UPDATE SET
                score = MAX(score, ?),
                updated_at = CURRENT_TIMESTAMP
        ]], {playerName, score, score})
    end)

    return success
end

-- 获取排行榜
function getLeaderboard(limit)
    limit = limit or 10

    local rankings = wingman.db.query(conn, [[
        SELECT player_name, score,
               RANK() OVER (ORDER BY score DESC) as rank
        FROM leaderboard
        ORDER BY score DESC
        LIMIT ?
    ]], {limit})

    return rankings
end

-- 获取玩家排名
function getPlayerRank(playerName)
    local rank = wingman.db.scalar(conn, [[
        SELECT rank FROM (
            SELECT player_name,
                   RANK() OVER (ORDER BY score DESC) as rank
            FROM leaderboard
        ) WHERE player_name = ?
    ]], {playerName})

    return rank
end
```

### 库存管理

#### Lua

```lua
local wingman = require("wingman")

local conn = wingman.db.open("inventory")

-- 创建表
wingman.db.execute(conn, [[
    CREATE TABLE IF NOT EXISTS items (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        max_stack INTEGER DEFAULT 99
    )
]])

wingman.db.execute(conn, [[
    CREATE TABLE IF NOT EXISTS player_inventory (
        id INTEGER PRIMARY KEY,
        player_id INTEGER,
        item_id INTEGER,
        quantity INTEGER DEFAULT 0,
        FOREIGN KEY (item_id) REFERENCES items(id)
    )
]])

local inventory = wingman.db.table(conn, "player_inventory")

-- 添加物品
function addItem(playerId, itemId, quantity)
    local success = wingman.db.transaction(conn, function(tx)
        -- 检查是否已有该物品
        local existing = wingman.db.query(tx, [[
            SELECT quantity FROM player_inventory
            WHERE player_id = ? AND item_id = ?
        ]], {playerId, itemId})

        if #existing > 0 then
            -- 更新数量
            wingman.db.execute(tx, [[
                UPDATE player_inventory
                SET quantity = quantity + ?
                WHERE player_id = ? AND item_id = ?
            ]], {quantity, playerId, itemId})
        else
            -- 添加新物品
            wingman.db.execute(tx, [[
                INSERT INTO player_inventory (player_id, item_id, quantity)
                VALUES (?, ?, ?)
            ]], {playerId, itemId, quantity})
        end
    end)

    return success
end

-- 使用物品
function useItem(playerId, itemId)
    local success = wingman.db.transaction(conn, function(tx)
        -- 减少数量
        wingman.db.execute(tx, [[
            UPDATE player_inventory
            SET quantity = quantity - 1
            WHERE player_id = ? AND item_id = ? AND quantity > 0
        ]], {playerId, itemId})

        -- 删除数量为 0 的物品
        wingman.db.execute(tx, [[
            DELETE FROM player_inventory
            WHERE player_id = ? AND item_id = ? AND quantity = 0
        ]], {playerId, itemId})
    end)

    return success
end

-- 获取玩家物品
function getPlayerInventory(playerId)
    local items = wingman.db.query(conn, [[
        SELECT i.name, pi.quantity
        FROM player_inventory pi
        INNER JOIN items i ON pi.item_id = i.id
        WHERE pi.player_id = ? AND pi.quantity > 0
        ORDER BY i.name
    ]], {playerId})

    return items
end
```

---

## 相关文档

- [数据持久化 API](../api/db.md)
- [配置管理指南](configuration.md)
- [核心 API](../api/core.md)

---

**返回**: [文档首页](../README.md) | [使用指南](../README.md#使用指南)
