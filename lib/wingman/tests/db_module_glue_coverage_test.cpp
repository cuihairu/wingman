#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script/module_registry.hpp"
#include "wingman/script/iscript_engine.hpp"
#include <stdexcept>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// db_module.cpp 胶水层补测（2026-09-22 覆盖率收口）：现有 db_module_test.cpp
// 直测 DbConnection/QueryBuilder 内部类，ScriptValue 胶水（open/execute/query/
// table_*/query_* 共 23 个导出函数与连接注册表的句柄校验分支）此前大面积未覆盖。
// 本文件经 ModuleDescriptor 直接调用胶水函数。连接一律用 ":memory:"（不落盘），
// 每个用例结束 close，避免注册表复用旧句柄。

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

class DbGlueCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        mod_ = getModule("db");
        ASSERT_FALSE(mod_.name.empty());
    }

    // 打开一个全新的内存库连接（每个用例独立，结束时关闭）。
    // 注意：不能在此缓存 static 函数指针——mod_ 随 fixture 每用例重建，
    // 缓存会指向已销毁实例（悬空）
    ScriptValue open() {
        const auto* openFn = findFunction(mod_, "open");
        return (*openFn)({ScriptValue::fromString(":memory:")});
    }

    void closeConn(const ScriptValue& conn) {
        const auto* closeFn = findFunction(mod_, "close");
        (*closeFn)({conn});
    }

    ModuleDescriptor mod_;
};

// ========== open/close 生命周期与句柄失效防护 ==========

TEST_F(DbGlueCoverageTest, OpenEmptyArgsReturnsNull) {
    const auto* openFn = findFunction(mod_, "open");
    EXPECT_TRUE((*openFn)({}).isNull());
}

TEST_F(DbGlueCoverageTest, OpenInvalidNameReturnsNull) {
    const auto* openFn = findFunction(mod_, "open");
    // 含路径分隔符的名字被安全策略拒绝 → 连接无效 → null
    EXPECT_TRUE((*openFn)({ScriptValue::fromString("bad/name")}).isNull());
}

TEST_F(DbGlueCoverageTest, OpenReusesSameConnectionAndCloseLifecycle) {
    auto c1 = open();
    ASSERT_TRUE(c1.isObject());
    ASSERT_NE(c1.get("__conn_ptr__"), nullptr);
    ASSERT_NE(c1.get("__conn_name__"), nullptr);

    auto c2 = open(); // 同名复用注册表中的连接
    ASSERT_TRUE(c2.isObject());
    EXPECT_EQ(c1.get("__conn_ptr__")->asInt(), c2.get("__conn_ptr__")->asInt());

    const auto* closeFn = findFunction(mod_, "close");
    EXPECT_TRUE((*closeFn)({c1}).asBool());
    EXPECT_FALSE((*closeFn)({c1}).asBool()); // 重复 close：句柄已失效
}

TEST_F(DbGlueCoverageTest, ClosedHandleIsRejectedByGuards) {
    auto conn = open();
    const auto* executeFn = findFunction(mod_, "execute");
    closeConn(conn);
    // close 后原句柄再操作 → extractConnection 防护返回 false（use-after-free 防护）
    EXPECT_FALSE((*executeFn)({conn, ScriptValue::fromString("SELECT 1")}).asBool());
}

// ========== execute/query/scalar 参数化往返 ==========

TEST_F(DbGlueCoverageTest, ExecuteQueryScalarWithTypedParams) {
    auto conn = open();
    const auto* executeFn = findFunction(mod_, "execute");
    const auto* queryFn = findFunction(mod_, "query");
    const auto* scalarFn = findFunction(mod_, "scalar");
    const auto* lastIdFn = findFunction(mod_, "last_insert_id");
    const auto* changesFn = findFunction(mod_, "changes");
    ASSERT_NE(executeFn, nullptr);
    ASSERT_NE(queryFn, nullptr);
    ASSERT_NE(scalarFn, nullptr);
    ASSERT_NE(lastIdFn, nullptr);
    ASSERT_NE(changesFn, nullptr);

    ASSERT_TRUE((*executeFn)({conn, ScriptValue::fromString(
        "CREATE TABLE t (id INTEGER PRIMARY KEY, s TEXT, i INTEGER, f REAL, b INTEGER)")}).asBool());

    // 四种标量类型经 scriptValueToString 转换
    auto params = ScriptValue::fromArray({
        ScriptValue::fromString("hello"), ScriptValue::fromInt(42),
        ScriptValue::fromFloat(1.5), ScriptValue::fromBool(true),
    });
    ASSERT_TRUE((*executeFn)({conn, ScriptValue::fromString(
        "INSERT INTO t (s, i, f, b) VALUES (?, ?, ?, ?)"), params}).asBool());

    auto rows = (*queryFn)({conn, ScriptValue::fromString("SELECT * FROM t")});
    ASSERT_TRUE(rows.isArray());
    ASSERT_EQ(rows.size(), 1u);
    auto row = rows.at(0);
    ASSERT_TRUE(row.isObject());
    EXPECT_EQ(row.get("s")->asString(), "hello");
    EXPECT_EQ(row.get("i")->asString(), "42"); // 查询结果统一字符串化

    // maxRows 限制参数
    (*executeFn)({conn, ScriptValue::fromString("INSERT INTO t (s) VALUES ('x')")});
    auto limited = (*queryFn)({conn, ScriptValue::fromString("SELECT * FROM t"),
                               ScriptValue::fromArray({}), ScriptValue::fromInt(1)});
    EXPECT_EQ(limited.size(), 1u);

    EXPECT_EQ((*scalarFn)({conn, ScriptValue::fromString("SELECT COUNT(*) FROM t")}).asString(), "2");
    EXPECT_EQ((*lastIdFn)({conn}).asInt(), 2);
    EXPECT_GE((*changesFn)({conn}).asInt(), 0);

    closeConn(conn);
}

// ========== transaction：回调注入事务连接 + 回滚分支 ==========

TEST_F(DbGlueCoverageTest, TransactionCommitWithInjectedConnection) {
    auto conn = open();
    const auto* executeFn = findFunction(mod_, "execute");
    const auto* txFn = findFunction(mod_, "transaction");
    const auto* queryFn = findFunction(mod_, "query");
    ASSERT_NE(txFn, nullptr);

    (*executeFn)({conn, ScriptValue::fromString("CREATE TABLE tx (v TEXT)")});

    // 回调收到事务连接对象，可直接 execute
    auto cb = ScriptValue::fromCallable([&executeFn](const std::vector<ScriptValue>& args) {
        (*executeFn)({args[0], ScriptValue::fromString("INSERT INTO tx (v) VALUES ('in-tx')")});
        return ScriptValue::fromBool(true);
    });
    EXPECT_TRUE((*txFn)({conn, cb}).asBool());

    auto rows = (*queryFn)({conn, ScriptValue::fromString("SELECT v FROM tx")});
    ASSERT_EQ(rows.size(), 1u);

    closeConn(conn);
}

TEST_F(DbGlueCoverageTest, TransactionCallbackExceptionRollsBack) {
    auto conn = open();
    const auto* executeFn = findFunction(mod_, "execute");
    const auto* txFn = findFunction(mod_, "transaction");
    const auto* queryFn = findFunction(mod_, "query");
    ASSERT_NE(txFn, nullptr);

    (*executeFn)({conn, ScriptValue::fromString("CREATE TABLE tx2 (v TEXT)")});

    auto boom = ScriptValue::fromCallable([](const std::vector<ScriptValue>&) -> ScriptValue {
        throw std::runtime_error("cov-rollback");
    });
    EXPECT_FALSE((*txFn)({conn, boom}).asBool()); // 异常 → rollback → false

    auto rows = (*queryFn)({conn, ScriptValue::fromString("SELECT COUNT(*) AS n FROM tx2")});
    EXPECT_EQ(rows.at(0).get("n")->asString(), "0"); // 事务内写入未持久化

    closeConn(conn);
}

// ========== table / query 链式 ORM 全生命周期 ==========

TEST_F(DbGlueCoverageTest, TableOrmLifecycle) {
    auto conn = open();
    const auto* tableFn = findFunction(mod_, "table");
    const auto* createFn = findFunction(mod_, "table_create");
    const auto* insertFn = findFunction(mod_, "table_insert");
    const auto* getFn = findFunction(mod_, "table_get");
    const auto* whereFn = findFunction(mod_, "table_where");
    const auto* allFn = findFunction(mod_, "table_all");
    const auto* countFn = findFunction(mod_, "table_count");
    ASSERT_NE(tableFn, nullptr);
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(insertFn, nullptr);
    ASSERT_NE(getFn, nullptr);
    ASSERT_NE(whereFn, nullptr);
    ASSERT_NE(allFn, nullptr);
    ASSERT_NE(countFn, nullptr);

    auto tbl = (*tableFn)({conn, ScriptValue::fromString("cov_users")});
    ASSERT_TRUE(tbl.isObject());
    ASSERT_NE(tbl.get("__table_ptr__"), nullptr);

    auto schema = makeObj({
        {"id", ScriptValue::fromString("INTEGER PRIMARY KEY")},
        {"name", ScriptValue::fromString("TEXT NOT NULL")},
        {"age", ScriptValue::fromString("INTEGER")},
    });
    EXPECT_TRUE((*createFn)({tbl, schema}).asBool());
    EXPECT_EQ((*countFn)({tbl}).asInt(), 0);

    EXPECT_TRUE((*insertFn)({tbl, makeObj({
        {"name", ScriptValue::fromString("Alice")}, {"age", ScriptValue::fromInt(25)}})}).asBool());
    EXPECT_TRUE((*insertFn)({tbl, makeObj({
        {"name", ScriptValue::fromString("Bob")}, {"age", ScriptValue::fromInt(30)}})}).asBool());
    EXPECT_EQ((*countFn)({tbl}).asInt(), 2);

    auto row = (*getFn)({tbl, ScriptValue::fromString("1")});
    ASSERT_TRUE(row.isObject());
    EXPECT_EQ(row.get("name")->asString(), "Alice");
    EXPECT_TRUE((*getFn)({tbl, ScriptValue::fromString("999")}).isNull()); // 不存在 → null

    EXPECT_EQ((*allFn)({tbl}).size(), 2u);

    // where → 链式查询
    auto q = (*whereFn)({tbl, ScriptValue::fromString("age"),
                         ScriptValue::fromString(">="), ScriptValue::fromString("26")});
    ASSERT_TRUE(q.isObject());
    ASSERT_NE(q.get("__query_ptr__"), nullptr);

    const auto* qAllFn = findFunction(mod_, "query_all");
    const auto* qFirstFn = findFunction(mod_, "query_first");
    const auto* qCountFn = findFunction(mod_, "query_count");
    const auto* qLimitFn = findFunction(mod_, "query_limit");
    const auto* qOrderFn = findFunction(mod_, "query_order_by");
    const auto* qUpdateFn = findFunction(mod_, "query_update");
    const auto* qDeleteFn = findFunction(mod_, "query_delete");
    ASSERT_NE(qAllFn, nullptr);
    ASSERT_NE(qFirstFn, nullptr);
    ASSERT_NE(qCountFn, nullptr);
    ASSERT_NE(qLimitFn, nullptr);
    ASSERT_NE(qOrderFn, nullptr);
    ASSERT_NE(qUpdateFn, nullptr);
    ASSERT_NE(qDeleteFn, nullptr);

    // limit（正数/负数拒绝/非 int 默认）与 order_by（显式 desc/空串默认 asc）——
    // 返回原对象支持链式
    EXPECT_TRUE((*qLimitFn)({q, ScriptValue::fromInt(10)}).isObject());
    EXPECT_TRUE((*qLimitFn)({q, ScriptValue::fromInt(-1)}).isObject());
    EXPECT_TRUE((*qLimitFn)({q, ScriptValue::fromString("bad")}).isObject());
    EXPECT_TRUE((*qOrderFn)({q, ScriptValue::fromString("age"), ScriptValue::fromString("desc")}).isObject());
    EXPECT_TRUE((*qOrderFn)({q, ScriptValue::fromString("name"), ScriptValue::fromString("")}).isObject());

    auto matched = (*qAllFn)({q});
    ASSERT_EQ(matched.size(), 1u);
    EXPECT_EQ(matched.at(0).get("name")->asString(), "Bob");
    EXPECT_EQ((*qCountFn)({q}).asInt(), 1);
    auto first = (*qFirstFn)({q});
    ASSERT_TRUE(first.isObject());

    // update + delete 走 where 结果集
    EXPECT_TRUE((*qUpdateFn)({q, makeObj({
        {"age", ScriptValue::fromInt(31)}})}).asBool());
    EXPECT_EQ((*getFn)({tbl, ScriptValue::fromString("2")}).get("age")->asString(), "31");
    EXPECT_EQ((*qDeleteFn)({q}).asInt(), 1);
    EXPECT_EQ((*countFn)({tbl}).asInt(), 1);

    closeConn(conn);
}

// ========== 全部错误/防御分支 ==========

TEST_F(DbGlueCoverageTest, ConnectionFunctionErrorBranches) {
    ScriptValue bogus = ScriptValue::fromInt(7); // 非 Object → extractConnection null

    const auto* executeFn = findFunction(mod_, "execute");
    const auto* queryFn = findFunction(mod_, "query");
    const auto* scalarFn = findFunction(mod_, "scalar");
    const auto* txFn = findFunction(mod_, "transaction");
    const auto* lastIdFn = findFunction(mod_, "last_insert_id");
    const auto* changesFn = findFunction(mod_, "changes");
    const auto* closeFn = findFunction(mod_, "close");
    const auto* tableFn = findFunction(mod_, "table");
    ASSERT_NE(executeFn, nullptr);
    ASSERT_NE(queryFn, nullptr);
    ASSERT_NE(scalarFn, nullptr);
    ASSERT_NE(txFn, nullptr);
    ASSERT_NE(lastIdFn, nullptr);
    ASSERT_NE(changesFn, nullptr);
    ASSERT_NE(closeFn, nullptr);
    ASSERT_NE(tableFn, nullptr);

    // 缺参分支
    EXPECT_FALSE((*executeFn)({}).asBool());
    EXPECT_TRUE((*queryFn)({}).isArray());
    EXPECT_EQ((*scalarFn)({}).asString(), "");
    EXPECT_FALSE((*txFn)({}).asBool());
    EXPECT_EQ((*lastIdFn)({}).asInt(), 0);
    EXPECT_EQ((*changesFn)({}).asInt(), 0);
    EXPECT_FALSE((*closeFn)({}).asBool());
    EXPECT_TRUE((*tableFn)({}).isNull());

    // 无效句柄分支
    EXPECT_FALSE((*executeFn)({bogus, ScriptValue::fromString("SELECT 1")}).asBool());
    EXPECT_EQ((*queryFn)({bogus, ScriptValue::fromString("SELECT 1")}).size(), 0u);
    EXPECT_EQ((*scalarFn)({bogus, ScriptValue::fromString("SELECT 1")}).asString(), "");
    EXPECT_EQ((*lastIdFn)({bogus}).asInt(), 0);
    EXPECT_EQ((*changesFn)({bogus}).asInt(), 0);
    EXPECT_FALSE((*closeFn)({bogus}).asBool());
    EXPECT_TRUE((*tableFn)({bogus, ScriptValue::fromString("t")}).isNull());

    // transaction：非 callable 回调
    auto conn = open();
    EXPECT_FALSE((*txFn)({conn, ScriptValue::fromString("not-callable")}).asBool());
    // table：空表名 → null
    EXPECT_TRUE((*tableFn)({conn, ScriptValue::fromString("")}).isNull());
    closeConn(conn);
}

TEST_F(DbGlueCoverageTest, TableFunctionErrorBranches) {
    ScriptValue bogusTable = ScriptValue::fromInt(9);

    const auto* createFn = findFunction(mod_, "table_create");
    const auto* insertFn = findFunction(mod_, "table_insert");
    const auto* getFn = findFunction(mod_, "table_get");
    const auto* whereFn = findFunction(mod_, "table_where");
    const auto* allFn = findFunction(mod_, "table_all");
    const auto* countFn = findFunction(mod_, "table_count");
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(insertFn, nullptr);
    ASSERT_NE(getFn, nullptr);
    ASSERT_NE(whereFn, nullptr);
    ASSERT_NE(allFn, nullptr);
    ASSERT_NE(countFn, nullptr);

    // 缺参分支
    EXPECT_FALSE((*createFn)({}).asBool());
    EXPECT_FALSE((*insertFn)({}).asBool());
    EXPECT_TRUE((*getFn)({}).isNull());
    EXPECT_TRUE((*whereFn)({}).isNull());
    EXPECT_TRUE((*allFn)({}).isArray());
    EXPECT_EQ((*allFn)({}).size(), 0u);
    EXPECT_EQ((*countFn)({}).asInt(), 0);

    // 无效句柄分支
    EXPECT_FALSE((*createFn)({bogusTable, makeObj({})}).asBool());
    EXPECT_FALSE((*insertFn)({bogusTable, makeObj({})}).asBool());
    EXPECT_TRUE((*getFn)({bogusTable, ScriptValue::fromString("1")}).isNull());
    EXPECT_TRUE((*whereFn)({bogusTable, ScriptValue::fromString("f"),
                            ScriptValue::fromString("="), ScriptValue::fromString("v")}).isNull());
    EXPECT_EQ((*allFn)({bogusTable}).size(), 0u);
    EXPECT_EQ((*countFn)({bogusTable}).asInt(), 0);
}

TEST_F(DbGlueCoverageTest, SchemaAndRowTypeValidation) {
    auto conn = open();
    const auto* tableFn = findFunction(mod_, "table");
    const auto* createFn = findFunction(mod_, "table_create");
    const auto* insertFn = findFunction(mod_, "table_insert");
    ASSERT_NE(tableFn, nullptr);
    ASSERT_NE(createFn, nullptr);
    ASSERT_NE(insertFn, nullptr);

    auto tbl = (*tableFn)({conn, ScriptValue::fromString("cov_types")});
    ASSERT_TRUE(tbl.isObject());

    // schema 非 Object → false
    EXPECT_FALSE((*createFn)({tbl, ScriptValue::fromString("id INTEGER")}).asBool());
    // row 非 Object → false
    EXPECT_FALSE((*insertFn)({tbl, ScriptValue::fromString("bad-row")}).asBool());

    closeConn(conn);
}

TEST_F(DbGlueCoverageTest, QueryFunctionErrorBranches) {
    ScriptValue bogusQuery = ScriptValue::fromInt(11);

    const auto* qAllFn = findFunction(mod_, "query_all");
    const auto* qFirstFn = findFunction(mod_, "query_first");
    const auto* qCountFn = findFunction(mod_, "query_count");
    const auto* qUpdateFn = findFunction(mod_, "query_update");
    const auto* qDeleteFn = findFunction(mod_, "query_delete");
    const auto* qLimitFn = findFunction(mod_, "query_limit");
    const auto* qOrderFn = findFunction(mod_, "query_order_by");
    ASSERT_NE(qAllFn, nullptr);
    ASSERT_NE(qFirstFn, nullptr);
    ASSERT_NE(qCountFn, nullptr);
    ASSERT_NE(qUpdateFn, nullptr);
    ASSERT_NE(qDeleteFn, nullptr);
    ASSERT_NE(qLimitFn, nullptr);
    ASSERT_NE(qOrderFn, nullptr);

    // 无效句柄：limit/order_by 返回原对象（链式语义），其余返回空值
    EXPECT_EQ((*qAllFn)({bogusQuery}).size(), 0u);
    EXPECT_TRUE((*qFirstFn)({bogusQuery}).isNull());
    EXPECT_EQ((*qCountFn)({bogusQuery}).asInt(), 0);
    EXPECT_FALSE((*qUpdateFn)({bogusQuery, makeObj({})}).asBool());
    EXPECT_EQ((*qDeleteFn)({bogusQuery}).asInt(), 0);
    EXPECT_TRUE((*qLimitFn)({bogusQuery, ScriptValue::fromInt(1)}).isInt());
    EXPECT_TRUE((*qOrderFn)({bogusQuery, ScriptValue::fromString("f")}).isInt());

    // 缺参：limit/order_by 无第二参返回原对象
    EXPECT_TRUE((*qLimitFn)({bogusQuery}).isInt());
    EXPECT_TRUE((*qOrderFn)({bogusQuery}).isInt());

    // query_update：row 非 Object → false
    const auto* whereFn = findFunction(mod_, "table_where");
    ASSERT_NE(whereFn, nullptr);
    EXPECT_FALSE((*qUpdateFn)({bogusQuery, ScriptValue::fromString("no")}).asBool());
}

} // anonymous namespace
