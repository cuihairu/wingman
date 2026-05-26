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

TEST(JsonValueTest, BoolAsString) {
    JsonValue v(true);
    EXPECT_EQ(v.asString(), "true");
}

TEST(JsonValueTest, IntAsDouble) {
    JsonValue v(5);
    EXPECT_DOUBLE_EQ(v.asDouble(), 5.0);
}
