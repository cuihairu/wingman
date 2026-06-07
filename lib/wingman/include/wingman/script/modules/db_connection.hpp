#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <filesystem>

#ifdef WINGMAN_HAS_SQLITE
#include <sqlite3.h>
#endif

namespace wingman {
namespace script {
namespace modules {

// ========== Forward Declarations ==========
class DbConnection;
class DbTable;
class QueryBuilder;

// ========== Type Aliases ==========
using Row = std::unordered_map<std::string, std::string>;
using Rows = std::vector<Row>;
using Params = std::vector<std::string>;
using Schema = std::unordered_map<std::string, std::string>;
using TransactionCallback = std::function<void(DbConnection&)>;

// ========== Constants ==========
constexpr size_t kDefaultMaxRows = 1000;
constexpr size_t kMaxAllowedRows = 10000;

// ========== DbConnection Class ==========
/**
 * DbConnection - 线程安全的 SQLite 数据库连接
 *
 * 设计约束：
 * 1. 只能打开 runtime 管理目录内的数据库或 :memory:
 * 2. 每个 connection 内部 mutex 串行化 sqlite3 调用
 * 3. statement 用 RAII finalize
 * 4. connection 用 RAII close
 * 5. transaction 禁止嵌套
 */
class DbConnection {
public:
	/**
	 * 构造函数
	 * @param name 数据库名称或 ":memory:"
	 * @param dataDir 数据目录路径（用于路径限制）
	 */
	explicit DbConnection(const std::string& name, const std::string& dataDir);

	/**
	 * 析构函数 - 自动关闭连接
	 */
	~DbConnection();

	// 禁止拷贝和移动
	DbConnection(const DbConnection&) = delete;
	DbConnection& operator=(const DbConnection&) = delete;
	DbConnection(DbConnection&&) = delete;
	DbConnection& operator=(DbConnection&&) = delete;

	/**
	 * 执行 SQL 语句（无结果返回）
	 * @param sql SQL 语句，使用 ? 作为参数占位符
	 * @param params 参数列表
	 * @return 是否成功
	 */
	bool execute(const std::string& sql, const Params& params = {});

	/**
	 * 查询 SQL 语句（返回多行）
	 * @param sql SQL 语句，使用 ? 作为参数占位符
	 * @param params 参数列表
	 * @param maxRows 最大返回行数（默认 kDefaultMaxRows）
	 * @return 查询结果
	 */
	Rows query(const std::string& sql, const Params& params = {}, size_t maxRows = kDefaultMaxRows);

	/**
	 * 查询单个值（标量）
	 * @param sql SQL 语句，使用 ? 作为参数占位符
	 * @param params 参数列表
	 * @return 查询结果（第一行第一列）
	 */
	std::string scalar(const std::string& sql, const Params& params = {});

	/**
	 * 执行事务
	 * @param callback 事务回调函数
	 * @return 是否成功
	 */
	bool transaction(TransactionCallback callback);

	/**
	 * 获取最后插入的行 ID
	 */
	int64_t lastInsertId() const;

	/**
	 * 获取受影响的行数
	 */
	int changes() const;

	/**
	 * 检查连接是否有效
	 */
	bool isValid() const;

	/**
	 * 关闭连接
	 */
	void close();

	/**
	 * 获取数据库路径
	 */
	const std::string& getPath() const { return m_path; }

	/**
	 * 获取数据库名称
	 */
	const std::string& getName() const { return m_name; }

private:
#ifdef WINGMAN_HAS_SQLITE
	/**
	 * 解析数据库路径
	 * @param name 数据库名称或 ":memory:"
	 * @param dataDir 数据目录
	 * @return 实际文件路径或 ":memory:"
	 */
	static std::string resolvePath(const std::string& name, const std::string& dataDir);

	/**
	 * 验证路径是否允许
	 * @param path 路径
	 * @param dataDir 数据目录
	 * @return 是否允许
	 */
	static bool isPathAllowed(const std::string& path, const std::string& dataDir);

	/**
	 * RAII Statement 封装
	 */
	class Stmt {
	public:
		explicit Stmt(sqlite3_stmt* stmt);
		~Stmt();

		Stmt(const Stmt&) = delete;
		Stmt& operator=(const Stmt&) = delete;
		Stmt(Stmt&& other) noexcept;
		Stmt& operator=(Stmt&& other) noexcept;

		sqlite3_stmt* get() const { return m_stmt; }
		explicit operator bool() const { return m_stmt != nullptr; }

	private:
		sqlite3_stmt* m_stmt = nullptr;
	};

	/**
	 * 准备语句
	 */
	Stmt prepare(const std::string& sql);

	/**
	 * 绑定参数
	 */
	bool bindParams(Stmt& stmt, const Params& params);

	/**
	 * 获取查询结果
	 */
	Rows fetchResults(Stmt& stmt, size_t maxRows);

	sqlite3* m_db = nullptr;
	std::string m_name;
	std::string m_path;
	std::string m_dataDir;
	mutable std::mutex m_mutex;
	bool m_inTransaction = false;
#else
	std::string m_name;
	std::string m_path;
#endif // WINGMAN_HAS_SQLITE
};

// ========== DbTable Class ==========
/**
 * DbTable - ORM 表助手
 *
 * 提供简单的 ORM 接口：
 * - create() - 创建表
 * - insert() - 插入行
 * - get() - 按 ID 查询
 * - where() - 构建查询
 */
class DbTable {
public:
	/**
	 * 构造函数
	 * @param connection 数据库连接
	 * @param tableName 表名
	 */
	DbTable(std::shared_ptr<DbConnection> connection, const std::string& tableName);

	/**
	 * 创建表
	 * @param schema 表结构定义 {字段名: 类型定义}
	 * @return 是否成功
	 */
	bool create(const Schema& schema);

	/**
	 * 插入行
	 * @param row 行数据 {字段名: 值}
	 * @return 是否成功
	 */
	bool insert(const Row& row);

	/**
	 * 按 ID 查询
	 * @param id ID 值
	 * @return 查询结果
	 */
	Row get(const std::string& id);

	/**
	 * 构建查询
	 * @param field 字段名
	 * @param op 操作符 (=, !=, >, >=, <, <=, like, in)
	 * @param value 值
	 * @return QueryBuilder
	 */
	QueryBuilder where(const std::string& field, const std::string& op, const std::string& value);

	/**
	 * 获取所有行（带默认 limit）
	 * @return 所有行（最多 kDefaultMaxRows 行）
	 */
	Rows all();

	/**
	 * 计数
	 * @return 行数
	 */
	int64_t count();

	/**
	 * 获取表名
	 */
	const std::string& getTableName() const { return m_tableName; }

	/**
	 * 获取连接
	 */
	std::shared_ptr<DbConnection> getConnection() const { return m_connection; }

	/**
	 * 验证表名/字段名是否合法
	 * @param name 名称
	 * @return 是否合法
	 */
	static bool isValidIdentifier(const std::string& name);

	/**
	 * 验证操作符是否合法
	 * @param op 操作符
	 * @return 是否合法
	 */
	static bool isValidOperator(const std::string& op);

private:
	std::shared_ptr<DbConnection> m_connection;
	std::string m_tableName;
};

// ========== QueryBuilder Class ==========
/**
 * QueryBuilder - 链式查询构建器
 *
 * 支持链式调用：
 * query.where("age", ">", "20").orderBy("id", "desc").limit(10).all()
 */
class QueryBuilder {
public:
	/**
	 * 构造函数
	 * @param table 表对象
	 */
	explicit QueryBuilder(DbTable* table);

	/**
	 * WHERE 条件
	 * @param field 字段名
	 * @param op 操作符
	 * @param value 值
	 * @return this
	 */
	QueryBuilder& where(const std::string& field, const std::string& op, const std::string& value);

	/**
	 * 排序
	 * @param field 字段名
	 * @param direction 方向 (asc, desc)
	 * @return this
	 */
	QueryBuilder& orderBy(const std::string& field, const std::string& direction = "asc");

	/**
	 * 限制行数
	 * @param limit 行数
	 * @return this
	 */
	QueryBuilder& limit(size_t limit);

	/**
	 * 获取所有结果
	 * @return 查询结果
	 */
	Rows all();

	/**
	 * 获取单行
	 * @return 第一行或空 Row
	 */
	Row first();

	/**
	 * 计数
	 * @return 行数
	 */
	int64_t count();

	/**
	 * 更新匹配的行
	 * @param row 更新数据
	 * @return 是否成功
	 */
	bool update(const Row& row);

	/**
	 * 删除匹配的行
	 * @return 删除的行数
	 */
	int deleteRows();

private:
	/**
	 * 构建 SQL 查询
	 */
	std::string buildQuery(bool forCount = false);

	/**
	 * 获取参数列表
	 */
	Params buildParams();

	DbTable* m_table;
	struct WhereClause {
		std::string field;
		std::string op;
		std::string value;
	};
	std::vector<WhereClause> m_wheres;
	struct OrderClause {
		std::string field;
		std::string direction;
	};
	OrderClause m_order;
	size_t m_limit = kDefaultMaxRows;
	bool m_hasOrder = false;
};

// ========== Path Utilities ==========
/**
 * 获取脚本数据目录
 * @return 数据目录路径
 */
std::string getScriptDataDir();

/**
 * 获取数据库路径
 * @param name 数据库名称
 * @return 数据库文件路径
 */
std::string getDatabasePath(const std::string& name);

} // namespace modules
} // namespace script
} // namespace wingman
