#include "wingman/script/modules/db_connection.hpp"
#include "wingman/script/iscript_engine.hpp"
#include <spdlog/spdlog.h>
#include <regex>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif

namespace wingman {
namespace script {
namespace modules {

// ========== Path Utilities ==========

namespace {
	// 缓存数据目录路径
	std::string g_scriptDataDir;

	std::string getDataDirImpl() {
		if (!g_scriptDataDir.empty()) {
			return g_scriptDataDir;
		}

#ifdef _WIN32
		// Windows: 使用 %APPDATA%/wingman/scripts
		PWSTR path = nullptr;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
			std::filesystem::path appData(path);
			CoTaskMemFree(path);
			g_scriptDataDir = (appData / "wingman" / "scripts").string();
		} else {
			// Fallback: 当前目录
			g_scriptDataDir = (std::filesystem::current_path() / "scripts").string();
		}
#else
		// Unix: 使用 ~/.local/share/wingman/scripts
		const char* home = std::getenv("HOME");
		if (home) {
			g_scriptDataDir = (std::filesystem::path(home) / ".local" / "share" / "wingman" / "scripts").string();
		} else {
			g_scriptDataDir = (std::filesystem::current_path() / "scripts").string();
		}
#endif

		// 确保目录存在
		try {
			std::filesystem::create_directories(g_scriptDataDir);
		} catch (const std::exception& e) {
			spdlog::warn("Failed to create script data directory: {}", e.what());
		}

		return g_scriptDataDir;
	}
} // anonymous namespace

std::string getScriptDataDir() {
	return getDataDirImpl();
}

std::string getDatabasePath(const std::string& name) {
	if (name == ":memory:") {
		return ":memory:";
	}

	const std::string dataDir = getScriptDataDir();
	std::filesystem::path dbPath = std::filesystem::path(dataDir) / (name + ".db");

	// 规范化路径
	try {
		return std::filesystem::absolute(dbPath).string();
	} catch (const std::exception& e) {
		spdlog::error("Failed to resolve database path: {}", e.what());
		return dbPath.string();
	}
}

// ========== DbConnection Implementation ==========

#ifdef WINGMAN_HAS_SQLITE

namespace {
	// 标识符验证正则表达式
	const std::regex g_identifierRegex("^[A-Za-z_][A-Za-z0-9_]*$");

	// 操作符白名单
	const std::unordered_set<std::string> g_validOperators = {
		"=", "!=", ">", ">=", "<", "<=", "like", "in"
	};
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
	if (path == ":memory:") {
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

	int rc = sqlite3_open(m_path.c_str(), &m_db);
	if (rc != SQLITE_OK) {
		spdlog::error("Failed to open database '{}': {}", m_path, sqlite3_errmsg(m_db));
		m_db = nullptr;
		return;
	}

	// 启用外键约束
	sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

	spdlog::debug("Opened database: {}", m_path);
}

DbConnection::~DbConnection() {
	close();
}

void DbConnection::close() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_db) {
		sqlite3_close(m_db);
		m_db = nullptr;
		spdlog::debug("Closed database: {}", m_path);
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

DbConnection::Stmt::Stmt(Stmt&& other) noexcept : m_stmt(other.m_stmt) {
	other.m_stmt = nullptr;
}

DbConnection::Stmt& DbConnection::Stmt::operator=(Stmt&& other) noexcept {
	if (this != &other) {
		if (m_stmt) {
			sqlite3_finalize(m_stmt);
		}
		m_stmt = other.m_stmt;
		other.m_stmt = nullptr;
	}
	return *this;
}

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
	std::lock_guard<std::mutex> lock(m_mutex);

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
	std::lock_guard<std::mutex> lock(m_mutex);

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
	std::lock_guard<std::mutex> lock(m_mutex);

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
	std::lock_guard<std::mutex> lock(m_mutex);

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
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_db) {
		return sqlite3_last_insert_rowid(m_db);
	}
	return 0;
}

int DbConnection::changes() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_db) {
		return sqlite3_changes(m_db);
	}
	return 0;
}

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
	std::transform(lowerOp.begin(), lowerOp.end(), lowerOp.begin(), ::tolower);
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

// ========== Connection Registry ==========

namespace {
	// 连接注册表（用于追踪和复用连接）
	std::unordered_map<std::string, std::shared_ptr<DbConnection>> g_connections;
	std::mutex g_connectionsMutex;

	std::shared_ptr<DbConnection> getOrCreateConnection(const std::string& name) {
		std::lock_guard<std::mutex> lock(g_connectionsMutex);

		auto it = g_connections.find(name);
		if (it != g_connections.end()) {
			return it->second;
		}

		std::string dataDir = getScriptDataDir();
		auto conn = std::make_shared<DbConnection>(name, dataDir);

		if (conn->isValid()) {
			g_connections[name] = conn;
			return conn;
		}

		return nullptr;
	}

	void closeConnection(const std::string& name) {
		std::lock_guard<std::mutex> lock(g_connectionsMutex);
		g_connections.erase(name);
	}

	void closeAllConnections() {
		std::lock_guard<std::mutex> lock(g_connectionsMutex);
		g_connections.clear();
	}

	// 表注册表
	std::unordered_map<uintptr_t, std::shared_ptr<DbTable>> g_tables;
	std::mutex g_tablesMutex;

	// 用于追踪 connection 指针到 name 的映射
	std::unordered_map<uintptr_t, std::string> g_connectionPtrToName;
	std::mutex g_connectionPtrToNameMutex;

	std::string getConnectionName(void* ptr) {
		std::lock_guard<std::mutex> lock(g_connectionPtrToNameMutex);
		auto it = g_connectionPtrToName.find(reinterpret_cast<uintptr_t>(ptr));
		if (it != g_connectionPtrToName.end()) {
			return it->second;
		}
		return "";
	}

	void storeConnectionName(void* ptr, const std::string& name) {
		std::lock_guard<std::mutex> lock(g_connectionPtrToNameMutex);
		g_connectionPtrToName[reinterpret_cast<uintptr_t>(ptr)] = name;
	}

	void removeConnectionName(void* ptr) {
		std::lock_guard<std::mutex> lock(g_connectionPtrToNameMutex);
		g_connectionPtrToName.erase(reinterpret_cast<uintptr_t>(ptr));
	}
} // anonymous namespace

// ========== Module Functions ==========

static ScriptValue dbOpen(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::null();
	}

	std::string name = args[0].asString();
	if (name.empty()) {
		name = ":memory:";
	}

	auto conn = getOrCreateConnection(name);
	if (!conn) {
		return ScriptValue::null();
	}

	// 存储 connection 指针和名称的映射（用于后续 close）
	// 注意：这里用 conn.get() 的地址作为 key
	storeConnectionName(conn.get(), name);

	// 返回 connection 对象（作为 opaque pointer 包装在 ScriptValue 中）
	// 使用 Object 类型存储连接指针
	std::unordered_map<std::string, ScriptValue> obj;
	obj["__conn_ptr__"] = ScriptValue::fromInt(reinterpret_cast<int64_t>(conn.get()));
	obj["__conn_name__"] = ScriptValue::fromString(name);

	return ScriptValue::fromObject(std::move(obj));
}

static DbConnection* extractConnection(const ScriptValue& value) {
	if (value.type != ScriptValue::Object) {
		return nullptr;
	}

	const ScriptValue* ptrVal = value.get("__conn_ptr__");
	if (!ptrVal || ptrVal->type != ScriptValue::Int) {
		return nullptr;
	}

	return reinterpret_cast<DbConnection*>(ptrVal->asInt());
}

static ScriptValue dbExecute(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromBool(false);
	}

	std::string sql = args[1].asString();
	Params params;

	// 解析参数数组
	if (args.size() > 2 && args[2].type == ScriptValue::Array) {
		for (const auto& param : args[2].arrayVal) {
			params.push_back(param.asString());
		}
	}

	return ScriptValue::fromBool(conn->execute(sql, params));
}

static ScriptValue dbQuery(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromArray({});
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromArray({});
	}

	std::string sql = args[1].asString();
	Params params;

	if (args.size() > 2 && args[2].type == ScriptValue::Array) {
		for (const auto& param : args[2].arrayVal) {
			params.push_back(param.asString());
		}
	}

	size_t maxRows = kDefaultMaxRows;
	if (args.size() > 3 && args[3].type == ScriptValue::Int) {
		maxRows = static_cast<size_t>(args[3].asInt());
	}

	auto rows = conn->query(sql, params, maxRows);

	// 转换为 ScriptValue 数组
	std::vector<ScriptValue> result;
	for (const auto& row : rows) {
		std::unordered_map<std::string, ScriptValue> obj;
		for (const auto& [key, value] : row) {
			obj[key] = ScriptValue::fromString(value);
		}
		result.push_back(ScriptValue::fromObject(std::move(obj)));
	}

	return ScriptValue::fromArray(std::move(result));
}

static ScriptValue dbScalar(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromString("");
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromString("");
	}

	std::string sql = args[1].asString();
	Params params;

	if (args.size() > 2 && args[2].type == ScriptValue::Array) {
		for (const auto& param : args[2].arrayVal) {
			params.push_back(param.asString());
		}
	}

	std::string result = conn->scalar(sql, params);
	return ScriptValue::fromString(result);
}

static ScriptValue dbTransaction(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromBool(false);
	}

	// args[1] 应该是一个 callable
	if (args[1].type != ScriptValue::Callable) {
		return ScriptValue::fromBool(false);
	}

	bool success = conn->transaction([&args, &conn](DbConnection& txConn) {
		// 调用回调函数，传入 connection 对象
		std::unordered_map<std::string, ScriptValue> obj;
		obj["__conn_ptr__"] = ScriptValue::fromInt(reinterpret_cast<int64_t>(&txConn));
		obj["__conn_name__"] = ScriptValue::fromString(conn->getName());

		ScriptValue connObj = ScriptValue::fromObject(std::move(obj));
		args[1].callableVal({connObj});
	});

	return ScriptValue::fromBool(success);
}

static ScriptValue dbLastInsertId(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromInt(0);
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromInt(0);
	}

	return ScriptValue::fromInt(conn->lastInsertId());
}

static ScriptValue dbChanges(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromInt(0);
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromInt(0);
	}

	return ScriptValue::fromInt(conn->changes());
}

static ScriptValue dbClose(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromBool(false);
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::fromBool(false);
	}

	std::string name = getConnectionName(conn);
	if (!name.empty()) {
		closeConnection(name);
		removeConnectionName(conn);
		return ScriptValue::fromBool(true);
	}

	return ScriptValue::fromBool(false);
}

static ScriptValue dbTable(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::null();
	}

	DbConnection* conn = extractConnection(args[0]);
	if (!conn) {
		return ScriptValue::null();
	}

	std::string tableName = args[1].asString();
	if (tableName.empty()) {
		return ScriptValue::null();
	}

	// 从注册表中查找或创建 shared_ptr
	auto sharedConn = getOrCreateConnection(conn->getName());
	if (!sharedConn) {
		return ScriptValue::null();
	}

	auto table = std::make_shared<DbTable>(sharedConn, tableName);

	// 存储表指针
	std::lock_guard<std::mutex> lock(g_tablesMutex);
	g_tables[reinterpret_cast<uintptr_t>(table.get())] = table;

	// 返回 table 对象
	std::unordered_map<std::string, ScriptValue> obj;
	obj["__table_ptr__"] = ScriptValue::fromInt(reinterpret_cast<int64_t>(table.get()));
	obj["__table_name__"] = ScriptValue::fromString(tableName);

	return ScriptValue::fromObject(std::move(obj));
}

static DbTable* extractTable(const ScriptValue& value) {
	if (value.type != ScriptValue::Object) {
		return nullptr;
	}

	const ScriptValue* ptrVal = value.get("__table_ptr__");
	if (!ptrVal || ptrVal->type != ScriptValue::Int) {
		return nullptr;
	}

	return reinterpret_cast<DbTable*>(ptrVal->asInt());
}

static ScriptValue tableCreate(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::fromBool(false);
	}

	if (args[1].type != ScriptValue::Object) {
		return ScriptValue::fromBool(false);
	}

	Schema schema;
	for (const auto& [field, typeVal] : args[1].objectVal) {
		schema[field] = typeVal.asString();
	}

	return ScriptValue::fromBool(table->create(schema));
}

static ScriptValue tableInsert(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::fromBool(false);
	}

	if (args[1].type != ScriptValue::Object) {
		return ScriptValue::fromBool(false);
	}

	Row row;
	for (const auto& [field, value] : args[1].objectVal) {
		row[field] = value.asString();
	}

	return ScriptValue::fromBool(table->insert(row));
}

static ScriptValue tableGet(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::null();
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::null();
	}

	std::string id = args[1].asString();
	Row row = table->get(id);

	if (row.empty()) {
		return ScriptValue::null();
	}

	std::unordered_map<std::string, ScriptValue> obj;
	for (const auto& [key, value] : row) {
		obj[key] = ScriptValue::fromString(value);
	}

	return ScriptValue::fromObject(std::move(obj));
}

static ScriptValue tableWhere(const std::vector<ScriptValue>& args) {
	if (args.size() < 4) {
		return ScriptValue::null();
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::null();
	}

	std::string field = args[1].asString();
	std::string op = args[2].asString();
	std::string value = args[3].asString();

	auto builder = std::make_shared<QueryBuilder>(table->where(field, op, value));

	// 返回 QueryBuilder 对象
	std::unordered_map<std::string, ScriptValue> obj;
	obj["__query_ptr__"] = ScriptValue::fromInt(reinterpret_cast<int64_t>(builder.get()));

	return ScriptValue::fromObject(std::move(obj));
}

static QueryBuilder* extractQuery(const ScriptValue& value) {
	if (value.type != ScriptValue::Object) {
		return nullptr;
	}

	const ScriptValue* ptrVal = value.get("__query_ptr__");
	if (!ptrVal || ptrVal->type != ScriptValue::Int) {
		return nullptr;
	}

	return reinterpret_cast<QueryBuilder*>(ptrVal->asInt());
}

static ScriptValue queryAll(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromArray({});
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return ScriptValue::fromArray({});
	}

	auto rows = query->all();

	std::vector<ScriptValue> result;
	for (const auto& row : rows) {
		std::unordered_map<std::string, ScriptValue> obj;
		for (const auto& [key, value] : row) {
			obj[key] = ScriptValue::fromString(value);
		}
		result.push_back(ScriptValue::fromObject(std::move(obj)));
	}

	return ScriptValue::fromArray(std::move(result));
}

static ScriptValue queryFirst(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::null();
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return ScriptValue::null();
	}

	Row row = query->first();

	if (row.empty()) {
		return ScriptValue::null();
	}

	std::unordered_map<std::string, ScriptValue> obj;
	for (const auto& [key, value] : row) {
		obj[key] = ScriptValue::fromString(value);
	}

	return ScriptValue::fromObject(std::move(obj));
}

static ScriptValue queryCount(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromInt(0);
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return ScriptValue::fromInt(0);
	}

	return ScriptValue::fromInt(query->count());
}

static ScriptValue queryUpdate(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return ScriptValue::fromBool(false);
	}

	if (args[1].type != ScriptValue::Object) {
		return ScriptValue::fromBool(false);
	}

	Row row;
	for (const auto& [field, value] : args[1].objectVal) {
		row[field] = value.asString();
	}

	return ScriptValue::fromBool(query->update(row));
}

static ScriptValue queryDelete(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromInt(0);
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return ScriptValue::fromInt(0);
	}

	return ScriptValue::fromInt(query->deleteRows());
}

static ScriptValue queryLimit(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return args[0];  // 返回原对象
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return args[0];
	}

	size_t limit = kDefaultMaxRows;
	if (args[1].type == ScriptValue::Int) {
		limit = static_cast<size_t>(args[1].asInt());
	}

	query->limit(limit);
	return args[0];  // 返回原对象（支持链式调用）
}

static ScriptValue queryOrderBy(const std::vector<ScriptValue>& args) {
	if (args.size() < 3) {
		return args[0];
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return args[0];
	}

	std::string field = args[1].asString();
	std::string direction = args[2].asString();

	query->orderBy(field, direction);
	return args[0];  // 返回原对象（支持链式调用）
}

static ScriptValue tableAll(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromArray({});
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::fromArray({});
	}

	auto rows = table->all();

	std::vector<ScriptValue> result;
	for (const auto& row : rows) {
		std::unordered_map<std::string, ScriptValue> obj;
		for (const auto& [key, value] : row) {
			obj[key] = ScriptValue::fromString(value);
		}
		result.push_back(ScriptValue::fromObject(std::move(obj)));
	}

	return ScriptValue::fromArray(std::move(result));
}

static ScriptValue tableCount(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromInt(0);
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::fromInt(0);
	}

	return ScriptValue::fromInt(table->count());
}

// ========== Module Descriptor ==========

ModuleDescriptor createDbModule() {
	ModuleDescriptor mod;
	mod.name = "db";

	// 底层 db 函数
	mod.functions.push_back({"open", dbOpen, "name:string -> connection"});
	mod.functions.push_back({"execute", dbExecute, "conn:connection, sql:string, params?:array -> bool"});
	mod.functions.push_back({"query", dbQuery, "conn:connection, sql:string, params?:array, maxRows?:int -> [{...}]"});
	mod.functions.push_back({"scalar", dbScalar, "conn:connection, sql:string, params?:array -> string"});
	mod.functions.push_back({"transaction", dbTransaction, "conn:connection, callback:function -> bool"});
	mod.functions.push_back({"last_insert_id", dbLastInsertId, "conn:connection -> int"});
	mod.functions.push_back({"changes", dbChanges, "conn:connection -> int"});
	mod.functions.push_back({"close", dbClose, "conn:connection -> bool"});

	// ORM table 函数
	mod.functions.push_back({"table", dbTable, "conn:connection, tableName:string -> table"});

	// table 方法（通过第一个参数识别）
	mod.functions.push_back({"table_create", tableCreate, "table:table, schema:object -> bool"});
	mod.functions.push_back({"table_insert", tableInsert, "table:table, row:object -> bool"});
	mod.functions.push_back({"table_get", tableGet, "table:table, id:string -> object|null"});
	mod.functions.push_back({"table_where", tableWhere, "table:table, field:string, op:string, value:string -> query"});
	mod.functions.push_back({"table_all", tableAll, "table:table -> [{...}]"});
	mod.functions.push_back({"table_count", tableCount, "table:table -> int"});

	// query 方法（链式调用）
	mod.functions.push_back({"query_all", queryAll, "query:query -> [{...}]"});
	mod.functions.push_back({"query_first", queryFirst, "query:query -> object|null"});
	mod.functions.push_back({"query_count", queryCount, "query:query -> int"});
	mod.functions.push_back({"query_update", queryUpdate, "query:query, row:object -> bool"});
	mod.functions.push_back({"query_delete", queryDelete, "query:query -> int"});
	mod.functions.push_back({"query_limit", queryLimit, "query:query, n:int -> query"});
	mod.functions.push_back({"query_order_by", queryOrderBy, "query:query, field:string, direction:string -> query"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
