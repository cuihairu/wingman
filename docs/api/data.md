# 数据存储

Wingman 提供两种本地数据存储方案：kv 模块和 db 模块。两者各有优势，适用于不同场景。

## 模块对比

| 特性 | kv 模块 | db 模块 |
|------|---------|---------|
| **数据模型** | 键值对（Redis-like） | 关系型数据库（SQLite） |
| **查询能力** | 简单键查询 | SQL 查询、索引、关联 |
| **数据结构** | String、Hash、List | 自定义表结构 |
| **适用场景** | 缓存、会话状态、队列 | 结构化数据、复杂查询 |
| **性能** | 极快（内存优先） | 较快（磁盘持久化） |
| **学习曲线** | 低（简单 API） | 中（需了解 SQL） |

## 选择指南

### 使用 kv 模块

**适合场景：**
- 简单键值缓存（如 token、配置）
- 会话状态存储
- 简单队列（List 操作）
- 需要过期时间的临时数据
- 快速原型开发

**示例：**

```python
from wingman import kv

# 简单缓存
kv.set("api_token", "abc123")
kv.set("user_session", "session_id", {"ttl": 3600})  # 1小时过期

# Hash 存储结构化数据
kv.hset("user:1001", "name", "Alice")
kv.hset("user:1001", "level", "50")
user = kv.hgetall("user:1001")

# 队列
kv.lpush("task_queue", "task1")
task = kv.lpop("task_queue")
```

### 使用 db 模块

**适合场景：**
- 需要复杂查询的数据（如按条件筛选、排序）
- 多表关联数据
- 需要事务保证的数据一致性
- 大量结构化数据
- 需要聚合统计（COUNT、SUM、AVG 等）

**示例：**

```python
from wingman import db

conn = db.open("local")
players = db.table(conn, "players")

# 创建表
db.table_create(players, {
    "id": "INTEGER PRIMARY KEY",
    "name": "TEXT NOT NULL",
    "age": "INTEGER",
    "score": "INTEGER"
})

# 插入数据
db.table_insert(players, {"name": "Alice", "age": 25, "score": 1000})

# 复杂查询
top_players = (db.table_where(players, "score", ">", "500")
               .query_order_by("score", "desc")
               .query_limit(10)
               .query_all())

# 聚合查询
count = db.scalar(conn, "SELECT COUNT(*) FROM players WHERE age > ?", [18])
avg_score = db.scalar(conn, "SELECT AVG(score) FROM players")
```

## 混合使用

kv 和 db 可以同时使用，各司其职：

```python
from wingman import kv, db

# kv 用于缓存（快速访问、自动过期）
kv.set("last_login:user1001", "2024-01-01", {"ttl": 86400})

# db 用于持久化（结构化存储）
conn = db.open("local")
users = db.table(conn, "users")
db.table_insert(users, {"id": "1001", "name": "Alice", "email": "alice@example.com"})
```

## 模块详情

- [kv 模块文档](./kv) - 键值存储完整 API
- [db 模块文档](./db) - SQLite 数据库完整 API

## 最佳实践

### 命名规范

**kv 模块：**
- 使用冒号分隔命名空间：`user:1001:name`
- Hash 使用统一前缀：`user:1001` 作为 hash key
- 队列使用 `queue:` 前缀

**db 模块：**
- 表名使用复数形式：`users`、`products`
- 字段名使用 snake_case：`created_at`、`user_id`
- 主键统一命名为 `id`

### 性能优化

**kv 模块：**
- 合理使用 TTL 避免数据堆积
- 批量操作使用管道（未来支持）

**db 模块：**
- 为常用查询字段创建索引
- 使用事务批量操作
- 查询使用 LIMIT 避免大结果集
- 频繁查询的数据考虑用 kv 缓存

### 数据迁移

如需在 kv 和 db 间迁移数据：

```python
# kv → db
for key in kv.keys("user:*"):
    data = kv.hgetall(key)
    db.table_insert(users, data)

# db → kv
for row in db.table_all(users):
    kv.hset(f"user:{row['id']}", "name", row['name'])
    kv.hset(f"user:{row['id']}", "email", row['email'])
```

## 注意事项

1. **数据安全**
   - kv 和 db 的数据文件位于 `%APPDATA%/wingman/scripts/`（Windows）
   - 卸载 Wingman 不会删除这些文件，需手动清理

2. **并发访问**
   - kv 和 db 都是线程安全的，可在多线程环境使用
   - 不同脚本的数据不共享

3. **容量限制**
   - kv: 内存存储，受系统内存限制
   - db: 磁盘存储，受磁盘空间限制

4. **备份建议**
   - 重要数据定期导出
   - db 可直接复制 `.db` 文件备份
   - kv 可调用持久化接口（如果支持）
