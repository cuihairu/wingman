from __future__ import annotations

from typing import Protocol, runtime_checkable

# ========== Type Definitions ==========

class Connection:
	"""数据库连接对象（不直接实例化，通过 db.open() 获取）"""
	pass

class Table:
	"""表对象（不直接实例化，通过 db.table() 获取）"""
	pass

class Query:
	"""查询构建器对象（通过 table.where() 获取）"""
	pass

# 类型别名
Row = dict[str, str]
Rows = list[Row]
Params = list[str]
Schema = dict[str, str]
TransactionCallback = Callable[[], None]

# ========== db Module Functions ==========

def open(name: str) -> Connection:
	"""
	打开数据库连接

	Args:
		name: 数据库名称（预置名称如 "local", "cache"）或 ":memory:"

	Returns:
		数据库连接对象

	Example:
		conn = db.open("local")
		mem_conn = db.open(":memory:")
	"""
	...

def execute(conn: Connection, sql: str, params: Params | None = ...) -> bool:
	"""
	执行 SQL 语句（无结果返回）

	Args:
		conn: 数据库连接对象
		sql: SQL 语句，使用 ? 作为参数占位符
		params: 参数列表

	Returns:
		是否成功

	Example:
		db.execute(conn, "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)")
		db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Alice"])
	"""
	...

def query(conn: Connection, sql: str, params: Params | None = ..., max_rows: int = ...) -> Rows:
	"""
	查询 SQL 语句（返回多行）

	Args:
		conn: 数据库连接对象
		sql: SQL 语句，使用 ? 作为参数占位符
		params: 参数列表
		max_rows: 最大返回行数（默认 1000）

	Returns:
		查询结果（字典列表）

	Example:
		rows = db.query(conn, "SELECT * FROM users WHERE age > ?", [20])
		for row in rows:
			print(row["name"])
	"""
	...

def scalar(conn: Connection, sql: str, params: Params | None = ...) -> str:
	"""
	查询单个值（标量）

	Args:
		conn: 数据库连接对象
		sql: SQL 语句，使用 ? 作为参数占位符
		params: 参数列表

	Returns:
		查询结果（第一行第一列）

	Example:
		count = db.scalar(conn, "SELECT COUNT(*) FROM users")
	"""
	...

def transaction(conn: Connection, callback: Callable[[Connection], None]) -> bool:
	"""
	执行事务

	Args:
		conn: 数据库连接对象
		callback: 事务回调函数

	Returns:
		是否成功

	Example:
		def tx(conn):
			db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Alice"])
			db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Bob"])

		db.transaction(conn, tx)
	"""
	...

def last_insert_id(conn: Connection) -> int:
	"""
	获取最后插入的行 ID

	Args:
		conn: 数据库连接对象

	Returns:
		最后插入的行 ID

	Example:
		db.execute(conn, "INSERT INTO users (name) VALUES (?)", ["Alice"])
		id = db.last_insert_id(conn)
	"""
	...

def changes(conn: Connection) -> int:
	"""
	获取受影响的行数

	Args:
		conn: 数据库连接对象

	Returns:
		受影响的行数

	Example:
		db.execute(conn, "UPDATE users SET age = 30 WHERE name = ?", ["Alice"])
		num = db.changes(conn)
	"""
	...

def close(conn: Connection) -> bool:
	"""
	关闭数据库连接

	Args:
		conn: 数据库连接对象

	Returns:
		是否成功

	Example:
		db.close(conn)
	"""
	...

def table(conn: Connection, table_name: str) -> Table:
	"""
	创建表助手

	Args:
		conn: 数据库连接对象
		table_name: 表名

	Returns:
		表对象

	Example:
		users = db.table(conn, "users")
	"""
	...

# ========== Table Methods ==========

def table_create(table: Table, schema: Schema) -> bool:
	"""
	创建表

	Args:
		table: 表对象
		schema: 表结构定义 {字段名: 类型定义}

	Returns:
		是否成功

	Example:
		db.table_create(users, {
			"id": "INTEGER PRIMARY KEY",
			"name": "TEXT NOT NULL",
			"age": "INTEGER"
		})
	"""
	...

def table_insert(table: Table, row: Row) -> bool:
	"""
	插入行

	Args:
		table: 表对象
		row: 行数据 {字段名: 值}

	Returns:
		是否成功

	Example:
		db.table_insert(users, {"name": "Alice", "age": "25"})
	"""
	...

def table_get(table: Table, id: str) -> Row | None:
	"""
	按 ID 查询

	Args:
		table: 表对象
		id: ID 值

	Returns:
		查询结果或 None

	Example:
		user = db.table_get(users, "1")
	"""
	...

def table_where(table: Table, field: str, op: str, value: str) -> Query:
	"""
	构建查询

	Args:
		table: 表对象
		field: 字段名
		op: 操作符 (=, !=, >, >=, <, <=, like, in)
		value: 值

	Returns:
		查询构建器对象

	Example:
		query = db.table_where(users, "age", ">", "20")
	"""
	...

def table_all(table: Table) -> Rows:
	"""
	获取所有行（带默认 limit）

	Args:
		table: 表对象

	Returns:
		所有行（最多 1000 行）

	Example:
		rows = db.table_all(users)
	"""
	...

def table_count(table: Table) -> int:
	"""
	计数

	Args:
		table: 表对象

	Returns:
		行数

	Example:
		count = db.table_count(users)
	"""
	...

# ========== Query Methods ==========

def query_all(query: Query) -> Rows:
	"""
	获取所有结果

	Args:
		query: 查询构建器对象

	Returns:
		查询结果

	Example:
		rows = db.query_all(query)
	"""
	...

def query_first(query: Query) -> Row | None:
	"""
	获取单行

	Args:
		query: 查询构建器对象

	Returns:
		第一行或 None

	Example:
		row = db.query_first(query)
	"""
	...

def query_count(query: Query) -> int:
	"""
	计数

	Args:
		query: 查询构建器对象

	Returns:
		行数

	Example:
		count = db.query_count(query)
	"""
	...

def query_update(query: Query, row: Row) -> bool:
	"""
	更新匹配的行

	Args:
		query: 查询构建器对象
		row: 更新数据

	Returns:
		是否成功

	Example:
		db.query_update(query, {"age": "26"})
	"""
	...

def query_delete(query: Query) -> int:
	"""
	删除匹配的行

	Args:
		query: 查询构建器对象

	Returns:
		删除的行数

	Example:
		count = db.query_delete(query)
	"""
	...

def query_limit(query: Query, n: int) -> Query:
	"""
	限制行数

	Args:
		query: 查询构建器对象
		n: 行数

	Returns:
		查询构建器对象（支持链式调用）

	Example:
		query = db.query_limit(query, 10)
	"""
	...

def query_order_by(query: Query, field: str, direction: str = ...) -> Query:
	"""
	排序

	Args:
		query: 查询构建器对象
		field: 字段名
		direction: 方向 (asc, desc)

	Returns:
		查询构建器对象（支持链式调用）

	Example:
		query = db.query_order_by(query, "id", "desc")
	"""
	...
