#include "wingman/script/modules/db_connection.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/platform/app_paths.hpp"
#include <spdlog/spdlog.h>
#include <mutex>

namespace wingman {
namespace script {
namespace modules {

// ========== Path Utilities ==========

namespace {
	// 缓存数据目录路径 - 使用 call_once 保证线程安全
	std::string g_scriptDataDir;
	std::mutex g_scriptDataDirMutex;
	std::once_flag g_scriptDataDirOnceFlag;

	std::string getDataDirImpl() {
		if (!g_scriptDataDir.empty()) {
			return g_scriptDataDir;
		}

		// Win: %APPDATA%/wingman/scripts；Unix: ~/.local/share/wingman/scripts
		// （基座解析的平台分支收敛在 platform::roamingAppDataDir）
		const std::filesystem::path base = platform::roamingAppDataDir();
		if (!base.empty()) {
			g_scriptDataDir = (base / "wingman" / "scripts").string();
		} else {
			// Fallback: 当前目录
			g_scriptDataDir = (std::filesystem::current_path() / "scripts").string();
		}

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
	std::call_once(g_scriptDataDirOnceFlag, []() {
		g_scriptDataDir = getDataDirImpl();
	});
	return g_scriptDataDir;
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

	// QueryBuilder 注册表（用于防止悬空指针）
	std::unordered_map<uintptr_t, std::shared_ptr<QueryBuilder>> g_queries;
	std::mutex g_queriesMutex;

	void storeQuery(uintptr_t ptr, std::shared_ptr<QueryBuilder> query) {
		std::lock_guard<std::mutex> lock(g_queriesMutex);
		g_queries[ptr] = query;
	}

	std::shared_ptr<QueryBuilder> getQuery(uintptr_t ptr) {
		std::lock_guard<std::mutex> lock(g_queriesMutex);
		auto it = g_queries.find(ptr);
		if (it != g_queries.end()) {
			return it->second;
		}
		return nullptr;
	}

	void removeQuery(uintptr_t ptr) {
		std::lock_guard<std::mutex> lock(g_queriesMutex);
		g_queries.erase(ptr);
	}

	// DbTable 注册表访问函数：dbTable 胶水创建的表对象统一经此存取，
	// table_close 负责释放，extractTable 依赖注册表校验句柄有效性
	std::shared_ptr<DbTable> getTable(uintptr_t ptr) {
		std::lock_guard<std::mutex> lock(g_tablesMutex);
		auto it = g_tables.find(ptr);
		if (it != g_tables.end()) {
			return it->second;
		}
		return nullptr;
	}

	void storeTable(uintptr_t ptr, std::shared_ptr<DbTable> table) {
		std::lock_guard<std::mutex> lock(g_tablesMutex);
		g_tables[ptr] = table;
	}

	void removeTable(uintptr_t ptr) {
		std::lock_guard<std::mutex> lock(g_tablesMutex);
		g_tables.erase(ptr);
	}
} // anonymous namespace

// ========== Module Functions ==========
/// ========== Helper Functions ==========

/**
 * 将 ScriptValue 转换为字符串（支持 Int/Float/Bool 类型）
 */
static std::string scriptValueToString(const ScriptValue& value) {
	switch (value.type) {
		case ScriptValue::Type::String:
			return value.asString();
		case ScriptValue::Type::Int:
			return std::to_string(value.asInt());
		case ScriptValue::Type::Float:
			return std::to_string(value.asFloat());
		case ScriptValue::Type::Bool:
			return value.asBool() ? "1" : "0";
		case ScriptValue::Type::Null:
			return "";
		default:
			return value.asString();
	}
}


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
	if (value.type != ScriptValue::Type::Object) {
		return nullptr;
	}

	const ScriptValue* ptrVal = value.get("__conn_ptr__");
	if (!ptrVal || ptrVal->type != ScriptValue::Type::Int) {
		return nullptr;
	}

	DbConnection* conn = reinterpret_cast<DbConnection*>(ptrVal->asInt());

	// 验证连接是否仍在注册表中（防止 use-after-free）
	if (!conn || getConnectionName(conn).empty()) {
		spdlog::warn("extractConnection: invalid or closed connection");
		return nullptr;
	}

	return conn;
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
	if (args.size() > 2 && args[2].type == ScriptValue::Type::Array) {
		for (const auto& param : args[2].arrayVal) {
			params.push_back(scriptValueToString(param));
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

	if (args.size() > 2 && args[2].type == ScriptValue::Type::Array) {
		for (const auto& param : args[2].arrayVal) {
			params.push_back(scriptValueToString(param));
		}
	}

	size_t maxRows = kDefaultMaxRows;
	if (args.size() > 3 && args[3].type == ScriptValue::Type::Int) {
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

	if (args.size() > 2 && args[2].type == ScriptValue::Type::Array) {
		for (const auto& param : args[2].arrayVal) {
			params.push_back(scriptValueToString(param));
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
	if (args[1].type != ScriptValue::Type::Callable) {
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
	storeTable(reinterpret_cast<uintptr_t>(table.get()), table);

	// 返回 table 对象
	std::unordered_map<std::string, ScriptValue> obj;
	obj["__table_ptr__"] = ScriptValue::fromInt(reinterpret_cast<int64_t>(table.get()));
	obj["__table_name__"] = ScriptValue::fromString(tableName);

	return ScriptValue::fromObject(std::move(obj));
}

static DbTable* extractTable(const ScriptValue& value) {
	if (value.type != ScriptValue::Type::Object) {
		return nullptr;
	}

	const ScriptValue* ptrVal = value.get("__table_ptr__");
	if (!ptrVal || ptrVal->type != ScriptValue::Type::Int) {
		return nullptr;
	}

	// 从注册表中获取（防止 use-after-free：table_close 释放后句柄失效）
	auto table = getTable(static_cast<uintptr_t>(ptrVal->asInt()));
	if (!table) {
		return nullptr;
	}

	return table.get();
}

static ScriptValue tableCreate(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return ScriptValue::fromBool(false);
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::fromBool(false);
	}

	if (args[1].type != ScriptValue::Type::Object) {
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

	if (args[1].type != ScriptValue::Type::Object) {
		return ScriptValue::fromBool(false);
	}

	Row row;
	for (const auto& [field, value] : args[1].objectVal) {
		row[field] = scriptValueToString(value);
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

	// 存储到注册表（防止悬空指针）
	uintptr_t ptr = reinterpret_cast<uintptr_t>(builder.get());
	storeQuery(ptr, builder);

	// 返回 QueryBuilder 对象
	std::unordered_map<std::string, ScriptValue> obj;
	obj["__query_ptr__"] = ScriptValue::fromInt(reinterpret_cast<int64_t>(builder.get()));

	return ScriptValue::fromObject(std::move(obj));
}

static QueryBuilder* extractQuery(const ScriptValue& value) {
	if (value.type != ScriptValue::Type::Object) {
		return nullptr;
	}

	const ScriptValue* ptrVal = value.get("__query_ptr__");
	if (!ptrVal || ptrVal->type != ScriptValue::Type::Int) {
		return nullptr;
	}

	// 从注册表中获取（防止 use-after-free）
	auto query = getQuery(static_cast<uintptr_t>(ptrVal->asInt()));
	if (!query) {
		return nullptr;
	}

	return query.get();
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

	if (args[1].type != ScriptValue::Type::Object) {
		return ScriptValue::fromBool(false);
	}

	Row row;
	for (const auto& [field, value] : args[1].objectVal) {
		row[field] = scriptValueToString(value);
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

	// 拒绝负数
	if (args[1].type == ScriptValue::Type::Int) {
		int64_t val = args[1].asInt();
		if (val < 0) {
			spdlog::warn("queryLimit: negative value {} rejected", val);
			return args[0];
		}
		query->limit(static_cast<size_t>(val));
	} else {
		query->limit(kDefaultMaxRows);
	}

	return args[0];  // 返回原对象（支持链式调用）
}

static ScriptValue queryOrderBy(const std::vector<ScriptValue>& args) {
	if (args.size() < 2) {
		return args[0];
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return args[0];
	}

	std::string field = args[1].asString();
	std::string direction = "asc";  // 默认方向
	if (args.size() >= 3 && !args[2].asString().empty()) {
		direction = args[2].asString();
	}

	query->orderBy(field, direction);
	return args[0];  // 返回原对象（支持链式调用）
}

// 显式释放 table/query 句柄。此前 g_tables/g_queries 只增不删，
// 脚本循环中反复 db.table()/db.table_where() 会持续泄漏对象；
// close 后旧句柄经 extractXxx 的注册表校验被拒绝，不会悬空。
static ScriptValue tableClose(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromBool(false);
	}

	DbTable* table = extractTable(args[0]);
	if (!table) {
		return ScriptValue::fromBool(false);
	}

	removeTable(reinterpret_cast<uintptr_t>(table));
	return ScriptValue::fromBool(true);
}

static ScriptValue queryClose(const std::vector<ScriptValue>& args) {
	if (args.empty()) {
		return ScriptValue::fromBool(false);
	}

	QueryBuilder* query = extractQuery(args[0]);
	if (!query) {
		return ScriptValue::fromBool(false);
	}

	removeQuery(reinterpret_cast<uintptr_t>(query));
	return ScriptValue::fromBool(true);
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
	mod.functions.push_back({"table_close", tableClose, "table:table -> bool"});

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
	mod.functions.push_back({"query_close", queryClose, "query:query -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
