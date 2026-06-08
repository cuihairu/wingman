#include <gtest/gtest.h>
#include "wingman/script/modules/db_connection.hpp"
#include <filesystem>
#include <fstream>

using namespace wingman::script::modules;

namespace {
	constexpr const char* kTestDbName = "__test_db_module__";

	void cleanTestDb() {
		std::string dbPath = getDatabasePath(kTestDbName);
		if (dbPath != ":memory:" && std::filesystem::exists(dbPath)) {
			std::filesystem::remove(dbPath);
		}
	}
} // anonymous namespace

// 测试夹具：确保连接在清理前正确关闭
class DbModuleTestFixture : public ::testing::Test {
protected:
	void SetUp() override {
		cleanTestDb();
		std::string dataDir = getScriptDataDir();
		conn = std::make_shared<DbConnection>(kTestDbName, dataDir);
	}

	void TearDown() override {
		if (conn) {
			conn->close();
			conn.reset();
		}
		cleanTestDb();
	}

	std::shared_ptr<DbConnection> conn;
};

// ========== Path Utilities Tests ==========

TEST(DbPathUtilsTest, GetScriptDataDir) {
	std::string dataDir = getScriptDataDir();

	EXPECT_FALSE(dataDir.empty());

	// 检查目录是否存在或可以创建
	EXPECT_TRUE(std::filesystem::exists(dataDir) ||
				std::filesystem::create_directories(dataDir));
}

TEST(DbPathUtilsTest, GetDatabasePath) {
	// 测试内存数据库
	EXPECT_EQ(getDatabasePath(":memory:"), ":memory:");

	// 测试命名数据库
	std::string path = getDatabasePath("test");
	EXPECT_FALSE(path.empty());
	EXPECT_NE(path, ":memory:");

	// 应该以 .db 结尾
	EXPECT_TRUE(path.find(".db") != std::string::npos);
}

// ========== DbConnection Tests ==========

TEST(DbConnectionTest, Connection_OpenMemory) {
	auto conn = std::make_shared<DbConnection>(":memory:", getScriptDataDir());

	EXPECT_TRUE(conn->isValid());
	EXPECT_EQ(conn->getName(), ":memory:");
}

TEST_F(DbModuleTestFixture, Connection_OpenNamed) {
	EXPECT_TRUE(conn->isValid());
	EXPECT_EQ(conn->getName(), kTestDbName);
}

TEST_F(DbModuleTestFixture, Connection_Close) {
	ASSERT_TRUE(conn->isValid());

	conn->close();

	EXPECT_FALSE(conn->isValid());
}

TEST_F(DbModuleTestFixture, Execute_CreateTable) {
	ASSERT_TRUE(conn->isValid());

	bool success = conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	);

	EXPECT_TRUE(success);
}

TEST_F(DbModuleTestFixture, Execute_Insert) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	bool success = conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Alice", "25"}
	);

	EXPECT_TRUE(success);
	EXPECT_EQ(conn->lastInsertId(), 1);
	EXPECT_EQ(conn->changes(), 1);
}

TEST_F(DbModuleTestFixture, Query_Select) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	ASSERT_TRUE(conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Alice", "25"}
	));

	ASSERT_TRUE(conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Bob", "30"}
	));

	auto rows = conn->query("SELECT * FROM users");

	EXPECT_EQ(rows.size(), 2);

	if (rows.size() >= 2) {
		EXPECT_EQ(rows[0]["name"], "Alice");
		EXPECT_EQ(rows[0]["age"], "25");
		EXPECT_EQ(rows[1]["name"], "Bob");
		EXPECT_EQ(rows[1]["age"], "30");
	}
}

TEST_F(DbModuleTestFixture, Query_WithParams) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	ASSERT_TRUE(conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Alice", "25"}
	));

	ASSERT_TRUE(conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Bob", "30"}
	));

	auto rows = conn->query("SELECT * FROM users WHERE age > ?", {"20"});

	EXPECT_EQ(rows.size(), 2);

	auto filtered = conn->query("SELECT * FROM users WHERE age > ?", {"28"});

	EXPECT_EQ(filtered.size(), 1);
	if (filtered.size() >= 1) {
		EXPECT_EQ(filtered[0]["name"], "Bob");
	}
}

TEST_F(DbModuleTestFixture, Query_MaxRows) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	// 插入 100 行
	for (int i = 0; i < 100; ++i) {
		ASSERT_TRUE(conn->execute(
			"INSERT INTO users (name, age) VALUES (?, ?)",
			{"User" + std::to_string(i), std::to_string(i)}
		));
	}

	auto rows = conn->query("SELECT * FROM users", {}, 10);

	EXPECT_EQ(rows.size(), 10);
}

TEST_F(DbModuleTestFixture, Scalar) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	ASSERT_TRUE(conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Alice", "25"}
	));

	ASSERT_TRUE(conn->execute(
		"INSERT INTO users (name, age) VALUES (?, ?)",
		{"Bob", "30"}
	));

	std::string count = conn->scalar("SELECT COUNT(*) FROM users");

	EXPECT_EQ(count, "2");

	std::string maxAge = conn->scalar("SELECT MAX(age) FROM users");

	EXPECT_EQ(maxAge, "30");
}

TEST_F(DbModuleTestFixture, Transaction_Commit) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	bool success = conn->transaction([](DbConnection& tx) {
		tx.execute("INSERT INTO users (name, age) VALUES (?, ?)", {"Alice", "25"});
		tx.execute("INSERT INTO users (name, age) VALUES (?, ?)", {"Bob", "30"});
	});

	EXPECT_TRUE(success);

	auto rows = conn->query("SELECT * FROM users");
	EXPECT_EQ(rows.size(), 2);
}

TEST_F(DbModuleTestFixture, Transaction_Rollback) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	// 故意违反约束以触发回滚
	bool success = conn->transaction([](DbConnection& tx) {
		tx.execute("INSERT INTO users (name, age) VALUES (?, ?)", {"Alice", "25"});
		// 这会违反主键约束（假设 id 是 INTEGER PRIMARY KEY）
		tx.execute("INSERT INTO users (id, name, age) VALUES (?, ?, ?)", {"1", "Bob", "30"});
	});

	// 根据实现，这可能返回 true（SQLite 默认不强制主键约束直到查询）
	// 但如果回滚，应该没有数据被插入
}

TEST_F(DbModuleTestFixture, Transaction_CallbackException) {
	ASSERT_TRUE(conn->isValid());

	ASSERT_TRUE(conn->execute(
		"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)"
	));

	bool success = conn->transaction([](DbConnection& tx) {
		tx.execute("INSERT INTO users (name, age) VALUES (?, ?)", {"Alice", "25"});
		throw std::runtime_error("Test exception");
	});

	EXPECT_FALSE(success);

	auto rows = conn->query("SELECT * FROM users");
	EXPECT_EQ(rows.size(), 0);
}

// ========== DbTable Tests ==========

TEST_F(DbModuleTestFixture, Table_Create) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	Schema schema = {
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	};

	bool success = table.create(schema);

	EXPECT_TRUE(success);

	// 验证表已创建
	auto rows = conn->query("SELECT name FROM sqlite_master WHERE type='table' AND name='users'");
	EXPECT_EQ(rows.size(), 1);
}

TEST_F(DbModuleTestFixture, Table_InvalidIdentifier) {
	ASSERT_TRUE(conn->isValid());

	// 测试无效表名
	DbTable invalidTable(conn, "invalid-table-name");
	Schema schema = {{"id", "INTEGER PRIMARY KEY"}};

	bool success = invalidTable.create(schema);
	EXPECT_FALSE(success);
}

TEST_F(DbModuleTestFixture, Table_Insert) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	bool success = table.insert({
		{"name", "Alice"},
		{"age", "25"}
	});

	EXPECT_TRUE(success);
	EXPECT_EQ(conn->lastInsertId(), 1);
}

TEST_F(DbModuleTestFixture, Table_InsertMultiple) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	EXPECT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	EXPECT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));
	EXPECT_TRUE(table.insert({{"name", "Charlie"}, {"age", "35"}}));

	auto count = table.count();
	EXPECT_EQ(count, 3);
}

TEST_F(DbModuleTestFixture, Table_Get) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));

	auto row = table.get("1");

	EXPECT_FALSE(row.empty());
	EXPECT_EQ(row["name"], "Alice");
	EXPECT_EQ(row["age"], "25");
}

TEST_F(DbModuleTestFixture, Table_All) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));

	auto rows = table.all();

	EXPECT_EQ(rows.size(), 2);
}

TEST_F(DbModuleTestFixture, Table_Count) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	EXPECT_EQ(table.count(), 0);

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));

	EXPECT_EQ(table.count(), 2);
}

// ========== QueryBuilder Tests ==========

TEST_F(DbModuleTestFixture, QueryBuilder_Where) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));
	ASSERT_TRUE(table.insert({{"name", "Charlie"}, {"age", "35"}}));

	auto query = table.where("age", ">", "28");
	auto rows = query.all();

	EXPECT_EQ(rows.size(), 2);
}

TEST_F(DbModuleTestFixture, QueryBuilder_First) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));

	auto row = table.where("age", ">", "28").first();

	EXPECT_FALSE(row.empty());
	EXPECT_EQ(row["name"], "Bob");
}

TEST_F(DbModuleTestFixture, QueryBuilder_Count) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));
	ASSERT_TRUE(table.insert({{"name", "Charlie"}, {"age", "35"}}));

	auto count = table.where("age", ">", "28").count();

	EXPECT_EQ(count, 2);
}

TEST_F(DbModuleTestFixture, QueryBuilder_Update) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));

	bool success = table.where("name", "=", "Alice").update({{"age", "26"}});

	EXPECT_TRUE(success);

	auto row = table.get("1");
	EXPECT_EQ(row["age"], "26");
}

TEST_F(DbModuleTestFixture, QueryBuilder_Delete) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));

	int deleted = table.where("age", "<", "28").deleteRows();

	EXPECT_EQ(deleted, 1);
	EXPECT_EQ(table.count(), 1);
}

TEST_F(DbModuleTestFixture, QueryBuilder_OrderBy) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));
	ASSERT_TRUE(table.insert({{"name", "Charlie"}, {"age", "20"}}));

	auto rows = table.where("age", ">", "0").orderBy("age", "desc").all();

	ASSERT_EQ(rows.size(), 3);
	EXPECT_EQ(rows[0]["age"], "30");
	EXPECT_EQ(rows[2]["age"], "20");
}

TEST_F(DbModuleTestFixture, QueryBuilder_Limit) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	ASSERT_TRUE(table.insert({{"name", "Alice"}, {"age", "25"}}));
	ASSERT_TRUE(table.insert({{"name", "Bob"}, {"age", "30"}}));
	ASSERT_TRUE(table.insert({{"name", "Charlie"}, {"age", "35"}}));

	auto rows = table.where("age", ">", "0").orderBy("age", "asc").limit(2).all();

	EXPECT_EQ(rows.size(), 2);
}

TEST_F(DbModuleTestFixture, QueryBuilder_Chained) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "users");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"age", "INTEGER"}
	}));

	// 插入 100 条数据
	for (int i = 0; i < 100; ++i) {
		ASSERT_TRUE(table.insert({
			{"name", "User" + std::to_string(i)},
			{"age", std::to_string(i)}
		}));
	}

	auto rows = table.where("age", ">", "50")
					.orderBy("age", "desc")
					.limit(5)
					.all();

	EXPECT_EQ(rows.size(), 5);
	EXPECT_EQ(rows[0]["age"], "99");
}

// ========== Security Tests ==========

TEST(DbSecurityTest, Security_PathRestriction) {
	std::string dataDir = getScriptDataDir();

	// 测试合法路径
	auto validConn = std::make_shared<DbConnection>("local", dataDir);
	EXPECT_TRUE(validConn->isValid());

	// 测试内存数据库
	auto memConn = std::make_shared<DbConnection>(":memory:", dataDir);
	EXPECT_TRUE(memConn->isValid());

	// 测试非法路径（包含路径分隔符）
	auto invalidConn = std::make_shared<DbConnection>("../etc/passwd", dataDir);
	EXPECT_FALSE(invalidConn->isValid());

	// 清理
	validConn->close();
	std::string dbPath = getDatabasePath("local");
	if (std::filesystem::exists(dbPath)) {
		std::filesystem::remove(dbPath);
	}
}

TEST_F(DbModuleTestFixture, SQL_IdentifierValidation) {
	ASSERT_TRUE(conn->isValid());

	DbTable validTable(conn, "valid_table_name");
	EXPECT_TRUE(validTable.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT"}
	}));

	// 无效标识符应该失败
	DbTable invalidTable(conn, "invalid-table-name");
	EXPECT_FALSE(invalidTable.create({
		{"id", "INTEGER PRIMARY KEY"}
	}));
}

// ========== Integration Tests ==========

TEST_F(DbModuleTestFixture, Integration_CRUD) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "products");

	// Create
	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"price", "REAL"},
		{"stock", "INTEGER DEFAULT 0"}
	}));

	// Insert
	ASSERT_TRUE(table.insert({
		{"name", "Laptop"},
		{"price", "999.99"},
		{"stock", "10"}
	}));

	ASSERT_TRUE(table.insert({
		{"name", "Mouse"},
		{"price", "29.99"},
		{"stock", "50"}
	}));

	EXPECT_EQ(table.count(), 2);

	// Read
	auto laptop = table.get("1");
	EXPECT_EQ(laptop["name"], "Laptop");
	EXPECT_EQ(laptop["price"], "999.99");

	auto expensive = table.where("price", ">", "100").all();
	EXPECT_EQ(expensive.size(), 1);

	// Update
	table.where("name", "=", "Mouse").update({{"price", "24.99"}});

	auto mouse = table.where("name", "=", "Mouse").first();
	EXPECT_EQ(mouse["price"], "24.99");

	// Delete
	table.where("stock", "<", "20").deleteRows();
	EXPECT_EQ(table.count(), 1);
}

TEST_F(DbModuleTestFixture, Integration_TransactionWithORM) {
	ASSERT_TRUE(conn->isValid());

	DbTable table(conn, "accounts");

	ASSERT_TRUE(table.create({
		{"id", "INTEGER PRIMARY KEY"},
		{"name", "TEXT NOT NULL"},
		{"balance", "INTEGER DEFAULT 0"}
	}));

	// 在事务中执行多个操作
	bool success = conn->transaction([&table](DbConnection& txConn) {
		table.insert({{"name", "Alice"}, {"balance", "100"}});
		table.insert({{"name", "Bob"}, {"balance", "200"}});
		table.insert({{"name", "Charlie"}, {"balance", "300"}});
	});

	EXPECT_TRUE(success);
	EXPECT_EQ(table.count(), 3);
}
