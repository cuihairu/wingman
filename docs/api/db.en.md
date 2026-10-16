> [中文](db.md) | **English**

# Database

This document is lengthy (764 lines in the source) and has no English translation yet.

The section outline below mirrors the Chinese source document; see [db.md](db.md) for the original.

- 设计约束
- 路径映射
- 底层 API
  - db.open(name)
  - db.execute(conn, sql, params?)
  - db.query(conn, sql, params?, maxRows?)
  - db.scalar(conn, sql, params?)
  - db.transaction(conn, callback)
  - db.last_insert_id(conn)
  - db.changes(conn)
  - db.close(conn)
- ORM API
  - db.table(conn, tableName)
  - table.create(schema)
  - table.insert(row)
  - table.get(id)
  - table.where(field, op, value)
  - table.all()
  - table.count()
- QueryBuilder API
  - query.all()
  - query.first()
  - query.count()
  - query.update(row)
  - query.delete()
  - query.limit(n)
  - query.orderBy(field, direction?)
  - table.close(t)
  - query.close(q)
- 完整示例
  - Python
  - Lua
- 安全注意事项
- 错误处理
