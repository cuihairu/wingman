#include <gtest/gtest.h>
#include "wingman/json.hpp"

using namespace wingman;

class JsonTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// JsonValue Tests
// ============================================================================

TEST_F(JsonTest, CreateNull) {
    JsonValue json;
    EXPECT_EQ(json.type(), JsonType::Null);
    EXPECT_TRUE(json.isNull());
}

TEST_F(JsonTest, CreateBool) {
    JsonValue json(true);
    EXPECT_EQ(json.type(), JsonType::Boolean);
    EXPECT_TRUE(json.asBool());
    EXPECT_TRUE(json.isBool());

    JsonValue json2(false);
    EXPECT_FALSE(json2.asBool());
}

TEST_F(JsonTest, CreateNumber) {
    JsonValue json(42);
    EXPECT_EQ(json.type(), JsonType::Number);
    EXPECT_EQ(json.asInt(), 42);
    EXPECT_TRUE(json.isNumber());

    JsonValue json2(3.14);
    EXPECT_DOUBLE_EQ(json2.asDouble(), 3.14);
}

TEST_F(JsonTest, CreateString) {
    JsonValue json("hello");
    EXPECT_EQ(json.type(), JsonType::String);
    EXPECT_STREQ(json.asString().c_str(), "hello");
    EXPECT_TRUE(json.isString());
}

TEST_F(JsonTest, CreateArray) {
    JsonValue json = JsonValue::array();
    EXPECT_EQ(json.type(), JsonType::Array);
    EXPECT_TRUE(json.isArray());
    EXPECT_EQ(json.size(), 0);

    json.push(1);
    json.push("test");
    EXPECT_EQ(json.size(), 2);
}

TEST_F(JsonTest, CreateObject) {
    JsonValue json = JsonValue::object();
    EXPECT_EQ(json.type(), JsonType::Object);
    EXPECT_TRUE(json.isObject());
    EXPECT_EQ(json.size(), 0);

    json.set("key", "value");
    EXPECT_EQ(json.size(), 1);
}

TEST_F(JsonTest, ParseString) {
    JsonValue json = JsonValue::parse("\"hello\"");
    EXPECT_EQ(json.type(), JsonType::String);
    EXPECT_STREQ(json.asString().c_str(), "hello");
}

TEST_F(JsonTest, ParseNumber) {
    JsonValue json = JsonValue::parse("42");
    EXPECT_EQ(json.type(), JsonType::Number);
    EXPECT_EQ(json.asInt(), 42);
}

TEST_F(JsonTest, ParseBool) {
    JsonValue json = JsonValue::parse("true");
    EXPECT_EQ(json.type(), JsonType::Boolean);
    EXPECT_TRUE(json.asBool());

    JsonValue json2 = JsonValue::parse("false");
    EXPECT_FALSE(json2.asBool());
}

TEST_F(JsonTest, ParseArray) {
    JsonValue json = JsonValue::parse("[1, 2, 3]");
    EXPECT_EQ(json.type(), JsonType::Array);
    EXPECT_EQ(json.size(), 3);
    EXPECT_EQ(json.at(0).asInt(), 1);
    EXPECT_EQ(json.at(1).asInt(), 2);
    EXPECT_EQ(json.at(2).asInt(), 3);
}

TEST_F(JsonTest, ParseObject) {
    JsonValue json = JsonValue::parse("{\"key\": \"value\", \"num\": 42}");
    EXPECT_EQ(json.type(), JsonType::Object);
    EXPECT_EQ(json.size(), 2);
    EXPECT_STREQ(json.get("key").asString().c_str(), "value");
    EXPECT_EQ(json.get("num").asInt(), 42);
}

TEST_F(JsonTest, DumpString) {
    JsonValue json("hello");
    std::string dumped = json.dump();
    EXPECT_EQ(dumped, "\"hello\"");
}

TEST_F(JsonTest, DumpNumber) {
    JsonValue json(42);
    std::string dumped = json.dump();
    EXPECT_EQ(dumped, "42");
}

TEST_F(JsonTest, DumpBool) {
    JsonValue json(true);
    std::string dumped = json.dump();
    EXPECT_EQ(dumped, "true");
}

TEST_F(JsonTest, DumpArray) {
    JsonValue json = JsonValue::array();
    json.push(1);
    json.push(2);
    json.push(3);
    std::string dumped = json.dump();
    EXPECT_EQ(dumped, "[1,2,3]");
}

TEST_F(JsonTest, DumpObject) {
    JsonValue json = JsonValue::object();
    json.set("key", "value");
    json.set("num", 42);
    std::string dumped = json.dump();
    EXPECT_EQ(dumped, "{\"key\":\"value\",\"num\":42}");
}

TEST_F(JsonTest, ArrayAccess) {
    JsonValue json = JsonValue::array();
    json.push("first");
    json.push("second");
    json.push("third");

    EXPECT_EQ(json.at(0).asString(), "first");
    EXPECT_EQ(json.at(1).asString(), "second");
    EXPECT_EQ(json.at(2).asString(), "third");
}

TEST_F(JsonTest, ObjectAccess) {
    JsonValue json = JsonValue::object();
    json.set("name", "wingman");
    json.set("version", "1.0");

    EXPECT_TRUE(json.has("name"));
    EXPECT_TRUE(json.has("version"));
    EXPECT_FALSE(json.has("missing"));

    EXPECT_EQ(json.get("name").asString(), "wingman");
    EXPECT_EQ(json.get("version").asString(), "1.0");
}

TEST_F(JsonTest, NestedStructure) {
    JsonValue json = JsonValue::object();
    JsonValue nested = JsonValue::object();
    nested.set("inner", "value");
    json.set("outer", nested);

    EXPECT_EQ(json.get("outer").get("inner").asString(), "value");
}

TEST_F(JsonTest, MixedArray) {
    JsonValue json = JsonValue::array();
    json.push(1);
    json.push("string");
    json.push(true);
    json.push(JsonValue(nullptr));  // Create null value

    EXPECT_EQ(json.size(), 4);
    EXPECT_EQ(json.at(0).asInt(), 1);
    EXPECT_EQ(json.at(1).asString(), "string");
    EXPECT_TRUE(json.at(2).asBool());
    EXPECT_TRUE(json.at(3).isNull());
}

TEST_F(JsonTest, ObjectKeys) {
    JsonValue json = JsonValue::object();
    json.set("a", 1);
    json.set("b", 2);
    json.set("c", 3);

    auto keys = json.keys();
    EXPECT_EQ(keys.size(), 3);
    // Note: JSON objects are unordered
}

TEST_F(JsonTest, CopyValue) {
    JsonValue original = JsonValue::object();
    original.set("key", "value");

    JsonValue copy = original;
    EXPECT_EQ(copy.get("key").asString(), "value");

    // Modify original
    original.set("key", "modified");
    EXPECT_EQ(original.get("key").asString(), "modified");
    // Copy should be independent (deep copy)
    EXPECT_EQ(copy.get("key").asString(), "value");
}

TEST_F(JsonTest, ArraySize) {
    JsonValue json = JsonValue::array();
    json.push(1);
    json.push(2);
    json.push(3);

    EXPECT_EQ(json.size(), 3);
}

TEST_F(JsonTest, ObjectSize) {
    JsonValue json = JsonValue::object();
    json.set("a", 1);
    json.set("b", 2);
    json.set("c", 3);

    EXPECT_EQ(json.size(), 3);
    EXPECT_TRUE(json.has("a"));
    EXPECT_TRUE(json.has("b"));
    EXPECT_TRUE(json.has("c"));
}
