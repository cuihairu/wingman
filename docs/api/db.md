# db 模块

db 模块提供本地 SQLite 数据库访问能力，用于脚本本地状态、缓存、轻量结构化数据存储。

## 设计约束

1. **db 是本地脚本数据库，不是 orchestrator/server 数据库**
   - db 模块仅供脚本本地使用，不参与 server 中控数据模型
   - 同名数据库连接在**进程内全局共享**：不同脚本 `db.open("local")` 拿到的是同一个连接、同一个数据文件，数据跨脚本可见；需要隔离时请使用不同的 db 名

2. **路径限制**
   - db 默认只能打开 runtime 管理目录内的数据库
   - `:memory:` 允许（内存数据库）
   - 禁止绝对路径（包含 `/`、`\` 或 `..` 的名称会被拒绝）
   - 所有数据库文件自动存储在 `{dataDir}/{name}.db`

3. **连接复用**
   - 相同名称的 `db.open()` 调用会复用已有连接（连接池机制）
   - 每个连接内部使用 `recursive_mutex` 串行化 sqlite3 调用
   - 使用 `recursive_mutex` 而非普通 `mutex`，确保事务回调中可以安全调用 `execute()`/`query()`
   - 不同 connection 可以并发使用

4. **资源管理**
   - statement 用 RAII finalize
   - connection 用 RAII close
   - 建议使用完后显式调用 `db.close(conn)`

5. **事务**
   - transaction 禁止嵌套（内部有 `m_inTransaction` 标志位检测）
   - 失败自动回滚
   - 事务回调中可安全调用 `execute()`、`query()`、`scalar()`（recursive_mutex 保证）

6. **查询限制**
   - query 返回最大行数默认 1000，可通过 `maxRows` 参数调整，**无上限钳制**
   - 10000 行上限仅约束 `query_limit`（ORM 链式查询）
   - 避免脚本一次性拉爆内存

## 路径映射

| 调用 | 实际路径 |
|------|---------|
| `db.open("local")` | `%APPDATA%/wingman/scripts/local.db` (Windows) |
| `db.open("cache")` | `%APPDATA%/wingman/scripts/cache.db` (Windows) |
| `db.open(":memory:")` | 内存数据库 |

Unix 系统路径：`~/.local/share/wingman/scripts/{name}.db`

## 底层 API

### db.open(name)

打开数据库连接。

**参数：**
- `name` (string): 数据库名称（预置名称如 "local", "cache"）或 ":memory:"

**返回：**
- 连接对象

**示例：**

```python
from wingman import db

conn = db.open("local")      # 持久化数据库
mem_conn = db.open(":memory:")  # 内存数据库
```

```lua
local wingman = require("wingman")

local conn = wingman.db.open("local")
local mem_conn = wingman.db.open(":memory:")
```

### db.execute(conn, sql, params?)

执行 SQL 语句（无结果返回）。

**参数：**
- `conn` (connection): 数据库连接对象
- `sql` (string): SQL 语句，使用 `?` 作为参数占位符
- `params` (array?, optional): 参数列表

**返回：**
- boolean: 是否成功

**示例：**

```python
db.execute(conn, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
db.execute(conn, "INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 25])
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.execute(conn, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)")
db.execute(conn, "INSERT INTO users (name, age) VALUES (?, ?)", {"Alice", 25})
```

### db.query(conn, sql, params?, maxRows?)

查询 SQL 语句（返回多行）。

**参数：**
- `conn` (connection): 数据库连接对象
- `sql` (string): SQL 语句，使用 `?` 作为参数占位符
- `params` (array?, optional): 参数列表
- `maxRows` (int?, optional): 最大返回行数，默认 1000。**无上限钳制**（传 100000 也会照取；10000 的钳制只存在于 `query_limit`）

**返回：**
- array: 查询结果（字典数组）

**示例：**

```python
rows = db.query(conn, "SELECT * FROM users WHERE age > ?", [20])
for row in rows:
    print(row["name"], row["age"])
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local rows = db.query(conn, "SELECT * FROM users WHERE age > ?", {20})
for i, row in ipairs(rows) do
    print(row.name, row.age)
end
```

### db.scalar(conn, sql, params?)

查询单个值（标量）。

**参数：**
- `conn` (connection): 数据库连接对象
- `sql` (string): SQL 语句，使用 `?` 作为参数占位符
- `params` (array?, optional): 参数列表

**返回：**
- string: 查询结果（第一行第一列）

**示例：**

```python
count = db.scalar(conn, "SELECT COUNT(*) FROM users")
max_age = db.scalar(conn, "SELECT MAX(age) FROM users")
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local count = db.scalar(conn, "SELECT COUNT(*) FROM users")
local maxAge = db.scalar(conn, "SELECT MAX(age) FROM users")
```

### db.transaction(conn, callback)

执行事务。

**参数：**
- `conn` (connection): 数据库连接对象
- `callback` (function): 事务回调函数，接收连接对象作为参数

**返回：**
- boolean: 是否成功

**示例：**

```python
def tx(conn):
    db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Alice"])
    db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Bob"])

success = db.transaction(conn, tx)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local success = db.transaction(conn, function(tx)
    db.execute(tx, "INSERT INTO users (name) VALUES (?)", {"Alice"})
    db.execute(tx, "INSERT INTO users (name) VALUES (?)", {"Bob"})
end)
```

### db.last_insert_id(conn)

获取最后插入的行 ID。

**参数：**
- `conn` (connection): 数据库连接对象

**返回：**
- int: 最后插入的行 ID

**示例：**

```python
db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Alice"])
id = db.last_insert_id(conn)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.execute(conn, "INSERT INTO users (name) VALUES (?)", {"Alice"})
local id = db.last_insert_id(conn)
```

### db.changes(conn)

获取受影响的行数。

**参数：**
- `conn` (connection): 数据库连接对象

**返回：**
- int: 受影响的行数

**示例：**

```python
db.execute(conn, "UPDATE users SET age = 30 WHERE name = ?", ["Alice"])
num = db.changes(conn)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.execute(conn, "UPDATE users SET age = 30 WHERE name = ?", {"Alice"})
local num = db.changes(conn)
```

### db.close(conn)

关闭数据库连接。

**参数：**
- `conn` (connection): 数据库连接对象

**返回：**
- boolean: 是否成功

**示例：**

```python
db.close(conn)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.close(conn)
```

## ORM API

### db.table(conn, tableName)

创建表助手。

**参数：**
- `conn` (connection): 数据库连接对象
- `tableName` (string): 表名

**返回：**
- table: 表对象

**示例：**

```python
users = db.table(conn, "users")
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local users = db.table(conn, "users")
```

### table.create(schema)

创建表。

**参数：**
- `table` (table): 表对象
- `schema` (object): 表结构定义 {字段名: 类型定义}

**返回：**
- boolean: 是否成功

**示例：**

```python
db.table_create(users, {
    "id": "INTEGER PRIMARY KEY",
    "name": "TEXT NOT NULL",
    "age": "INTEGER"
})
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.table_create(users, {
    id = "INTEGER PRIMARY KEY",
    name = "TEXT NOT NULL",
    age = "INTEGER"
})
```

### table.insert(row)

插入行。

**参数：**
- `table` (table): 表对象
- `row` (object): 行数据 {字段名: 值}

**返回：**
- boolean: 是否成功

**示例：**

```python
db.table_insert(users, {"name": "Alice", "age": 25})
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.table_insert(users, {name = "Alice", age = 25})
```

### table.get(id)

按 ID 查询。

**参数：**
- `table` (table): 表对象
- `id` (string): ID 值

**返回：**
- object|null: 查询结果或 null

**示例：**

```python
user = db.table_get(users, "1")
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local user = db.table_get(users, "1")
```

### table.where(field, op, value)

构建查询。

**参数：**
- `table` (table): 表对象
- `field` (string): 字段名
- `op` (string): 操作符 (=, !=, >, >=, <, <=, like, in)
- `value` (string): 值

**返回：**
- query: 查询构建器对象（数据句柄；后续 `query_*` 函数均以它为首参做函数式传递，**不支持** `.query_xxx()` 方法链）

**示例：**

```python
query = db.table_where(users, "age", ">", "20")
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local query = db.table_where(users, "age", ">", "20")
```

### table.all()

获取所有行（带默认 limit）。

**参数：**
- `table` (table): 表对象

**返回：**
- array: 所有行（最多 1000 行）

**示例：**

```python
rows = db.table_all(users)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local rows = db.table_all(users)
```

### table.count()

计数。

**参数：**
- `table` (table): 表对象

**返回：**
- int: 行数

**示例：**

```python
count = db.table_count(users)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local count = db.table_count(users)
```

## QueryBuilder API

### query.all()

获取所有结果。

**参数：**
- `query` (query): 查询构建器对象

**返回：**
- array: 查询结果

**示例：**

```python
rows = db.query_all(query)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local rows = db.query_all(query)
```

### query.first()

获取单行。

**参数：**
- `query` (query): 查询构建器对象

**返回：**
- object|null: 第一行或 null

**示例：**

```python
row = db.query_first(query)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local row = db.query_first(query)
```

### query.count()

计数。

**参数：**
- `query` (query): 查询构建器对象

**返回：**
- int: 行数

**示例：**

```python
count = db.query_count(query)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local count = db.query_count(query)
```

### query.update(row)

更新匹配的行。

**参数：**
- `query` (query): 查询构建器对象
- `row` (object): 更新数据

**返回：**
- boolean: 是否成功

**示例：**

```python
db.query_update(query, {"age": "26"})
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.query_update(query, {age = "26"})
```

### query.delete()

删除匹配的行。

**参数：**
- `query` (query): 查询构建器对象

**返回：**
- int: 删除的行数

**示例：**

```python
count = db.query_delete(query)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local count = db.query_delete(query)
```

### query.limit(n)

限制行数。

**参数：**
- `query` (query): 查询构建器对象
- `n` (int): 行数（上限 10000，超出会被钳制）

**返回：**
- query: 修改后的 query 对象（**不是**可链式调用的方法对象；返回值须作为下一步函数调用的首参传入，如 `db.query_all(db.query_limit(q, 10))`）

**示例：**

```python
query = db.query_limit(query, 10)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
query = db.query_limit(query, 10)
```

### query.orderBy(field, direction?)

排序。

**参数：**
- `query` (query): 查询构建器对象
- `field` (string): 字段名
- `direction` (string?, optional): 方向 (asc, desc)，默认 "asc"

**返回：**
- query: 修改后的 query 对象（返回值须作为下一步函数调用的首参传入，如 `db.query_all(db.query_order_by(q, "id", "desc"))`）

**示例：**

```python
query = db.query_order_by(query, "id", "desc")
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
query = db.query_order_by(query, "id", "desc")
```

### table.close(t)

释放 table 句柄（从进程内句柄注册表移除，句柄失效后不可再使用）。

**参数：**
- `t` (table): `db.table()` 返回的表对象

**返回：**
- boolean: 是否释放成功

**示例：**

```python
db.table_close(users)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.table_close(users)
```

### query.close(q)

释放 query 句柄（语义同 `table_close`）。

**参数：**
- `q` (query): `db.table_where()` / `db.query_limit()` 等返回的 query 对象

**返回：**
- boolean: 是否释放成功

**示例：**

```python
db.query_close(query)
```

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
db.query_close(query)
```

**注意**：table/query 句柄记录在进程内注册表中，不会被 GC 自动回收。长循环里反复 `db.table_where(...)` + `db.query_all(...)` 会持续累积句柄，用完的 query 应及时调用 `db.query_close(query)`（table 同理）释放。

## 完整示例

### Python

```python
from wingman import db

# 打开数据库
conn = db.open("local")

# 创建表
users = db.table(conn, "users")
db.table_create(users, {
    "id": "INTEGER PRIMARY KEY",
    "name": "TEXT NOT NULL",
    "age": "INTEGER",
    "email": "TEXT"
})

# 插入数据
db.table_insert(users, {"name": "Alice", "age": "25", "email": "alice@example.com"})
db.table_insert(users, {"name": "Bob", "age": "30", "email": "bob@example.com"})
db.table_insert(users, {"name": "Charlie", "age": "35", "email": "charlie@example.com"})

# 查询数据
adults = db.table_where(users, "age", ">=", "18")
results = db.query_all(adults)
for user in results:
    print(f"{user['name']}: {user['email']}")

# 更新数据
bob = db.table_where(users, "name", "=", "Bob")
db.query_update(bob, {"age": "31"})

# 删除数据
young = db.table_where(users, "age", "<", "20")
deleted = db.query_delete(young)
print(f"Deleted {deleted} young users")

# 计数
count = db.table_count(users)
print(f"Total users: {count}")

# 使用底层 SQL
rows = db.query(conn, "SELECT * FROM users WHERE age > ?", [25])
scalar_count = db.scalar(conn, "SELECT COUNT(*) FROM users")

# 事务
def tx(conn):
    db.execute(conn, "INSERT INTO users (name, age) VALUES (?, ?)", ["David", 28])
    db.execute(conn, "UPDATE users SET age = ? WHERE name = ?", [29, "David"])

db.transaction(conn, tx)

# 关闭连接
db.close(conn)
```

### Lua

```lua
local wingman = require("wingman")

-- 打开数据库
local conn = wingman.db.open("local")

-- 创建表
local users = wingman.db.table(conn, "users")
wingman.db.table_create(users, {
    id = "INTEGER PRIMARY KEY",
    name = "TEXT NOT NULL",
    age = "INTEGER",
    email = "TEXT"
})

-- 插入数据
wingman.db.table_insert(users, {name = "Alice", age = "25", email = "alice@example.com"})
wingman.db.table_insert(users, {name = "Bob", age = "30", email = "bob@example.com"})
wingman.db.table_insert(users, {name = "Charlie", age = "35", email = "charlie@example.com"})

-- 查询数据
local adults = wingman.db.table_where(users, "age", ">=", "18")
local results = wingman.db.query_all(adults)
for i, user in ipairs(results) do
    print(user.name .. ": " .. user.email)
end

-- 更新数据
local bob = wingman.db.table_where(users, "name", "=", "Bob")
wingman.db.query_update(bob, {age = "31"})

-- 删除数据
local young = wingman.db.table_where(users, "age", "<", "20")
local deleted = wingman.db.query_delete(young)
print("Deleted " .. deleted .. " young users")

-- 计数
local count = wingman.db.table_count(users)
print("Total users: " .. count)

-- 使用底层 SQL
local rows = wingman.db.query(conn, "SELECT * FROM users WHERE age > ?", {25})
local scalarCount = wingman.db.scalar(conn, "SELECT COUNT(*) FROM users")

-- 事务
wingman.db.transaction(conn, function(tx)
    wingman.db.execute(tx, "INSERT INTO users (name, age) VALUES (?, ?)", {"David", 28})
    wingman.db.execute(tx, "UPDATE users SET age = ? WHERE name = ?", {29, "David"})
end)

-- 关闭连接
wingman.db.close(conn)
```

## 安全注意事项

1. **SQL 注入防护**
   - 始终使用参数绑定（`?` 占位符），禁止字符串拼接 SQL
   - ORM 层自动处理参数绑定

2. **标识符验证**
   - 表名和字段名只允许 `[A-Za-z_][A-Za-z0-9_]*` 格式
   - 操作符白名单：`=, !=, >, >=, <, <=, like, in`

3. **资源限制**
   - 查询默认返回最多 1000 行，可通过 `maxRows` 参数调整（`db.query` 无上限钳制；10000 上限仅在 `query_limit`）
   - 大数据量查询应使用分页（`limit` + `orderBy`）

4. **路径限制**
   - 不允许打开任意文件路径
   - 所有数据库文件位于 runtime 管理目录内

## 错误处理

db 模块操作失败时**不抛出异常**，而是通过返回值报告：`execute`/`transaction` 等失败返回 `false`，查询类失败返回空结果，标量失败返回 `nil`/`0`（失败详情会记录到 runtime 日志）。因此 try-catch/pcall 捕获不到任何 db 失败，**必须检查返回值**：

**Python:**

```python
ok = db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Alice"])
if not ok:
    print("Database error: insert failed")
```

**Lua:**

```lua
local db = wingman.db  -- 模块挂在 wingman 表下，此处取局部别名
local ok = db.execute(conn, "INSERT INTO users (name) VALUES (?)", {"Alice"})
if not ok then
    print("Database error: insert failed")
end
```
