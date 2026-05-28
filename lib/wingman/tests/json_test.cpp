#include <gtest/gtest.h>
#include "wingman/json.hpp"

using namespace wingman;

// ========== 构造函数测试 ==========

TEST(JsonValueTest, DefaultConstructorIsNull) {
    JsonValue v;
    EXPECT_TRUE(v.isNull());
    EXPECT_EQ(v.type(), JsonType::Null);
}

TEST(JsonValueTest, NullptrConstructors) {
    JsonValue v(nullptr);
    EXPECT_TRUE(v.isNull());
}

TEST(JsonValueTest, BoolConstructor) {
    JsonValue v(true);
    EXPECT_TRUE(v.isBool());
    EXPECT_EQ(v.type(), JsonType::Boolean);
    EXPECT_TRUE(v.asBool());
}

TEST(JsonValueTest, IntConstructor) {
    JsonValue v(42);
    EXPECT_TRUE(v.isNumber());
    EXPECT_EQ(v.type(), JsonType::Number);
    EXPECT_EQ(v.asInt(), 42);
}

TEST(JsonValueTest, Int64Constructor) {
    JsonValue v(static_cast<int64_t>(123456789012LL));
    EXPECT_TRUE(v.isNumber());
    EXPECT_EQ(v.asInt64(), 123456789012LL);
}

TEST(JsonValueTest, DoubleConstructor) {
    JsonValue v(3.14);
    EXPECT_TRUE(v.isNumber());
    EXPECT_DOUBLE_EQ(v.asDouble(), 3.14);
}

TEST(JsonValueTest, ConstCharConstructor) {
    JsonValue v("hello");
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.type(), JsonType::String);
    EXPECT_EQ(v.asString(), "hello");
}

TEST(JsonValueTest, StdStringConstructor) {
    std::string s = "world";
    JsonValue v(s);
    EXPECT_TRUE(v.isString());
    EXPECT_EQ(v.asString(), "world");
}

// ========== 拷贝和赋值 ==========

TEST(JsonValueTest, CopyConstructor) {
    JsonValue original(42);
    JsonValue copy(original);
    EXPECT_TRUE(copy.isNumber());
    EXPECT_EQ(copy.asInt(), 42);
}

TEST(JsonValueTest, CopyAssignment) {
    JsonValue a(10);
    JsonValue b(20);
    b = a;
    EXPECT_EQ(b.asInt(), 10);
}

TEST(JsonValueTest, SelfAssignment) {
    JsonValue v(99);
    v = v;
    EXPECT_EQ(v.asInt(), 99);
}

// ========== 类型检查 ==========

TEST(JsonValueTest, IsArray) {
    JsonValue v = JsonValue::parse("[1,2,3]");
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.type(), JsonType::Array);
}

TEST(JsonValueTest, IsObject) {
    JsonValue v = JsonValue::parse("{\"a\":1}");
    EXPECT_TRUE(v.isObject());
    EXPECT_EQ(v.type(), JsonType::Object);
}

TEST(JsonValueTest, FalseIsNotNumber) {
    JsonValue v(false);
    EXPECT_FALSE(v.isNumber());
}

// ========== 数组操作 ==========

TEST(JsonValueTest, ArraySize) {
    JsonValue v = JsonValue::parse("[1,2,3]");
    EXPECT_EQ(v.size(), 3u);
}

TEST(JsonValueTest, ArrayAt) {
    JsonValue v = JsonValue::parse("[10,20,30]");
    EXPECT_EQ(v.at(0).asInt(), 10);
    EXPECT_EQ(v.at(2).asInt(), 30);
}

TEST(JsonValueTest, ArrayAtOutOfRange) {
    JsonValue v = JsonValue::parse("[1]");
    EXPECT_THROW(v.at(5), std::out_of_range);
}

TEST(JsonValueTest, ArrayPush) {
    JsonValue v = JsonValue::array();
    v.push(1);
    v.push(2);
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v.at(0).asInt(), 1);
    EXPECT_EQ(v.at(1).asInt(), 2);
}

TEST(JsonValueTest, PushConvertsToArray) {
    JsonValue v(42);
    EXPECT_FALSE(v.isArray());
    v.push(1);
    EXPECT_TRUE(v.isArray());
}

TEST(JsonValueTest, NonArraySizeReturnsZero) {
    JsonValue v(42);
    EXPECT_EQ(v.size(), 0u);
}

// ========== 对象操作 ==========

TEST(JsonValueTest, ObjectKeys) {
    JsonValue v = JsonValue::parse("{\"a\":1,\"b\":2}");
    auto ks = v.keys();
    EXPECT_EQ(ks.size(), 2u);
}

TEST(JsonValueTest, ObjectGet) {
    JsonValue v = JsonValue::parse("{\"name\":\"test\"}");
    EXPECT_EQ(v.get("name").asString(), "test");
}

TEST(JsonValueTest, ObjectGetMissing) {
    JsonValue v = JsonValue::parse("{\"a\":1}");
    EXPECT_TRUE(v.get("missing").isNull());
}

TEST(JsonValueTest, ObjectSet) {
    JsonValue v = JsonValue::object();
    v.set("key", JsonValue("value"));
    EXPECT_TRUE(v.has("key"));
    EXPECT_EQ(v.get("key").asString(), "value");
}

TEST(JsonValueTest, ObjectHas) {
    JsonValue v = JsonValue::parse("{\"exists\":true}");
    EXPECT_TRUE(v.has("exists"));
    EXPECT_FALSE(v.has("nope"));
}

TEST(JsonValueTest, SetConvertsToObject) {
    JsonValue v(42);
    EXPECT_FALSE(v.isObject());
    v.set("k", JsonValue(1));
    EXPECT_TRUE(v.isObject());
}

TEST(JsonValueTest, NonObjectKeysReturnsEmpty) {
    JsonValue v(42);
    EXPECT_TRUE(v.keys().empty());
}

// ========== 序列化 ==========

TEST(JsonValueTest, DumpCompact) {
    JsonValue v = JsonValue::parse("{\"a\":1}");
    std::string dumped = v.dump();
    EXPECT_EQ(dumped, "{\"a\":1}");
}

TEST(JsonValueTest, DumpIndented) {
    JsonValue v = JsonValue::parse("{\"a\":1}");
    std::string dumped = v.dump(2);
    EXPECT_NE(dumped.find('\n'), std::string::npos);
}

TEST(JsonValueTest, ParseValid) {
    JsonValue v = JsonValue::parse("[1, \"hello\", true]");
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v.at(0).asInt(), 1);
    EXPECT_EQ(v.at(1).asString(), "hello");
    EXPECT_TRUE(v.at(2).asBool());
}

TEST(JsonValueTest, ParseInvalidReturnsNull) {
    JsonValue v = JsonValue::parse("{invalid json}");
    EXPECT_TRUE(v.isNull());
}

// ========== 静态工厂 ==========

TEST(JsonValueTest, StaticObject) {
    JsonValue v = JsonValue::object();
    EXPECT_TRUE(v.isObject());
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonValueTest, StaticArray) {
    JsonValue v = JsonValue::array();
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.size(), 0u);
}

// ========== 值访问类型转换 ==========

TEST(JsonValueTest, DoubleAsInt) {
    JsonValue v(3.7);
    EXPECT_EQ(v.asInt(), 3);
}

TEST(JsonValueTest, BoolAsStringThrows) {
    JsonValue v(true);
    EXPECT_THROW(v.asString(), std::exception);
}

TEST(JsonValueTest, IntAsDouble) {
    JsonValue v(5);
    EXPECT_DOUBLE_EQ(v.asDouble(), 5.0);
}

// ========== 嵌套结构解析 ==========

TEST(JsonValueTest, ParseNestedObject) {
    JsonValue v = JsonValue::parse("{\"a\":{\"b\":1}}");
    EXPECT_TRUE(v.isObject());
    EXPECT_TRUE(v.has("a"));
    JsonValue inner = v.get("a");
    EXPECT_TRUE(inner.isObject());
    EXPECT_EQ(inner.get("b").asInt(), 1);
}

TEST(JsonValueTest, ParseArrayOfObjects) {
    JsonValue v = JsonValue::parse("[{\"x\":1},{\"x\":2}]");
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v.at(0).get("x").asInt(), 1);
    EXPECT_EQ(v.at(1).get("x").asInt(), 2);
}

TEST(JsonValueTest, ParseEmptyObject) {
    JsonValue v = JsonValue::parse("{}");
    EXPECT_TRUE(v.isObject());
    EXPECT_EQ(v.size(), 0u);
    EXPECT_TRUE(v.keys().empty());
}

TEST(JsonValueTest, ParseEmptyArray) {
    JsonValue v = JsonValue::parse("[]");
    EXPECT_TRUE(v.isArray());
    EXPECT_EQ(v.size(), 0u);
}

TEST(JsonValueTest, LargeNestedRoundtrip) {
    JsonValue v = JsonValue::parse("{\"level1\":{\"level2\":{\"level3\":[1,2,3],\"flag\":true}}}");
    ASSERT_TRUE(v.isObject());
    std::string dumped = v.dump();
    JsonValue reparsed = JsonValue::parse(dumped);
    EXPECT_EQ(reparsed.get("level1").get("level2").get("level3").at(1).asInt(), 2);
    EXPECT_TRUE(reparsed.get("level1").get("level2").get("flag").asBool());
}

// ========== 序列化扩展 ==========

TEST(JsonValueTest, DumpNullValue) {
    JsonValue v(nullptr);
    EXPECT_EQ(v.dump(), "null");
}

TEST(JsonValueTest, DumpBooleanValues) {
    JsonValue t(true);
    EXPECT_EQ(t.dump(), "true");
    JsonValue f(false);
    EXPECT_EQ(f.dump(), "false");
}

TEST(JsonValueTest, DumpStringWithSpecialChars) {
    JsonValue v("hello \"world\"");
    std::string dumped = v.dump();
    EXPECT_NE(dumped.find("\\\""), std::string::npos);
}

// ========== 对象操作扩展 ==========

TEST(JsonValueTest, ObjectOverwriteExistingKey) {
    JsonValue v = JsonValue::object();
    v.set("key", JsonValue("old"));
    EXPECT_EQ(v.get("key").asString(), "old");
    v.set("key", JsonValue("new"));
    EXPECT_EQ(v.get("key").asString(), "new");
    // Should still be exactly one key
    EXPECT_EQ(v.keys().size(), 1u);
}

// ========== 数组操作扩展 ==========

TEST(JsonValueTest, ArrayPushMultipleTypes) {
    JsonValue v = JsonValue::array();
    v.push(JsonValue(42));
    v.push(JsonValue("hello"));
    v.push(JsonValue(true));
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v.at(0).asInt(), 42);
    EXPECT_EQ(v.at(1).asString(), "hello");
    EXPECT_TRUE(v.at(2).asBool());
}

// ========== 类型转换扩展 ==========

TEST(JsonValueTest, AsBoolOnNonZeroInt) {
    JsonValue v(7);
    EXPECT_TRUE(v.asBool());
}

TEST(JsonValueTest, AsBoolOnZeroInt) {
    JsonValue v(0);
    EXPECT_FALSE(v.asBool());
}

TEST(JsonValueTest, AsBoolOnString) {
    // nlohmann::json: non-empty string converts to true
    JsonValue v("hello");
    EXPECT_TRUE(v.asBool());
}

TEST(JsonValueTest, AsIntOnBool) {
    // nlohmann::json: true -> 1, false -> 0
    JsonValue t(true);
    EXPECT_EQ(t.asInt(), 1);
    JsonValue f(false);
    EXPECT_EQ(f.asInt(), 0);
}

// ========== has() 和 at() 在非目标类型上的行为 ==========

TEST(JsonValueTest, NonObjectHasReturnsFalse) {
    JsonValue v(42);
    EXPECT_FALSE(v.has("anything"));
}

TEST(JsonValueTest, NonArrayAtThrows) {
    JsonValue v("string");
    EXPECT_THROW(v.at(0), std::exception);
}
