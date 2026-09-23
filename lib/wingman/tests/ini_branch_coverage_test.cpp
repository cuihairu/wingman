#include <gtest/gtest.h>
#include "wingman/script/module_registry.hpp"

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

// ini_module.cpp 第八批覆盖率收口（2026-09-23）：ini_module_test.cpp 覆盖了
// 常规编解码/往返/合并主路径，本文件补齐残余分支——转义矩阵中的 \r \t \\ 与
// 非法转义、解码对畸形行/畸形 section 的容错、编码对非法 key/section 名的
// 警告路径、get/set/delete/has_* 的参数与类型防御、merge 的类型覆盖分支。

namespace {

	ModuleDescriptor getIniModule() {
		for (auto& mod : getAllModules()) {
			if (mod.name == "ini") return mod;
		}
		return {};
	}

	ScriptValue call(const ModuleDescriptor& mod, const std::string& fn, std::vector<ScriptValue> args) {
		for (const auto& f : mod.functions) {
			if (f.name == fn) return f.func(args);
		}
		return ScriptValue::null();
	}

	ScriptValue obj(std::unordered_map<std::string, ScriptValue> fields) {
		return ScriptValue::fromObject(std::move(fields));
	}

	class IniBranchTest : public ::testing::Test {
	protected:
		void SetUp() override {
			mod_ = getIniModule();
			ASSERT_FALSE(mod_.name.empty());
		}

		ScriptValue call(const std::string& fn, std::vector<ScriptValue> args) {
			return ::call(mod_, fn, std::move(args));
		}

		ModuleDescriptor mod_;
	};

} // anonymous namespace

// ========== decode 容错 ==========

// 非法 section 行与非法 key=value 行：警告并跳过，其余内容正常解析
TEST_F(IniBranchTest, DecodeMalformedLinesSkipped) {
	auto result = call("decode", {ScriptValue::fromString(
		"[unclosed\n"     // section 缺右括号
		"[]\n"            // 空 section 名 → trim 后合法？名字为空仍创建 section
		"no_equals_here\n" // 无等号
		"=novalue\n"      // key 为空
		"ok = fine\n")});

	ASSERT_TRUE(result.isObject());
	// 非法行全部被跳过，只有合法 kv 进入全局 section
	ASSERT_NE(result.get(""), nullptr);
	EXPECT_NE(result.get("")->get("ok"), nullptr);
	EXPECT_EQ(result.get("")->get("ok")->asString(), "fine");
	EXPECT_EQ(result.get("")->get("no_equals_here"), nullptr);
	EXPECT_EQ(result.get("")->get(""), nullptr);
}

// decode 空参数 → 空对象
TEST_F(IniBranchTest, DecodeEmptyArgsReturnsEmptyObject) {
	auto result = call("decode", {});
	EXPECT_TRUE(result.isObject());
	EXPECT_EQ(result.size(), 0u);
}

// 未知转义序列保留反斜杠原样
TEST_F(IniBranchTest, DecodeUnknownEscapeKeepsBackslash) {
	auto result = call("decode", {ScriptValue::fromString("[s]\nv = a\\xb\n")});
	ASSERT_TRUE(result.isObject());
	const auto* sec = result.get("s");
	ASSERT_NE(sec, nullptr);
	const auto* v = sec->get("v");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->asString(), "a\\xb");
}

// ========== encode 防御与转义矩阵 ==========

// 非对象输入 → 空串
TEST_F(IniBranchTest, EncodeRejectsNonObject) {
	EXPECT_EQ(call("encode", {}).asString(), "");
	EXPECT_EQ(call("encode", {ScriptValue::fromString("not-an-object")}).asString(), "");
}

// 值不是 Object 的 section 被跳过（不产出非法 INI）
TEST_F(IniBranchTest, EncodeSkipsNonObjectSection) {
	auto data = obj({
		{"bad", ScriptValue::fromString("i-am-a-string")},
		{"good", obj({{"k", ScriptValue::fromString("v")}})},
	});
	auto out = call("encode", {data});
	EXPECT_EQ(out.asString().find("bad"), std::string::npos);
	EXPECT_NE(out.asString().find("[good]"), std::string::npos);
}

// 非法 key/section 名字符触发警告但仍产出（降级不失败）
TEST_F(IniBranchTest, EncodeWarnsInvalidKeyAndSectionNames) {
	auto data = obj({
		{"sec]tion", obj({{"we=ird", ScriptValue::fromString("1")}})},
	});
	auto out = call("encode", {data});
	EXPECT_NE(out.asString().find("we=ird=1"), std::string::npos);
	EXPECT_NE(out.asString().find("[sec]tion]"), std::string::npos);
}

// 控制字符转义矩阵：\n \r \t \\ 编码后解码还原（round trip）
TEST_F(IniBranchTest, EscapeMatrixRoundTrip) {
	const std::string tricky = "line1\nline2\rret\ttab\\slash";
	auto data = obj({{"s", obj({{"v", ScriptValue::fromString(tricky)}})}});

	auto encoded = call("encode", {data});
	// 编码结果单行内不含裸控制字符
	EXPECT_EQ(encoded.asString().find("\r"), std::string::npos);
	EXPECT_EQ(encoded.asString().find("\t"), std::string::npos);

	auto decoded = call("decode", {encoded});
	const auto* v = decoded.get("s")->get("v");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->asString(), tricky);
}

// ========== get / set / delete 防御分支 ==========

TEST_F(IniBranchTest, GetDefensiveBranches) {
	auto data = obj({{"s", obj({{"k", ScriptValue::fromString("v")}})}});

	EXPECT_TRUE(call("get", {}).isNull());                       // 缺参
	EXPECT_TRUE(call("get", {ScriptValue::fromString("str"), ScriptValue::fromString("s")}).isNull());
	EXPECT_TRUE(call("get", {data, ScriptValue::fromString("noexist")}).isNull()); // section 不存在
	// section 值非 Object
	auto bad = obj({{"s", ScriptValue::fromString("notobj")}});
	EXPECT_TRUE(call("get", {bad, ScriptValue::fromString("s")}).isNull());
	// key 不存在 → null
	EXPECT_TRUE(call("get", {data, ScriptValue::fromString("s"), ScriptValue::fromString("nokey")}).isNull());
	// key 存在 → 值
	EXPECT_EQ(call("get", {data, ScriptValue::fromString("s"), ScriptValue::fromString("k")}).asString(), "v");
}

TEST_F(IniBranchTest, SetDefensiveBranches) {
	auto data = obj({{"s", obj({{"k", ScriptValue::fromString("old")}})}});

	EXPECT_TRUE(call("set", {data, ScriptValue::fromString("s")}).isNull());            // 缺 key
	EXPECT_TRUE(call("set", {ScriptValue::fromString("str"), ScriptValue::fromString("s"),
							 ScriptValue::fromString("k"), ScriptValue::fromString("v")}).isNull()); // 非 Object
	// section 值非 Object → 无法合并 → null
	auto bad = obj({{"s", ScriptValue::fromString("notobj")}});
	EXPECT_TRUE(call("set", {bad, ScriptValue::fromString("s"), ScriptValue::fromString("k"),
							 ScriptValue::fromString("v")}).isNull());
	// 正常 set：新 section 创建 / 旧值覆盖
	auto out1 = call("set", {data, ScriptValue::fromString("new"), ScriptValue::fromString("k"),
							 ScriptValue::fromString("v")});
	ASSERT_TRUE(out1.isObject());
	EXPECT_EQ(out1.get("new")->get("k")->asString(), "v");
	// 原 data 不被就地修改（副本语义）
	EXPECT_EQ(data.get("new"), nullptr);

	auto out2 = call("set", {data, ScriptValue::fromString("s"), ScriptValue::fromString("k"),
							 ScriptValue::fromString("fresh")});
	EXPECT_EQ(out2.get("s")->get("k")->asString(), "fresh");
	EXPECT_EQ(data.get("s")->get("k")->asString(), "old");
}

TEST_F(IniBranchTest, DeleteDefensiveBranches) {
	auto data = obj({
		{"s", obj({{"k", ScriptValue::fromString("v")}, {"k2", ScriptValue::fromString("v2")}})},
		{"t", ScriptValue::fromString("notobj")},
	});

	EXPECT_TRUE(call("delete", {}).isNull());                   // 缺参
	EXPECT_TRUE(call("delete", {ScriptValue::fromString("str"), ScriptValue::fromString("s")}).isNull());
	// section 值非 Object → 原样返回副本
	auto out1 = call("delete", {data, ScriptValue::fromString("t"), ScriptValue::fromString("k")});
	ASSERT_TRUE(out1.isObject());
	EXPECT_NE(out1.get("t"), nullptr);
	// 删 key / 删整个 section
	auto out2 = call("delete", {data, ScriptValue::fromString("s"), ScriptValue::fromString("k")});
	EXPECT_EQ(out2.get("s")->get("k"), nullptr);
	EXPECT_NE(out2.get("s")->get("k2"), nullptr);
	auto out3 = call("delete", {data, ScriptValue::fromString("s")});
	EXPECT_EQ(out3.get("s"), nullptr);
}

// ========== has_section / has_key / sections / keys 防御分支 ==========

TEST_F(IniBranchTest, HasSectionAndHasKeyDefensive) {
	auto data = obj({{"s", obj({{"k", ScriptValue::fromString("v")}})}});
	auto bad = obj({{"s", ScriptValue::fromString("notobj")}});

	EXPECT_FALSE(call("has_section", {data}).asBool());
	EXPECT_FALSE(call("has_section", {ScriptValue::fromString("x"), ScriptValue::fromString("s")}).asBool());
	EXPECT_TRUE(call("has_section", {data, ScriptValue::fromString("s")}).asBool());
	EXPECT_FALSE(call("has_section", {data, ScriptValue::fromString("no")}).asBool());

	EXPECT_FALSE(call("has_key", {data, ScriptValue::fromString("s")}).asBool());
	EXPECT_FALSE(call("has_key", {ScriptValue::fromString("x"), ScriptValue::fromString("s"),
								  ScriptValue::fromString("k")}).asBool());
	EXPECT_TRUE(call("has_key", {data, ScriptValue::fromString("s"), ScriptValue::fromString("k")}).asBool());
	EXPECT_FALSE(call("has_key", {data, ScriptValue::fromString("s"), ScriptValue::fromString("no")}).asBool());
	// section 值非 Object → false
	EXPECT_FALSE(call("has_key", {bad, ScriptValue::fromString("s"), ScriptValue::fromString("k")}).asBool());
}

TEST_F(IniBranchTest, SectionsAndKeysDefensive) {
	auto data = obj({{"s", obj({{"k", ScriptValue::fromString("v")}})}});

	EXPECT_EQ(call("sections", {}).size(), 0u);
	EXPECT_EQ(call("sections", {ScriptValue::fromString("x")}).size(), 0u);
	EXPECT_EQ(call("sections", {data}).size(), 1u);

	EXPECT_EQ(call("keys", {}).size(), 0u);                    // 缺参
	EXPECT_EQ(call("keys", {ScriptValue::fromString("x"), ScriptValue::fromString("s")}).size(), 0u);
	EXPECT_EQ(call("keys", {data, ScriptValue::fromString("noexist")}).size(), 0u);
	EXPECT_EQ(call("keys", {data, ScriptValue::fromString("s")}).size(), 1u);
}

// ========== merge 防御与类型覆盖 ==========

TEST_F(IniBranchTest, MergeDefensiveAndTypeOverride) {
	EXPECT_TRUE(call("merge", {}).isNull());                   // 缺参
	EXPECT_TRUE(call("merge", {ScriptValue::fromString("x")}).isNull());
	EXPECT_TRUE(call("merge", {ScriptValue::fromString("x"),
							   ScriptValue::fromString("y")}).isNull());

	// 同名 section 类型不匹配（object vs string）→ 整体覆盖
	auto base = obj({
		{"keep", obj({{"a", ScriptValue::fromString("1")}})},
		{"clash", obj({{"old", ScriptValue::fromString("x")}})},
	});
	auto patch = obj({
		{"clash", ScriptValue::fromString("scalar-wins")},
		{"new", obj({{"b", ScriptValue::fromString("2")}})},
	});
	auto merged = call("merge", {base, patch});
	ASSERT_TRUE(merged.isObject());
	EXPECT_EQ(merged.get("clash")->asString(), "scalar-wins");   // 类型覆盖
	EXPECT_EQ(merged.get("keep")->get("a")->asString(), "1");    // 未涉及 section 保留
	EXPECT_EQ(merged.get("new")->get("b")->asString(), "2");     // 新 section 复制
	// base 不被就地修改
	EXPECT_TRUE(base.get("clash")->isObject());
}
