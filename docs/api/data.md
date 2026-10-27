> [中文](../zh/api/data.md) | **English**

# Data Storage

Wingman provides two local data storage options: the kv module and the db module. Each has its own strengths and suits different scenarios.

## Module Comparison

| Feature | kv module | db module |
|------|---------|---------|
| **Data model** | Key-value pairs (Redis-like) | Relational database (SQLite) |
| **Query capability** | Simple key lookups | SQL queries, indexes, joins |
| **Data structures** | String, Hash, List | Custom table schemas |
| **Best suited for** | Caching, session state, queues | Structured data, complex queries |
| **Performance** | Extremely fast (memory-first) | Fast (disk-persisted) |
| **Learning curve** | Low (simple API) | Medium (requires SQL knowledge) |

## Selection Guide

### Use the kv module

**Suitable scenarios:**
- Simple key-value caching (e.g. tokens, config)
- Session state storage
- Simple queues (List operations)
- Temporary data that needs expiry
- Rapid prototyping

**Example:**

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

### Use the db module

**Suitable scenarios:**
- Data requiring complex queries (e.g. conditional filtering, sorting)
- Multi-table relational data
- Data consistency requiring transaction guarantees
- Large volumes of structured data
- Aggregate statistics (COUNT, SUM, AVG, etc.)

**Example:**

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

# 复杂查询（函数式：上一步返回的 query 对象作为下一步调用的首参）
where = db.table_where(players, "score", ">", "500")
ordered = db.query_order_by(where, "score", "desc")
top_players = db.query_all(db.query_limit(ordered, 10))

# 聚合查询
count = db.scalar(conn, "SELECT COUNT(*) FROM players WHERE age > ?", [18])
avg_score = db.scalar(conn, "SELECT AVG(score) FROM players")
```

## Mixing Both

kv and db can be used at the same time, each doing its own job:

```python
from wingman import kv, db

# kv 用于缓存（快速访问、自动过期）
kv.set("last_login:user1001", "2024-01-01", {"ttl": 86400})

# db 用于持久化（结构化存储）
conn = db.open("local")
users = db.table(conn, "users")
db.table_insert(users, {"id": "1001", "name": "Alice", "email": "alice@example.com"})
```

## Module Details

- [kv module docs](../zh/api/kv.md) - full key-value storage API
- [db module docs](../zh/api/db.md) - full SQLite database API

## Best Practices

### Naming Conventions

**kv module:**
- Use colon-separated namespaces: `user:1001:name`
- Use a consistent prefix for Hashes: `user:1001` as the hash key
- Prefix queues with `queue:`

**db module:**
- Use plural table names: `users`, `products`
- Use snake_case field names: `created_at`, `user_id`
- Name primary keys uniformly as `id`

### Performance Tuning

**kv module:**
- Use TTLs wisely to avoid unbounded data accumulation
- Use pipelining for bulk operations (future support)

**db module:**
- Create indexes on frequently queried fields
- Use transactions for bulk operations
- Use LIMIT in queries to avoid large result sets
- Consider caching frequently queried data in kv

### Data Migration

To migrate data between kv and db:

```python
# db → kv
for row in db.table_all(users):
    kv.hset(f"user:{row['id']}", "name", row['name'])
    kv.hset(f"user:{row['id']}", "email", row['email'])
```

Note: the kv module currently exposes no key-enumeration interface to the script layer (no `keys`/`scan`), so bulk migration in the "kv → db" direction requires maintaining your own list of key names, calling `hgetall` on each, and writing the results into db.

## Notes

1. **Data safety**
   - db data files live in `%APPDATA%/wingman/scripts/` (Windows) or `~/.local/share/wingman/scripts/` (Linux/macOS)
   - kv is normally pure in-memory storage; the persistence location is entirely determined by the path passed to `kv.save(path)` / `kv.enableAutoSave(path)` — there is no default directory
   - Uninstalling Wingman does not delete these files; they must be cleaned up manually

2. **Concurrent access**
   - Both kv and db are thread-safe and can be used in multi-threaded environments
   - Connections to a same-named database are shared process-wide and visible across scripts; use a different db name when isolation is needed

3. **Capacity limits**
   - kv: in-memory storage, limited by system memory
   - db: on-disk storage, limited by disk space

4. **Backup advice**
   - Export important data regularly
   - db can be backed up by directly copying the `.db` file
   - kv can be persisted to a chosen file via `kv.save(path)` or `kv.enableAutoSave(path)`
