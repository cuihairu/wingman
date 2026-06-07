#include <gtest/gtest.h>
#include "wingman/script/iscript_engine.hpp"
#include "wingman/script/module_registry.hpp"

using namespace wingman::script;
using namespace wingman::script::modules;

// Forward declaration - defined in ini_module.cpp (in wingman::script::modules namespace)
namespace wingman::script::modules {
	ModuleDescriptor createIniModule();
}

namespace {
	// 测试辅助函数：调用模块函数
	ScriptValue callModule(const ModuleDescriptor& mod, const std::string& funcName, const std::vector<ScriptValue>& args) {
		for (const auto& func : mod.functions) {
			if (func.name == funcName) {
				return func.func(args);
			}
		}
		return ScriptValue::null();
	}

	// 创建测试 INI 内容
	std::string createTestIni() {
		return R"(
; Server configuration
[Server]
host = localhost
port = 8080
debug = true

; Database configuration
[Database]
host = db.example.com
port = 5432
username = admin
password = secret

; Empty section
[Empty]

; Global keys (no section)
global_key = global_value
)";
	}
} // anonymous namespace

// ========== INI 解析测试 ==========

TEST(IniModuleTest, Decode_Simple) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
key2 = value2

[Section2]
key3 = value3
)";

	auto result = callModule(mod, "decode", {ScriptValue::fromString(content)});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);
	EXPECT_EQ(result.objectVal.size(), 2);  // 两个 section

	ASSERT_NE(result.get("Section1"), nullptr);
	EXPECT_EQ(result.get("Section1")->objectVal.size(), 2);

	ASSERT_NE(result.get("Section2"), nullptr);
	EXPECT_EQ(result.get("Section2")->objectVal.size(), 1);
}

TEST(IniModuleTest, Decode_WithComments) {
	auto mod = createIniModule();

	std::string content = R"(
; This is a comment
# This is also a comment
[Section]
key = value
; Another comment
key2 = value2
)";

	auto result = callModule(mod, "decode", {ScriptValue::fromString(content)});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);
	ASSERT_NE(result.get("Section"), nullptr);
	EXPECT_EQ(result.get("Section")->objectVal.size(), 2);
}

TEST(IniModuleTest, Decode_WithEmptyLines) {
	auto mod = createIniModule();

	std::string content = R"(

[Section1]


key1 = value1


key2 = value2

)";

	auto result = callModule(mod, "decode", {ScriptValue::fromString(content)});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);
	ASSERT_NE(result.get("Section1"), nullptr);
	EXPECT_EQ(result.get("Section1")->objectVal.size(), 2);
}

TEST(IniModuleTest, Decode_GlobalKeys) {
	auto mod = createIniModule();

	std::string content = R"(
global_key1 = global_value1

[Section]
key = value

global_key2 = global_value2
)";

	auto result = callModule(mod, "decode", {ScriptValue::fromString(content)});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);

	// 全局键应该存储在空字符串 section 中
	ASSERT_NE(result.get(""), nullptr);
	EXPECT_EQ(result.get("")->objectVal.size(), 2);
}

TEST(IniModuleTest, Decode_WithSpaces) {
	auto mod = createIniModule();

	std::string content = R"(
[Section]
  key1  =  value1
key2=value2
  key3=value3
)";

	auto result = callModule(mod, "decode", {ScriptValue::fromString(content)});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);
	ASSERT_NE(result.get("Section"), nullptr);

	auto section = result.get("Section")->objectVal;
	EXPECT_EQ(section["key1"].asString(), "value1");
	EXPECT_EQ(section["key2"].asString(), "value2");
	EXPECT_EQ(section["key3"].asString(), "value3");
}

TEST(IniModuleTest, Decode_EscapeSequences) {
	auto mod = createIniModule();

	std::string content = R"(
[Section]
key1 = value\nwith\nnewlines
key2 = value\twith\ttabs
key3 = value\\with\\backslash
)";

	auto result = callModule(mod, "decode", {ScriptValue::fromString(content)});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);
	ASSERT_NE(result.get("Section"), nullptr);

	auto section = result.get("Section")->objectVal;
	EXPECT_EQ(section["key1"].asString(), "value\nwith\nnewlines");
	EXPECT_EQ(section["key2"].asString(), "value\twith\ttabs");
	EXPECT_EQ(section["key3"].asString(), "value\\with\\backslash");
}

// ========== INI 编码测试 ==========

TEST(IniModuleTest, Encode_Simple) {
	auto mod = createIniModule();

	std::unordered_map<std::string, ScriptValue> data;
	std::unordered_map<std::string, ScriptValue> section1;
	section1["key1"] = ScriptValue::fromString("value1");
	section1["key2"] = ScriptValue::fromString("value2");
	data["Section1"] = ScriptValue::fromObject(std::move(section1));

	auto result = callModule(mod, "encode", {ScriptValue::fromObject(std::move(data))});

	ASSERT_EQ(result.type, ScriptValue::Type::String);
	std::string encoded = result.asString();

	EXPECT_NE(encoded.find("[Section1]"), std::string::npos);
	EXPECT_NE(encoded.find("key1=value1"), std::string::npos);
	EXPECT_NE(encoded.find("key2=value2"), std::string::npos);
}

TEST(IniModuleTest, Encode_EscapeSequences) {
	auto mod = createIniModule();

	std::unordered_map<std::string, ScriptValue> data;
	std::unordered_map<std::string, ScriptValue> section1;
	section1["key1"] = ScriptValue::fromString("value\nwith\nnewlines");
	section1["key2"] = ScriptValue::fromString("value\twith\ttabs");
	section1["key3"] = ScriptValue::fromString("value\\with\\backslash");
	data["Section1"] = ScriptValue::fromObject(std::move(section1));

	auto result = callModule(mod, "encode", {ScriptValue::fromObject(std::move(data))});

	ASSERT_EQ(result.type, ScriptValue::Type::String);
	std::string encoded = result.asString();

	EXPECT_NE(encoded.find("value\\nwith\\nnewlines"), std::string::npos);
	EXPECT_NE(encoded.find("value\\twith\\ttabs"), std::string::npos);
	EXPECT_NE(encoded.find("value\\\\with\\\\backslash"), std::string::npos);
}

TEST(IniModuleTest, Encode_MultipleSections) {
	auto mod = createIniModule();

	std::unordered_map<std::string, ScriptValue> data;

	std::unordered_map<std::string, ScriptValue> section1;
	section1["key1"] = ScriptValue::fromString("value1");
	data["Section1"] = ScriptValue::fromObject(std::move(section1));

	std::unordered_map<std::string, ScriptValue> section2;
	section2["key2"] = ScriptValue::fromString("value2");
	data["Section2"] = ScriptValue::fromObject(std::move(section2));

	auto result = callModule(mod, "encode", {ScriptValue::fromObject(std::move(data))});

	ASSERT_EQ(result.type, ScriptValue::Type::String);
	std::string encoded = result.asString();

	EXPECT_NE(encoded.find("[Section1]"), std::string::npos);
	EXPECT_NE(encoded.find("[Section2]"), std::string::npos);
}

// ========== INI 读写操作测试 ==========

TEST(IniModuleTest, Get_Section) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
key2 = value2
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 获取整个 section
	auto section = callModule(mod, "get", {data, ScriptValue::fromString("Section1")});

	ASSERT_EQ(section.type, ScriptValue::Type::Object);
	EXPECT_EQ(section.objectVal.size(), 2);
	EXPECT_EQ(section.objectVal["key1"].asString(), "value1");
	EXPECT_EQ(section.objectVal["key2"].asString(), "value2");
}

TEST(IniModuleTest, Get_Key) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
key2 = value2
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 获取特定 key
	auto value = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key1")
	});

	EXPECT_EQ(value.asString(), "value1");
}

TEST(IniModuleTest, Get_NonExistent) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 获取不存在的 section
	auto section1 = callModule(mod, "get", {data, ScriptValue::fromString("NonExistent")});
	EXPECT_EQ(section1.type, ScriptValue::Type::Null);

	// 获取不存在的 key
	auto key1 = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("NonExistent")
	});
	EXPECT_EQ(key1.type, ScriptValue::Type::Null);
}

TEST(IniModuleTest, Set_NewKey) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 设置新 key
	callModule(mod, "set", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key2"),
		ScriptValue::fromString("value2")
	});

	// 验证
	auto value = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key2")
	});

	EXPECT_EQ(value.asString(), "value2");
}

TEST(IniModuleTest, Set_ExistingKey) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 修改现有 key
	callModule(mod, "set", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key1"),
		ScriptValue::fromString("new_value")
	});

	// 验证
	auto value = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key1")
	});

	EXPECT_EQ(value.asString(), "new_value");
}

TEST(IniModuleTest, Set_NewSection) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 创建新 section
	callModule(mod, "set", {
		data,
		ScriptValue::fromString("Section2"),
		ScriptValue::fromString("key2"),
		ScriptValue::fromString("value2")
	});

	// 验证 section 存在
	auto hasSection = callModule(mod, "has_section", {
		data,
		ScriptValue::fromString("Section2")
	});

	EXPECT_TRUE(hasSection.asBool());
}

// ========== INI 删除操作测试 ==========

TEST(IniModuleTest, Delete_Section) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1

[Section2]
key2 = value2
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 删除 section
	auto result = callModule(mod, "delete", {
		data,
		ScriptValue::fromString("Section1")
	});

	EXPECT_TRUE(result.asBool());

	// 验证 section 已删除
	auto hasSection = callModule(mod, "has_section", {
		data,
		ScriptValue::fromString("Section1")
	});

	EXPECT_FALSE(hasSection.asBool());
}

TEST(IniModuleTest, Delete_Key) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
key2 = value2
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 删除 key
	auto result = callModule(mod, "delete", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key1")
	});

	EXPECT_TRUE(result.asBool());

	// 验证 key 已删除
	auto hasKey = callModule(mod, "has_key", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key1")
	});

	EXPECT_FALSE(hasKey.asBool());

	// 验证 section 中还有其他 key
	auto hasKey2 = callModule(mod, "has_key", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key2")
	});

	EXPECT_TRUE(hasKey2.asBool());
}

TEST(IniModuleTest, Delete_NonExistent) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 删除不存在的 section
	auto result1 = callModule(mod, "delete", {
		data,
		ScriptValue::fromString("NonExistent")
	});

	EXPECT_FALSE(result1.asBool());

	// 删除不存在的 key
	auto result2 = callModule(mod, "delete", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("NonExistent")
	});

	EXPECT_FALSE(result2.asBool());
}

// ========== INI 检查操作测试 ==========

TEST(IniModuleTest, HasSection) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 存在的 section
	auto result1 = callModule(mod, "has_section", {
		data,
		ScriptValue::fromString("Section1")
	});

	EXPECT_TRUE(result1.asBool());

	// 不存在的 section
	auto result2 = callModule(mod, "has_section", {
		data,
		ScriptValue::fromString("NonExistent")
	});

	EXPECT_FALSE(result2.asBool());
}

TEST(IniModuleTest, HasKey) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 存在的 key
	auto result1 = callModule(mod, "has_key", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("key1")
	});

	EXPECT_TRUE(result1.asBool());

	// 不存在的 key
	auto result2 = callModule(mod, "has_key", {
		data,
		ScriptValue::fromString("Section1"),
		ScriptValue::fromString("NonExistent")
	});

	EXPECT_FALSE(result2.asBool());

	// 不存在的 section
	auto result3 = callModule(mod, "has_key", {
		data,
		ScriptValue::fromString("NonExistent"),
		ScriptValue::fromString("key1")
	});

	EXPECT_FALSE(result3.asBool());
}

// ========== INI 查询操作测试 ==========

TEST(IniModuleTest, Sections) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1

[Section2]
key2 = value2

[Section3]
key3 = value3
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	auto result = callModule(mod, "sections", {data});

	ASSERT_EQ(result.type, ScriptValue::Type::Array);
	EXPECT_EQ(result.arrayVal.size(), 3);
}

TEST(IniModuleTest, Keys) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
key2 = value2
key3 = value3
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	auto result = callModule(mod, "keys", {
		data,
		ScriptValue::fromString("Section1")
	});

	ASSERT_EQ(result.type, ScriptValue::Type::Array);
	EXPECT_EQ(result.arrayVal.size(), 3);
}

TEST(IniModuleTest, Keys_NonExistentSection) {
	auto mod = createIniModule();

	std::string content = R"(
[Section1]
key1 = value1
)";

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	auto result = callModule(mod, "keys", {
		data,
		ScriptValue::fromString("NonExistent")
	});

	ASSERT_EQ(result.type, ScriptValue::Type::Array);
	EXPECT_EQ(result.arrayVal.size(), 0);
}

// ========== INI 合并操作测试 ==========

TEST(IniModuleTest, Merge_Simple) {
	auto mod = createIniModule();

	std::unordered_map<std::string, ScriptValue> base;
	std::unordered_map<std::string, ScriptValue> baseSection;
	baseSection["key1"] = ScriptValue::fromString("value1");
	baseSection["key2"] = ScriptValue::fromString("value2");
	base["Section1"] = ScriptValue::fromObject(std::move(baseSection));

	std::unordered_map<std::string, ScriptValue> override;
	std::unordered_map<std::string, ScriptValue> overrideSection;
	overrideSection["key3"] = ScriptValue::fromString("value3");
	override["Section2"] = ScriptValue::fromObject(std::move(overrideSection));

	auto result = callModule(mod, "merge", {
		ScriptValue::fromObject(std::move(base)),
		ScriptValue::fromObject(std::move(override))
	});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);
	EXPECT_EQ(result.objectVal.size(), 2);  // Section1 和 Section2
	EXPECT_NE(result.get("Section1"), nullptr);
	EXPECT_NE(result.get("Section2"), nullptr);
}

TEST(IniModuleTest, Merge_Override) {
	auto mod = createIniModule();

	std::unordered_map<std::string, ScriptValue> base;
	std::unordered_map<std::string, ScriptValue> baseSection;
	baseSection["key1"] = ScriptValue::fromString("value1");
	baseSection["key2"] = ScriptValue::fromString("value2");
	base["Section1"] = ScriptValue::fromObject(std::move(baseSection));

	std::unordered_map<std::string, ScriptValue> override;
	std::unordered_map<std::string, ScriptValue> overrideSection;
	overrideSection["key2"] = ScriptValue::fromString("new_value2");
	overrideSection["key3"] = ScriptValue::fromString("value3");
	override["Section1"] = ScriptValue::fromObject(std::move(overrideSection));

	auto result = callModule(mod, "merge", {
		ScriptValue::fromObject(std::move(base)),
		ScriptValue::fromObject(std::move(override))
	});

	ASSERT_EQ(result.type, ScriptValue::Type::Object);

	auto section = result.get("Section1")->objectVal;
	EXPECT_EQ(section["key1"].asString(), "value1");  // 保留
	EXPECT_EQ(section["key2"].asString(), "new_value2");  // 覆盖
	EXPECT_EQ(section["key3"].asString(), "value3");  // 新增
}

// ========== 集成测试 ==========

TEST(IniModuleTest, RoundTrip) {
	auto mod = createIniModule();

	std::string original = R"(
[Database]
host = localhost
port = 5432
username = admin

[Server]
host = 0.0.0.0
port = 8080
debug = true
)";

	// 解码
	auto data = callModule(mod, "decode", {ScriptValue::fromString(original)});

	// 编码
	auto encoded = callModule(mod, "encode", {data});

	// 再次解码
	auto data2 = callModule(mod, "decode", {encoded});

	// 验证数据一致
	auto dbHost = callModule(mod, "get", {
		data2,
		ScriptValue::fromString("Database"),
		ScriptValue::fromString("host")
	});

	EXPECT_EQ(dbHost.asString(), "localhost");

	auto serverPort = callModule(mod, "get", {
		data2,
		ScriptValue::fromString("Server"),
		ScriptValue::fromString("port")
	});

	EXPECT_EQ(serverPort.asString(), "8080");
}

TEST(IniModuleTest, ComplexConfiguration) {
	auto mod = createIniModule();

	std::string content = createTestIni();

	auto data = callModule(mod, "decode", {ScriptValue::fromString(content)});

	// 验证所有 section 存在
	EXPECT_TRUE(callModule(mod, "has_section", {data, ScriptValue::fromString("Server")}).asBool());
	EXPECT_TRUE(callModule(mod, "has_section", {data, ScriptValue::fromString("Database")}).asBool());
	EXPECT_TRUE(callModule(mod, "has_section", {data, ScriptValue::fromString("Empty")}).asBool());

	// 验证 Server 配置
	auto serverHost = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Server"),
		ScriptValue::fromString("host")
	});
	EXPECT_EQ(serverHost.asString(), "localhost");

	auto serverPort = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Server"),
		ScriptValue::fromString("port")
	});
	EXPECT_EQ(serverPort.asString(), "8080");

	// 验证 Database 配置
	auto dbHost = callModule(mod, "get", {
		data,
		ScriptValue::fromString("Database"),
		ScriptValue::fromString("host")
	});
	EXPECT_EQ(dbHost.asString(), "db.example.com");

	// 验证全局键
	auto globalKey = callModule(mod, "get", {
		data,
		ScriptValue::fromString(""),
		ScriptValue::fromString("global_key")
	});
	EXPECT_EQ(globalKey.asString(), "global_value");
}
