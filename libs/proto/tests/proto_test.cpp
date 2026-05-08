#include <gtest/gtest.h>
#include <wingman/proto/wrapper.hpp>
#include <nlohmann/json.hpp>

namespace proto = wingman::proto;

class ProtoWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== JSON 辅助函数 ==========

TEST_F(ProtoWrapperTest, JsonValueParse) {
    std::string json = R"({"name": "test", "value": 42})";
    std::string result = proto::JsonValue::parse(json);
    EXPECT_EQ(result, json);
}

TEST_F(ProtoWrapperTest, JsonValueSerialize) {
    std::string value = R"({"key": "value"})";
    std::string result = proto::JsonValue::serialize(value);
    EXPECT_EQ(result, value);
}

// ========== JSON 验证 ==========

TEST_F(ProtoWrapperTest, WrapJsonValid) {
    nlohmann::json obj = {{"id", 1}, {"name", "test"}};
    std::string json = obj.dump();

    // 确保是有效的 JSON
    nlohmann::json parsed = nlohmann::json::parse(json);
    EXPECT_TRUE(parsed.contains("id"));
    EXPECT_EQ(parsed["id"], 1);
}

TEST_F(ProtoWrapperTest, WrapJsonArray) {
    nlohmann::json arr = nlohmann::json::array({1, 2, 3});
    std::string json = arr.dump();

    nlohmann::json parsed = nlohmann::json::parse(json);
    EXPECT_TRUE(parsed.is_array());
    EXPECT_EQ(parsed.size(), 3);
}

TEST_F(ProtoWrapperTest, WrapJsonNested) {
    nlohmann::json obj = {
        {"user", {
            {"name", "Alice"},
            {"age", 30}
        }}
    };
    std::string json = obj.dump();

    nlohmann::json parsed = nlohmann::json::parse(json);
    EXPECT_TRUE(parsed.contains("user"));
    EXPECT_TRUE(parsed["user"].contains("name"));
    EXPECT_EQ(parsed["user"]["name"], "Alice");
}

// ========== 特殊字符处理 ==========

TEST_F(ProtoWrapperTest, JsonUnicode) {
    std::string json = R"({"message": "你好，世界！"})";
    nlohmann::json parsed = nlohmann::json::parse(json);
    EXPECT_EQ(parsed["message"], "你好，世界！");
}

TEST_F(ProtoWrapperTest, JsonEscaped) {
    std::string json = R"({"path": "C:\\Users\\test"})";
    nlohmann::json parsed = nlohmann::json::parse(json);
    EXPECT_EQ(parsed["path"], "C:\\Users\\test");
}
