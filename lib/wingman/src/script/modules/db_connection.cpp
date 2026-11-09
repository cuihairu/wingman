#include "wingman/script/modules/db_connection.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>  // std::transform（显式包含，不依赖传递包含）
#include <regex>
#include <sstream>
#include <unordered_set>

namespace wingman {
namespace script {
namespace modules {


#ifdef WINGMAN_HAS_SQLITE

namespace {
	// 标识符验证正则表达式
	const std::regex g_identifierRegex("^[A-Za-z_][A-Za-z0-9_]*$");

	// 操作符白名单
	const std::unordered_set<std::string> g_validOperators = {
		"=", "!=", ">", ">=", "<", "<=", "like", "in"
	};

	// SQL 类型白名单验证（用于防止注入）
	bool isValidTypeDef(const std::string& typeDef) {
		for (char c : typeDef) {
			if (c == '(' || c == ')' || c == ';' || c == '\'' || c == '"' || c == '-' || c == '/') {
				return false;
			}
		}
		return true;
	}
} // anonymous namespace

std::string DbConnection::resolvePath(const std::string& name, const std::string& dataDir) {
	if (name == ":memory:") {
		return ":memory:";
	}

	// 禁止绝对路径
	if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
		name.find("..") != std::string::npos) {
		spdlog::error("Invalid database name: '{}'. Only simple names and ':memory:' are allowed.", name);
		return "";
	}

	// 构建 dataDir/name.db 路径
	std::filesystem::path dbPath = std::filesystem::path(dataDir) / (name + ".db");

	try {
		return std::filesystem::absolute(dbPath).string();
	} catch (const std::exception& e) {
		spdlog::error("Failed to resolve database path: {}", e.what());
		return "";
	}
}

bool DbConnection::isPathAllowed(const std::string& path, const std::string& dataDir) {
	// 允许 :memory: 开头的特殊名称（用于隔离）
	if (path.find(":memory:") == 0) {
		return true;
	}

	try {
		auto absPath = std::filesystem::absolute(path);
		auto absDataDir = std::filesystem::absolute(dataDir);

		// 检查路径是否在 dataDir 内
		auto [aEnd, aDirEnd] = std::mismatch(absPath.begin(), absPath.end(), absDataDir.begin(), absDataDir.end());
		if (aDirEnd != absDataDir.end()) {
			return false;  // dataDir 不是前缀
		}

		return true;
	} catch (const std::exception&) {
		return false;
	}
}

DbConnection::DbConnection(const std::string& name, const std::string& dataDir)
	: m_name(name), m_dataDir(dataDir) {

	m_path = resolvePath(name, dataDir);
	if (m_path.empty() || !isPathAllowed(m_path, dataDir)) {
		spdlog::error("Database path not allowed: '{}'", m_path);
		return;
	}

	sqlite3* db = nullptr;
	int rc = sqlite3_open(m_path.c_str(), &db);
	if (rc != SQLITE_OK) {
		spdlog::error("Failed to open database '{}': {}", m_path, db ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
		// 修复内存泄漏：打开失败时必须关闭 sqlite3 返回的句柄
		if (db) {
			sqlite3_close(db);
		}
		m_db = nullptr;
		return;
	}

	m_db = db;

	// 启用外键约束
	sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

	spdlog::debug("Opened database: {}", m_path);
}

DbConnection::~DbConnection() {
	close();
}

void DbConnection::close() {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_db) {
		sqlite3_close(m_db);
		m_db = nullptr;
		// 不打日志：close 会从 ~DbConnection 触发，进程退出阶段静态对象的
		// 析构顺序不定，spdlog registry 可能已先析构（ASan 实测对已销毁
		// logger 的 heap-use-after-free）。析构路径禁止打日志。
	}
}

bool DbConnection::isValid() const {
	return m_db != nullptr;
}

DbConnection::Stmt::Stmt(sqlite3_stmt* stmt) : m_stmt(stmt) {}

DbConnection::Stmt::~Stmt() {
	if (m_stmt) {
		sqlite3_finalize(m_stmt);
	}
}

// Stmt 不提供移动语义（声明处已说明）：prepare() 以纯右值返回，
// C++17 强制省略拷贝保证不调用移动构造，故此处无移动操作定义。

DbConnection::Stmt DbConnection::prepare(const std::string& sql) {
	if (!m_db) {
		return Stmt(nullptr);
	}

	sqlite3_stmt* stmt = nullptr;
	const char* tail = nullptr;
	int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, &tail);
	if (rc != SQLITE_OK) {
		spdlog::error("Failed to prepare SQL: {}", sqlite3_errmsg(m_db));
		return Stmt(nullptr);
	}

	return Stmt(stmt);
}

bool DbConnection::bindParams(Stmt& stmt, const Params& params) {
	if (!stmt) {
		return false;
	}

	int idx = 1;
	for (const auto& param : params) {
		int rc = sqlite3_bind_text(stmt.get(), idx++, param.c_str(), -1, SQLITE_TRANSIENT);
		if (rc != SQLITE_OK) {
			spdlog::error("Failed to bind parameter {}: {}", idx - 1, sqlite3_errmsg(m_db));
			return false;
		}
	}

	return true;
}

Rows DbConnection::fetchResults(Stmt& stmt, size_t maxRows) {
	Rows results;

	if (!stmt) {
		return results;
	}

	int columnCount = sqlite3_column_count(stmt.get());

	while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
		if (results.size() >= maxRows) {
			spdlog::warn("Query reached max rows limit: {}", maxRows);
			break;
		}

		Row row;
		for (int i = 0; i < columnCount; ++i) {
			const char* name = sqlite3_column_name(stmt.get(), i);
			const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), i));
			if (name) {
				row[name] = value ? value : "";
			}
		}
		results.push_back(std::move(row));
	}

	return results;
}

bool DbConnection::execute(const std::string& sql, const Params& params) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!m_db) {
		return false;
	}

	auto stmt = prepare(sql);
	if (!stmt) {
		return false;
	}

	if (!bindParams(stmt, params)) {
		return false;
	}

	int rc = sqlite3_step(stmt.get());
	if (rc != SQLITE_DONE) {
		spdlog::error("Failed to execute SQL: {}", sqlite3_errmsg(m_db));
		return false;
	}

	return true;
}

Rows DbConnection::query(const std::string& sql, const Params& params, size_t maxRows) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!m_db) {
		return {};
	}

	auto stmt = prepare(sql);
	if (!stmt) {
		return {};
	}

	if (!bindParams(stmt, params)) {
		return {};
	}

	return fetchResults(stmt, maxRows);
}

std::string DbConnection::scalar(const std::string& sql, const Params& params) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!m_db) {
		return "";
	}

	auto stmt = prepare(sql);
	if (!stmt) {
		return "";
	}

	if (!bindParams(stmt, params)) {
		return "";
	}

	if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
		const char* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
		return value ? value : "";
	}

	return "";
}

bool DbConnection::transaction(TransactionCallback callback) {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!m_db || m_inTransaction) {
		spdlog::error("Cannot start transaction: database invalid or already in transaction");
		return false;
	}

	// 开始事务
	if (sqlite3_exec(m_db, "BEGIN;", nullptr, nullptr, nullptr) != SQLITE_OK) {
		spdlog::error("Failed to begin transaction: {}", sqlite3_errmsg(m_db));
		return false;
	}

	m_inTransaction = true;

	// 执行回调
	try {
		callback(*this);

		// 提交事务
		if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
			spdlog::error("Failed to commit transaction: {}", sqlite3_errmsg(m_db));
			sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
			m_inTransaction = false;
			return false;
		}

		m_inTransaction = false;
		return true;
	} catch (const std::exception& e) {
		spdlog::error("Transaction callback failed: {}", e.what());
		sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
		m_inTransaction = false;
		return false;
	}
}

int64_t DbConnection::lastInsertId() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_db) {
		return sqlite3_last_insert_rowid(m_db);
	}
	return 0;
}

int DbConnection::changes() const {
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (m_db) {
		return sqlite3_changes(m_db);
	}
	return 0;
}

// 事务回调内直接使用加锁版 execute/query/scalar：m_mutex 为
// recursive_mutex，回调重入加锁版不会死锁，曾经的 Unlocked 变体
// 从未有任何调用方（死代码），已于 2026-09 移除。

#else // !WINGMAN_HAS_SQLITE

DbConnection::DbConnection(const std::string& name, const std::string& dataDir)
	: m_name(name), m_path(":memory:") {
	spdlog::error("SQLite support not compiled in");
}

DbConnection::~DbConnection() = default;

void DbConnection::close() {}

bool DbConnection::isValid() const { return false; }

bool DbConnection::execute(const std::string&, const Params&) { return false; }
Rows DbConnection::query(const std::string&, const Params&, size_t) { return {}; }
std::string DbConnection::scalar(const std::string&, const Params&) { return ""; }
bool DbConnection::transaction(TransactionCallback) { return false; }
int64_t DbConnection::lastInsertId() const { return 0; }
int DbConnection::changes() const { return 0; }

#endif // WINGMAN_HAS_SQLITE

// ========== DbTable Implementation ==========

DbTable::DbTable(std::shared_ptr<DbConnection> connection, const std::string& tableName)
	: m_connection(connection), m_tableName(tableName) {

	if (!isValidIdentifier(tableName)) {
		spdlog::error("Invalid table name: '{}'", tableName);
	}
}

bool DbTable::isValidIdentifier(const std::string& name) {
	return std::regex_match(name, g_identifierRegex);
}

bool DbTable::isValidOperator(const std::string& op) {
	std::string lowerOp = op;
	std::transform(lowerOp.begin(), lowerOp.end(), lowerOp.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return g_validOperators.count(lowerOp) > 0;
}

bool DbTable::create(const Schema& schema) {
	if (!m_connection || !m_connection->isValid()) {
		return false;
	}

	// 验证表名
	if (!isValidIdentifier(m_tableName)) {
		spdlog::error("Invalid table name: '{}'", m_tableName);
		return false;
	}

	// 构建 CREATE TABLE 语句
	std::ostringstream sql;
	sql << "CREATE TABLE IF NOT EXISTS " << m_tableName << " (";

	bool first = true;
	for (const auto& [field, typeDef] : schema) {
		if (!isValidIdentifier(field)) {
			spdlog::error("Invalid field name: '{}'", field);
			return false;
		}

		// 验证类型定义（防止 SQL 注入）
		if (!isValidTypeDef(typeDef)) {
			spdlog::error("Invalid type definition for field '{}': '{}'. Only basic types and simple constraints allowed.", field, typeDef);
			return false;
		}

		if (!first) sql << ", ";
		first = false;

		sql << field << " " << typeDef;
	}

	sql << ")";

	return m_connection->execute(sql.str());
}

bool DbTable::insert(const Row& row) {
	if (!m_connection || !m_connection->isValid() || row.empty()) {
		return false;
	}

	// 验证表名
	if (!isValidIdentifier(m_tableName)) {
		return false;
	}

	// 构建 INSERT 语句
	std::ostringstream sql;
	sql << "INSERT INTO " << m_tableName << " (";

	// 验证字段名并构建列列表
	std::vector<std::string> fields;
	std::vector<std::string> placeholders;

	for (const auto& [field, value] : row) {
		if (!isValidIdentifier(field)) {
			spdlog::error("Invalid field name: '{}'", field);
			return false;
		}
		fields.push_back(field);
		placeholders.push_back("?");
	}

	// 写入列名
	for (size_t i = 0; i < fields.size(); ++i) {
		if (i > 0) sql << ", ";
		sql << fields[i];
	}

	sql << ") VALUES (";

	// 写入占位符
	for (size_t i = 0; i < placeholders.size(); ++i) {
		if (i > 0) sql << ", ";
		sql << placeholders[i];
	}

	sql << ")";

	// 构建参数
	Params params;
	for (const auto& field : fields) {
		auto it = row.find(field);
		if (it != row.end()) {
			params.push_back(it->second);
		}
	}

	return m_connection->execute(sql.str(), params);
}

Row DbTable::get(const std::string& id) {
	if (!m_connection || !m_connection->isValid()) {
		return {};
	}

	// 假设第一个主键字段名为 'id'
	std::string sql = "SELECT * FROM " + m_tableName + " WHERE id = ? LIMIT 1";
	auto results = m_connection->query(sql, {id}, 1);

	if (!results.empty()) {
		return results[0];
	}

	return {};
}

QueryBuilder DbTable::where(const std::string& field, const std::string& op, const std::string& value) {
	QueryBuilder builder(this);
	builder.where(field, op, value);
	return builder;
}

Rows DbTable::all() {
	if (!m_connection || !m_connection->isValid()) {
		return {};
	}

	std::string sql = "SELECT * FROM " + m_tableName + " LIMIT " + std::to_string(kDefaultMaxRows);
	return m_connection->query(sql);
}

int64_t DbTable::count() {
	if (!m_connection || !m_connection->isValid()) {
		return 0;
	}

	std::string sql = "SELECT COUNT(*) FROM " + m_tableName;
	std::string result = m_connection->scalar(sql);

	try {
		return std::stoll(result);
	} catch (...) {
		return 0;
	}
}

// ========== QueryBuilder Implementation ==========

QueryBuilder::QueryBuilder(DbTable* table) : m_table(table) {}

QueryBuilder& QueryBuilder::where(const std::string& field, const std::string& op, const std::string& value) {
	if (DbTable::isValidIdentifier(field) && DbTable::isValidOperator(op)) {
		m_wheres.push_back({field, op, value});
	} else {
		spdlog::warn("Invalid WHERE clause: field='{}', op='{}'", field, op);
	}
	return *this;
}

QueryBuilder& QueryBuilder::orderBy(const std::string& field, const std::string& direction) {
	if (DbTable::isValidIdentifier(field) &&
		(direction == "asc" || direction == "desc" ||
		 direction == "ASC" || direction == "DESC")) {

		m_order.field = field;
		m_order.direction = direction;
		m_hasOrder = true;
	}
	return *this;
}

QueryBuilder& QueryBuilder::limit(size_t limit) {
	m_limit = std::min(limit, kMaxAllowedRows);
	return *this;
}

std::string QueryBuilder::buildQuery(bool forCount) {
	if (!m_table) {
		return "";
	}

	const std::string& tableName = m_table->getTableName();

	if (forCount) {
		std::ostringstream sql;
		sql << "SELECT COUNT(*) FROM " << tableName;

		if (!m_wheres.empty()) {
			sql << " WHERE ";
			for (size_t i = 0; i < m_wheres.size(); ++i) {
				if (i > 0) sql << " AND ";
				sql << m_wheres[i].field << " " << m_wheres[i].op << " ?";
			}
		}

		return sql.str();
	}

	std::ostringstream sql;
	sql << "SELECT * FROM " << tableName;

	if (!m_wheres.empty()) {
		sql << " WHERE ";
		for (size_t i = 0; i < m_wheres.size(); ++i) {
			if (i > 0) sql << " AND ";
			sql << m_wheres[i].field << " " << m_wheres[i].op << " ?";
		}
	}

	if (m_hasOrder) {
		sql << " ORDER BY " << m_order.field << " " << m_order.direction;
	}

	sql << " LIMIT " << m_limit;

	return sql.str();
}

Params QueryBuilder::buildParams() {
	Params params;
	for (const auto& where : m_wheres) {
		params.push_back(where.value);
	}
	return params;
}

Rows QueryBuilder::all() {
	if (!m_table || !m_table->getConnection()) {
		return {};
	}

	std::string sql = buildQuery(false);
	Params params = buildParams();

	return m_table->getConnection()->query(sql, params, m_limit);
}

Row QueryBuilder::first() {
	auto results = all();
	if (!results.empty()) {
		return results[0];
	}
	return {};
}

int64_t QueryBuilder::count() {
	if (!m_table || !m_table->getConnection()) {
		return 0;
	}

	std::string sql = buildQuery(true);
	Params params = buildParams();

	std::string result = m_table->getConnection()->scalar(sql, params);

	try {
		return std::stoll(result);
	} catch (...) {
		return 0;
	}
}

bool QueryBuilder::update(const Row& row) {
	if (!m_table || !m_table->getConnection() || row.empty()) {
		return false;
	}

	const std::string& tableName = m_table->getTableName();

	// 构建 UPDATE 语句
	std::ostringstream sql;
	sql << "UPDATE " << tableName << " SET ";

	// 分离 SET 字段和 WHERE 字段
	std::vector<std::string> setFields;
	std::vector<std::string> setValues;

	for (const auto& [field, value] : row) {
		if (DbTable::isValidIdentifier(field)) {
			setFields.push_back(field);
			setValues.push_back(value);
		}
	}

	if (setFields.empty()) {
		return false;
	}

	// 写入 SET 子句
	for (size_t i = 0; i < setFields.size(); ++i) {
		if (i > 0) sql << ", ";
		sql << setFields[i] << " = ?";
	}

	// 写入 WHERE 子句
	if (!m_wheres.empty()) {
		sql << " WHERE ";
		for (size_t i = 0; i < m_wheres.size(); ++i) {
			if (i > 0) sql << " AND ";
			sql << m_wheres[i].field << " " << m_wheres[i].op << " ?";
		}
	}

	// 合并参数：SET 值 + WHERE 值
	Params params;
	params.insert(params.end(), setValues.begin(), setValues.end());
	Params whereParams = buildParams();
	params.insert(params.end(), whereParams.begin(), whereParams.end());

	return m_table->getConnection()->execute(sql.str(), params);
}

int QueryBuilder::deleteRows() {
	if (!m_table || !m_table->getConnection()) {
		return 0;
	}

	const std::string& tableName = m_table->getTableName();

	std::ostringstream sql;
	sql << "DELETE FROM " << tableName;

	if (!m_wheres.empty()) {
		sql << " WHERE ";
		for (size_t i = 0; i < m_wheres.size(); ++i) {
			if (i > 0) sql << " AND ";
			sql << m_wheres[i].field << " " << m_wheres[i].op << " ?";
		}
	}

	Params params = buildParams();

	if (m_table->getConnection()->execute(sql.str(), params)) {
		return m_table->getConnection()->changes();
	}

	return 0;
}

} // namespace modules
} // namespace script
} // namespace wingman
