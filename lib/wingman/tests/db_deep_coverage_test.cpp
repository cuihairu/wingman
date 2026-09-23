#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include "wingman/script/modules/db_connection.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// db_module.cpp 第八批覆盖率收口（2026-09-23）：第七批后 gcov 显示 DbConnection
// 的 open 失败/关闭后拒绝/坏 SQL/嵌套事务防御、DbTable 与 QueryBuilder 的全部
// 校验分支、以及胶水层伪造句柄（非 Int 指针字段、注册表未命中、空参数）仍未
// 覆盖。本文件 C++ 直驱 public 类 + 经 ModuleDescriptor 直调胶水函数两种路径。
// 同时为新增的 table_close/query_close（修复 g_tables/g_queries 只增不删泄漏）
// 建立生命周期与失效防护用例。

namespace {

	ModuleDescriptor getModule(const std::string& name) {
		for (auto& mod : getAllModules()) {
			if (mod.name == name) return mod;
		}
		return {};
	}

	const ModuleDescriptor::FunctionEntry* findFunction(const ModuleDescriptor& mod, const std::string& name) {
		for (const auto& f : mod.functions) {
			if (f.name == name) return &f;
		}
		return nullptr;
	}

	ScriptValue makeObj(std::unordered_map<std::string, ScriptValue> fields) {
		return ScriptValue::fromObject(std::move(fields));
	}

	std::string uniqueSuffix() {
		return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	}

	// DbConnection 禁止拷贝，独立打开新的内存库
	std::shared_ptr<DbConnection> openMem() {
		return std::make_shared<DbConnection>(":memory:", getScriptDataDir());
	}

} // anonymous namespace

// ========== C++ 层：DbConnection 错误路径 ==========

class DbDeepConnectionTest : public ::testing::Test {
protected:
	void SetUp() override {
		const auto* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
		testDbName = "__test_db_deep__" + std::string(testInfo->test_suite_name()) + "_" +
					 testInfo->name() + "_" + uniqueSuffix();
		conn = std::make_shared<DbConnection>(testDbName, getScriptDataDir());
		ASSERT_TRUE(conn->isValid());
	}

	void TearDown() override {
		if (conn) {
			conn->close();
			conn.reset();
		}
		std::string dbPath = getDatabasePath(testDbName);
		if (dbPath != ":memory:" && std::filesystem::exists(dbPath)) {
			std::filesystem::remove(dbPath);
		}
	}

	std::shared_ptr<DbConnection> conn;
	std::string testDbName;
};

// open 失败：目标路径已存在且是目录，sqlite3 返回 CANTOPEN，
// 构造函数必须吞掉句柄（避免泄漏 sqlite3 分配的 db 结构）
TEST_F(DbDeepConnectionTest, OpenFailsWhenTargetPathIsDirectory) {
	auto trapDir = std::filesystem::temp_directory_path() /
				   ("wm_db_trap_" + std::string(::testing::UnitTest::GetInstance()
													 ->current_test_info()
													 ->name()) +
					"_" + uniqueSuffix());
	std::filesystem::create_directories(trapDir / "trap.db");

	{
		DbConnection bad("trap", trapDir.string());
		EXPECT_FALSE(bad.isValid());
		EXPECT_FALSE(bad.execute("CREATE TABLE t (id INTEGER)"));
		EXPECT_TRUE(bad.query("SELECT 1").empty());
		EXPECT_EQ(bad.scalar("SELECT 1"), "");
	}

	std::filesystem::remove_all(trapDir);
}

// close 后所有操作统一拒绝：execute/query/scalar/transaction 与
// lastInsertId/changes 的无效连接分支
TEST_F(DbDeepConnectionTest, ClosedConnectionRejectsAllOperations) {
	conn->close();
	ASSERT_FALSE(conn->isValid());

	EXPECT_FALSE(conn->execute("CREATE TABLE t (id INTEGER)"));
	EXPECT_TRUE(conn->query("SELECT 1").empty());
	EXPECT_EQ(conn->scalar("SELECT 1"), "");
	EXPECT_FALSE(conn->transaction([](DbConnection&) {}));
	EXPECT_EQ(conn->lastInsertId(), 0);
	EXPECT_EQ(conn->changes(), 0);
}

// 坏 SQL：prepare 失败沿 execute/query/scalar 三个入口全部返回默认值
TEST_F(DbDeepConnectionTest, BadSqlFailsAllEntryPoints) {
	static const char* kBadSql = "THIS IS NOT SQL;;;";
	EXPECT_FALSE(conn->execute(kBadSql));
	EXPECT_TRUE(conn->query(kBadSql).empty());
	EXPECT_EQ(conn->scalar(kBadSql), "");

	// 好表坏列：prepare 同样失败
	EXPECT_FALSE(conn->execute("SELECT missing_col FROM no_such_table"));
}

// scalar 无匹配行返回空串（SQLITE_ROW 分支未命中）
TEST_F(DbDeepConnectionTest, ScalarWithoutRowsReturnsEmpty) {
	ASSERT_TRUE(conn->execute("CREATE TABLE t (id INTEGER)"));
	ASSERT_TRUE(conn->execute("INSERT INTO t (id) VALUES (1)"));
	EXPECT_EQ(conn->scalar("SELECT id FROM t WHERE id = 999"), "");
}

// 嵌套事务拒绝：回调内再次 transaction 走 "already in transaction" 分支，
// 且内层回调不会被执行（拒绝发生在 BEGIN 之前）
TEST_F(DbDeepConnectionTest, TransactionRejectsNestedCall) {
	bool innerCallbackRan = false;
	ASSERT_TRUE(conn->transaction([this, &innerCallbackRan](DbConnection& tx) {
		EXPECT_FALSE(tx.transaction([&innerCallbackRan](DbConnection&) {
			innerCallbackRan = true;
		}));
	}));
	EXPECT_FALSE(innerCallbackRan);
}

// query 的 maxRows 截断：命中 "reached max rows limit" 分支
TEST_F(DbDeepConnectionTest, QueryMaxRowsTruncates) {
	ASSERT_TRUE(conn->execute("CREATE TABLE t (id INTEGER)"));
	for (int i = 0; i < 5; ++i) {
		ASSERT_TRUE(conn->execute("INSERT INTO t (id) VALUES (?)", {std::to_string(i)}));
	}
	auto rows = conn->query("SELECT id FROM t", {}, 2);
	EXPECT_EQ(rows.size(), 2u);
	// 不带限制时全量返回
	EXPECT_EQ(conn->query("SELECT id FROM t").size(), 5u);
}

// ========== C++ 层：DbTable 校验分支 ==========

// 无效连接下 table 的五个入口统一拒绝
TEST(DbDeepTableTest, InvalidConnectionGuards) {
	DbTable nullConnTable(nullptr, "t");
	EXPECT_FALSE(nullConnTable.create({{"id", "INTEGER"}}));
	EXPECT_FALSE(nullConnTable.insert({{"id", "1"}}));
	EXPECT_TRUE(nullConnTable.get("1").empty());
	EXPECT_TRUE(nullConnTable.all().empty());
	EXPECT_EQ(nullConnTable.count(), 0);

	// 已关闭的真实连接同样走 isValid() 分支
	auto closed = openMem();
	closed->close();
	DbTable closedConnTable(closed, "t");
	EXPECT_FALSE(closedConnTable.create({{"id", "INTEGER"}}));
	EXPECT_FALSE(closedConnTable.insert({{"id", "1"}}));
	EXPECT_TRUE(closedConnTable.get("1").empty());
	EXPECT_TRUE(closedConnTable.all().empty());
	EXPECT_EQ(closedConnTable.count(), 0);
}

// 非法表名/字段名/类型定义：注入防护分支
TEST(DbDeepTableTest, InjectionGuards) {
	DbTable badName(openMem(), "bad name; DROP");
	EXPECT_FALSE(badName.create({{"id", "INTEGER"}}));
	EXPECT_FALSE(badName.insert({{"id", "1"}}));

	DbTable table(openMem(), "t");
	ASSERT_TRUE(table.create({{"id", "INTEGER"}, {"v", "TEXT"}}));

	// 非法字段名
	EXPECT_FALSE(table.create({{"bad field", "INTEGER"}}));
	EXPECT_FALSE(table.insert({{"bad field", "x"}}));
	// 非法类型定义（含分号 → isValidTypeDef 拒绝）
	EXPECT_FALSE(table.create({{"id", "INTEGER; DROP TABLE t"}}));
	// 空行
	EXPECT_FALSE(table.insert({}));
}

// ========== C++ 层：QueryBuilder 校验分支 ==========

TEST(DbDeepQueryBuilderTest, InvalidWhereAndOrderClauses) {
	auto connPtr = openMem();
	DbTable table(connPtr, "t");
	ASSERT_TRUE(table.create({{"id", "INTEGER"}, {"v", "TEXT"}}));
	ASSERT_TRUE(table.insert({{"id", "1"}, {"v", "a"}}));
	ASSERT_TRUE(table.insert({{"id", "2"}, {"v", "b"}}));

	// 非法操作符被忽略（warn 分支），结果退化为全表
	auto badOp = table.where("id", "bogus", "1");
	EXPECT_EQ(badOp.all().size(), 2u);
	// 非法字段名同样被忽略
	auto badField = table.where("bad field", "=", "1");
	EXPECT_EQ(badField.all().size(), 2u);

	// 大小写方向均合法
	EXPECT_EQ(table.where("id", ">", "0").orderBy("id", "DESC").all().size(), 2u);
	EXPECT_EQ(table.where("id", ">", "0").orderBy("id", "ASC").all().size(), 2u);
	// 非法方向/非法字段被静默忽略
	EXPECT_EQ(table.where("id", ">", "0").orderBy("id", "sideways").all().size(), 2u);
	EXPECT_EQ(table.where("id", ">", "0").orderBy("bad field", "asc").all().size(), 2u);
}

TEST(DbDeepQueryBuilderTest, NullTableGuardsAndEmptyResults) {
	// 空表指针：all/count/deleteRows 统一防御
	QueryBuilder nullQ(nullptr);
	EXPECT_TRUE(nullQ.all().empty());
	EXPECT_TRUE(nullQ.first().empty());
	EXPECT_EQ(nullQ.count(), 0);
	EXPECT_EQ(nullQ.deleteRows(), 0);
	EXPECT_FALSE(nullQ.update({{"id", "1"}}));

	// 真实连接但无匹配行：first 空结果分支
	DbTable table(openMem(), "t");
	ASSERT_TRUE(table.create({{"id", "INTEGER"}}));
	auto noMatch = table.where("id", "=", "999");
	EXPECT_TRUE(noMatch.first().empty());
	EXPECT_EQ(noMatch.count(), 0);
}

TEST(DbDeepQueryBuilderTest, UpdateValidationAndDeleteFailure) {
	auto connPtr = openMem();
	DbTable table(connPtr, "t");
	ASSERT_TRUE(table.create({{"id", "INTEGER"}, {"v", "TEXT"}}));
	ASSERT_TRUE(table.insert({{"id", "1"}, {"v", "a"}}));

	// 空 row / 全非法字段名的 row → update 拒绝
	auto q = table.where("id", "=", "1");
	EXPECT_FALSE(q.update({}));
	EXPECT_FALSE(q.update({{"bad field", "x"}}));

	// 合法 update 命中且生效
	EXPECT_TRUE(q.update({{"v", "b"}}));
	EXPECT_EQ(table.get("1")["v"], "b");

	// 非法表名 → DELETE 语句 SQL 语法错误 → execute 失败 → 返回 0
	DbTable badTable(connPtr, "bad name; DROP");
	EXPECT_EQ(badTable.where("id", "=", "1").deleteRows(), 0);
}

// ========== 胶水层：伪造句柄与注册表防护 ==========

class DbDeepGlueTest : public ::testing::Test {
protected:
	void SetUp() override {
		mod_ = getModule("db");
		ASSERT_FALSE(mod_.name.empty());
	}

	ScriptValue call(const std::string& fn, std::vector<ScriptValue> args) {
		const auto* f = findFunction(mod_, fn);
		EXPECT_NE(f, nullptr);
		if (!f) return ScriptValue::null();
		return (*f)(std::move(args));
	}

	ScriptValue open() {
		return call("open", {ScriptValue::fromString(":memory:")});
	}

	void closeConn(const ScriptValue& conn) {
		call("close", {conn});
	}

	ModuleDescriptor mod_;
};

// __conn_ptr__ 存在但类型非 Int / 指向注册表外 → extractConnection 拒绝
TEST_F(DbDeepGlueTest, ForgedConnectionHandlesRejected) {
	auto conn = open();
	ASSERT_TRUE(conn.isObject());

	ScriptValue stringPtr = makeObj({{"__conn_ptr__", ScriptValue::fromString("not-a-ptr")}});
	EXPECT_FALSE(call("execute", {stringPtr, ScriptValue::fromString("SELECT 1")}).asBool());
	EXPECT_FALSE(call("close", {stringPtr}).asBool());

	// 缺字段
	ScriptValue noField = makeObj({{"other", ScriptValue::fromInt(1)}});
	EXPECT_FALSE(call("execute", {noField, ScriptValue::fromString("SELECT 1")}).asBool());

	// Int 但不在注册表（模拟已释放/伪造句柄）
	ScriptValue stalePtr = makeObj({{"__conn_ptr__", ScriptValue::fromInt(123456789)}});
	EXPECT_FALSE(call("execute", {stalePtr, ScriptValue::fromString("SELECT 1")}).asBool());

	closeConn(conn);
}

// __table_ptr__ 变体：非 Int / 缺字段 / 注册表未命中 → 全部 table_* 拒绝
TEST_F(DbDeepGlueTest, ForgedTableHandlesRejected) {
	EXPECT_FALSE(call("table_create", {ScriptValue::fromString("str"), makeObj({})}).asBool());

	ScriptValue stringPtr = makeObj({{"__table_ptr__", ScriptValue::fromString("x")}});
	EXPECT_FALSE(call("table_create", {stringPtr, makeObj({})}).asBool());

	ScriptValue noField = makeObj({{"other", ScriptValue::fromInt(2)}});
	EXPECT_TRUE(call("table_get", {noField, ScriptValue::fromString("1")}).isNull());

	ScriptValue stalePtr = makeObj({{"__table_ptr__", ScriptValue::fromInt(987654321)}});
	EXPECT_FALSE(call("table_create", {stalePtr, makeObj({})}).asBool());
	EXPECT_FALSE(call("table_insert", {stalePtr, makeObj({})}).asBool());
	EXPECT_TRUE(call("table_get", {stalePtr, ScriptValue::fromString("1")}).isNull());
	EXPECT_TRUE(call("table_where", {stalePtr, ScriptValue::fromString("f"),
									 ScriptValue::fromString("="), ScriptValue::fromString("v")})
					 .isNull());
	EXPECT_EQ(call("table_all", {stalePtr}).size(), 0u);
	EXPECT_EQ(call("table_count", {stalePtr}).asInt(), 0);
}

// __query_ptr__ 变体：缺字段/非 Int/注册表未命中 → 七个 query_* 全部退化
TEST_F(DbDeepGlueTest, ForgedQueryHandlesRejected) {
	ScriptValue noField = makeObj({{"other", ScriptValue::fromInt(3)}});
	EXPECT_EQ(call("query_all", {noField}).size(), 0u);
	EXPECT_TRUE(call("query_first", {noField}).isNull());

	ScriptValue stringPtr = makeObj({{"__query_ptr__", ScriptValue::fromString("x")}});
	EXPECT_EQ(call("query_count", {stringPtr}).asInt(), 0);

	ScriptValue stalePtr = makeObj({{"__query_ptr__", ScriptValue::fromInt(555555)}});
	EXPECT_EQ(call("query_all", {stalePtr}).size(), 0u);
	EXPECT_TRUE(call("query_first", {stalePtr}).isNull());
	EXPECT_EQ(call("query_count", {stalePtr}).asInt(), 0);
	EXPECT_FALSE(call("query_update", {stalePtr, makeObj({})}).asBool());
	EXPECT_EQ(call("query_delete", {stalePtr}).asInt(), 0);

	// limit/order_by 对无效句柄原样返回（链式退化）
	EXPECT_TRUE(call("query_limit", {stalePtr, ScriptValue::fromInt(5)})
					 .isObject());
	EXPECT_TRUE(call("query_order_by", {stalePtr, ScriptValue::fromString("id"),
										ScriptValue::fromString("asc")})
					 .isObject());
}

// query_* 空参数分支
TEST_F(DbDeepGlueTest, QueryFunctionsEmptyArgs) {
	EXPECT_EQ(call("query_all", {}).size(), 0u);
	EXPECT_TRUE(call("query_first", {}).isNull());
	EXPECT_EQ(call("query_count", {}).asInt(), 0);
	EXPECT_FALSE(call("query_update", {}).asBool());
	EXPECT_EQ(call("query_delete", {}).asInt(), 0);
	EXPECT_FALSE(call("query_close", {}).asBool());
	EXPECT_FALSE(call("table_close", {}).asBool());

	// query_limit 缺第二个参数：原样返回
	ScriptValue q = makeObj({{"__query_ptr__", ScriptValue::fromInt(1)}});
	EXPECT_TRUE(call("query_limit", {q}).isObject());
	EXPECT_TRUE(call("query_order_by", {q}).isObject());
}

// query_limit 负数拒绝与非 Int 回退默认上限
TEST_F(DbDeepGlueTest, QueryLimitValidation) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("CREATE TABLE t (id INTEGER)")}).asBool());
	for (int i = 0; i < 6; ++i) {
		ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("INSERT INTO t (id) VALUES (?)"),
									 ScriptValue::fromArray({ScriptValue::fromInt(i)})})
						 .asBool());
	}
	auto tbl = call("table", {conn, ScriptValue::fromString("t")});
	ASSERT_TRUE(tbl.isObject());
	auto query = call("table_where", {tbl, ScriptValue::fromString("id"),
									  ScriptValue::fromString(">="), ScriptValue::fromString("0")});
	ASSERT_TRUE(query.isObject());

	// 负数拒绝：返回原对象且不截断（全 6 行）
	EXPECT_TRUE(call("query_limit", {query, ScriptValue::fromInt(-5)}).isObject());
	EXPECT_EQ(call("query_all", {query}).size(), 6u);
	// 非 Int：回退 kDefaultMaxRows（不报错）
	EXPECT_TRUE(call("query_limit", {query, ScriptValue::fromString("many")}).isObject());
	EXPECT_EQ(call("query_all", {query}).size(), 6u);

	// order_by 带大写方向与空串方向
	EXPECT_TRUE(call("query_order_by", {query, ScriptValue::fromString("id"),
										ScriptValue::fromString("DESC")})
					 .isObject());
	EXPECT_TRUE(call("query_order_by", {query, ScriptValue::fromString("id"), ScriptValue::fromString("")})
					 .isObject());

	closeConn(conn);
}

// 空表 query_first → null（row.empty 分支）
TEST_F(DbDeepGlueTest, QueryFirstOnEmptyTableReturnsNull) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("CREATE TABLE t (id INTEGER)")}).asBool());
	auto tbl = call("table", {conn, ScriptValue::fromString("t")});
	auto query = call("table_where", {tbl, ScriptValue::fromString("id"),
									  ScriptValue::fromString("="), ScriptValue::fromString("999")});
	EXPECT_TRUE(call("query_first", {query}).isNull());
	EXPECT_EQ(call("query_count", {query}).asInt(), 0);
	closeConn(conn);
}

// query_update 行参数非 Object → false
TEST_F(DbDeepGlueTest, QueryUpdateNonObjectRowRejected) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("CREATE TABLE t (id INTEGER)")}).asBool());
	auto tbl = call("table", {conn, ScriptValue::fromString("t")});
	auto query = call("table_where", {tbl, ScriptValue::fromString("id"),
									  ScriptValue::fromString("="), ScriptValue::fromString("1")});
	EXPECT_FALSE(call("query_update", {query, ScriptValue::fromString("not-an-object")}).asBool());
	closeConn(conn);
}

// db.query / db.scalar 携带参数数组 + 数组元素为 Null/Object 的字符串化
TEST_F(DbDeepGlueTest, QueryAndScalarWithParamArrays) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString(
		"CREATE TABLE t (id INTEGER, a TEXT, b TEXT)")}).asBool());

	// params 数组含 null 与 object 元素 → scriptValueToString 的 Null/default 分支；
	// 第一个参数落在 id 列（整数语义），后两个走异常类型转换
	ScriptValue exotic = ScriptValue::fromArray({
		ScriptValue::fromInt(7),                   // Int → "7"
		ScriptValue::null(),                       // Null → 空串
		makeObj({{"ignored", ScriptValue::fromInt(1)}}), // Object → asString() default
	});
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString(
		"INSERT INTO t (id, a, b) VALUES (?, ?, ?)"), exotic}).asBool());

	// query 携带参数数组（此前只测过无参路径）
	auto rows = call("query", {conn, ScriptValue::fromString("SELECT id FROM t WHERE id = ?"),
							   ScriptValue::fromArray({ScriptValue::fromString("7")})});
	ASSERT_EQ(rows.size(), 1u);

	// scalar 携带参数数组：读回 id 的整数语义（Object/null 元素经 asString()
	// 降级为空串落库，不产生错误——胶水 params 语义是"尽力字符串化"）
	auto id = call("scalar", {conn, ScriptValue::fromString("SELECT id FROM t WHERE a IS NULL OR a = ''"),
							  ScriptValue::fromArray({})});
	EXPECT_EQ(id.asString(), "7");

	closeConn(conn);
}

// open("") 空名回退 :memory:
TEST_F(DbDeepGlueTest, OpenEmptyNameFallsBackToMemory) {
	auto conn = call("open", {ScriptValue::fromString("")});
	ASSERT_TRUE(conn.isObject());
	ASSERT_NE(conn.get("__conn_name__"), nullptr);
	EXPECT_EQ(conn.get("__conn_name__")->asString(), ":memory:");
	closeConn(conn);
}

// ========== 胶水层：table_close / query_close 生命周期（泄漏修复回归） ==========

TEST_F(DbDeepGlueTest, TableCloseReleasesAndInvalidatesHandle) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("CREATE TABLE t (id INTEGER)")}).asBool());
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("INSERT INTO t (id) VALUES (1)")}).asBool());

	auto tbl = call("table", {conn, ScriptValue::fromString("t")});
	ASSERT_TRUE(tbl.isObject());
	EXPECT_EQ(call("table_count", {tbl}).asInt(), 1);

	// 首次 close 成功；此后旧句柄被注册表防护拒绝；重复 close 失败
	EXPECT_TRUE(call("table_close", {tbl}).asBool());
	EXPECT_EQ(call("table_count", {tbl}).asInt(), 0);
	EXPECT_EQ(call("table_all", {tbl}).size(), 0u);
	EXPECT_FALSE(call("table_close", {tbl}).asBool());

	closeConn(conn);
}

TEST_F(DbDeepGlueTest, QueryCloseReleasesAndInvalidatesHandle) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("CREATE TABLE t (id INTEGER)")}).asBool());
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("INSERT INTO t (id) VALUES (1)")}).asBool());

	auto tbl = call("table", {conn, ScriptValue::fromString("t")});
	auto query = call("table_where", {tbl, ScriptValue::fromString("id"),
									  ScriptValue::fromString("="), ScriptValue::fromString("1")});
	ASSERT_TRUE(query.isObject());
	EXPECT_EQ(call("query_count", {query}).asInt(), 1);

	EXPECT_TRUE(call("query_close", {query}).asBool());
	EXPECT_EQ(call("query_count", {query}).asInt(), 0);
	EXPECT_EQ(call("query_all", {query}).size(), 0u);
	EXPECT_FALSE(call("query_close", {query}).asBool());

	closeConn(conn);
}

// 循环创建 table/query 后全部 close：句柄值可能被新对象复用，
// close 必须只影响自己的句柄
TEST_F(DbDeepGlueTest, RepeatedCreateCloseCycles) {
	auto conn = open();
	ASSERT_TRUE(call("execute", {conn, ScriptValue::fromString("CREATE TABLE t (id INTEGER)")}).asBool());

	for (int i = 0; i < 8; ++i) {
		auto tbl = call("table", {conn, ScriptValue::fromString("t")});
		ASSERT_TRUE(tbl.isObject());
		EXPECT_EQ(call("table_count", {tbl}).asInt(), 0);

		// 在 table 仍有效时构建 query 并消费，再依次释放两个句柄
		auto query = call("table_where", {tbl, ScriptValue::fromString("id"),
										  ScriptValue::fromString("="), ScriptValue::fromString("x")});
		ASSERT_TRUE(query.isObject());
		EXPECT_EQ(call("query_count", {query}).asInt(), 0);
		EXPECT_TRUE(call("query_close", {query}).asBool());
		EXPECT_TRUE(call("table_close", {tbl}).asBool());
	}

	closeConn(conn);
}
