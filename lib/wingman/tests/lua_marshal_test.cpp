/**
 * ScriptValue ↔ sol::object 双向转换（lua_marshal）测试。
 *
 * 与 python_marshal_test（libs/python/tests）同口径的 Lua 侧镜像：marshal
 * 两端都是真实 Lua 栈对象，必须在嵌入式 Lua state 里跑。此前该文件 40%
 * 覆盖（仅 timer_module 间接触发零星分支），转换层契约零直测。
 *
 * 覆盖即契约：标量/容器往返、lua_isinteger 整数性分流（42→Int、2.5→Float）、
 * table 数组/对象智能判别（1 起始连续整数为数组、字符串键为对象、混合/非正
 * 整数键落对象分支）、callable 双向包装（C++ callable 经 variadic_args 实参
 * marshaling 回 Lua、Lua function 包装为非线程安全 ScriptValue callable）、
 * userdata 回落 null。
 */
#include <gtest/gtest.h>
#include <sol/sol.hpp>

#include "wingman/lua/lua_marshal.hpp"

using wingman::script::ScriptValue;
using wingman::lua::toLuaObject;
using wingman::lua::toScriptValue;
using wingman::lua::tableToScriptValue;

namespace {

class LuaMarshalTest : public ::testing::Test {
protected:
	void SetUp() override {
		lua_.open_libraries(sol::lib::base);
	}

	sol::state lua_;
};

} // namespace

// ========== toLuaObject：ScriptValue → Lua ==========

TEST_F(LuaMarshalTest, NullBecomesNil) {
	auto obj = toLuaObject(lua_, ScriptValue::null());
	EXPECT_EQ(obj.get_type(), sol::type::lua_nil);
}

TEST_F(LuaMarshalTest, BoolBecomesBoolean) {
	auto obj = toLuaObject(lua_, ScriptValue::fromBool(true));
	ASSERT_TRUE(obj.is<bool>());
	EXPECT_TRUE(obj.as<bool>());
}

TEST_F(LuaMarshalTest, IntBecomesInteger) {
	auto obj = toLuaObject(lua_, ScriptValue::fromInt(-42));
	EXPECT_EQ(obj.get_type(), sol::type::number);
	// 整数性在 Lua 5.4 是真实类型属性：-42 必须保持 integer 而非 42.0
	obj.push(lua_.lua_state());
	EXPECT_TRUE(lua_isinteger(lua_.lua_state(), -1) != 0);
	lua_pop(lua_.lua_state(), 1);
	EXPECT_EQ(obj.as<int64_t>(), -42);
}

TEST_F(LuaMarshalTest, FloatBecomesNumber) {
	auto obj = toLuaObject(lua_, ScriptValue::fromFloat(2.5));
	EXPECT_EQ(obj.get_type(), sol::type::number);
	EXPECT_DOUBLE_EQ(obj.as<double>(), 2.5);
}

TEST_F(LuaMarshalTest, StringBecomesString) {
	auto obj = toLuaObject(lua_, ScriptValue::fromString("翼人 wingman\n\t\"引号\""));
	EXPECT_EQ(obj.as<std::string>(), "翼人 wingman\n\t\"引号\"");
}

TEST_F(LuaMarshalTest, ArrayBecomesSequenceTable) {
	auto obj = toLuaObject(lua_, ScriptValue::fromArray(std::vector<ScriptValue>{
		ScriptValue::fromInt(1), ScriptValue::fromString("two"), ScriptValue::fromFloat(3.5)}));
	auto tbl = obj.as<sol::table>();
	EXPECT_EQ(tbl[1].get<int>(), 1);
	EXPECT_EQ(tbl[2].get<std::string>(), "two");
	EXPECT_DOUBLE_EQ(tbl[3].get<double>(), 3.5);
}

TEST_F(LuaMarshalTest, ObjectBecomesMapTable) {
	auto obj = toLuaObject(lua_, ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
		{"name", ScriptValue::fromString("wingman")},
		{"count", ScriptValue::fromInt(3)},
	}));
	auto tbl = obj.as<sol::table>();
	EXPECT_EQ(tbl["name"].get<std::string>(), "wingman");
	EXPECT_EQ(tbl["count"].get<int>(), 3);
}

TEST_F(LuaMarshalTest, NestedContainersRoundTripThroughLua) {
	ScriptValue inner = ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
		{"x", ScriptValue::fromInt(7)}});
	ScriptValue nested = ScriptValue::fromArray(std::vector<ScriptValue>{inner, ScriptValue::fromString("tail")});
	auto obj = toLuaObject(lua_, nested);
	auto back = toScriptValue(obj);
	ASSERT_TRUE(back.type == ScriptValue::Type::Array);
	ASSERT_EQ(back.arrayVal.size(), 2u);
	ASSERT_TRUE(back.arrayVal[0].type == ScriptValue::Type::Object);
	EXPECT_EQ(back.arrayVal[0].objectVal.at("x").asInt(), 7);
	EXPECT_EQ(back.arrayVal[1].asString(), "tail");
}

TEST_F(LuaMarshalTest, CxxCallableCallableFromLuaWithMarshaledArgs) {
	// C++ callable → Lua function：variadic_args 实参经 toScriptValue 逐个转换，
	// 返回值再经 toLuaObject 回 Lua
	ScriptValue sv = ScriptValue::fromCallable(
		[](const std::vector<ScriptValue>& args) -> ScriptValue {
			EXPECT_EQ(args.size(), 5u);
			EXPECT_EQ(args[0].type, ScriptValue::Type::Int);
			EXPECT_EQ(args[0].asInt(), 21);
			EXPECT_EQ(args[1].type, ScriptValue::Type::String);
			EXPECT_EQ(args[1].asString(), "s");
			EXPECT_EQ(args[2].type, ScriptValue::Type::Bool);
			EXPECT_TRUE(args[2].asBool());
			EXPECT_EQ(args[3].type, ScriptValue::Type::Null);  // Lua nil
			EXPECT_EQ(args[4].type, ScriptValue::Type::Array); // 嵌套 table 实参
			if (args[4].type != ScriptValue::Type::Array) {
				return ScriptValue::fromInt(-1);
			}
			EXPECT_EQ(args[4].arrayVal.size(), 1u);
			return ScriptValue::fromInt(args[0].asInt() * 2);
		});
	lua_["f"] = toLuaObject(lua_, sv);
	auto r = lua_.safe_script("r = f(21, 's', true, nil, {2}); return r");
	ASSERT_TRUE(r.valid());
	EXPECT_EQ(lua_["r"].get<int>(), 42);
}

TEST_F(LuaMarshalTest, CxxCallableReturningStringRoundTrips) {
	ScriptValue sv = ScriptValue::fromCallable(
		[](const std::vector<ScriptValue>&) -> ScriptValue {
			return ScriptValue::fromString("done");
		}, /*threadSafe=*/true);
	lua_["g"] = toLuaObject(lua_, sv);
	auto r = lua_.safe_script("s = g(); return s");
	ASSERT_TRUE(r.valid());
	EXPECT_EQ(lua_["s"].get<std::string>(), "done");
}

// ========== toScriptValue：sol::object → ScriptValue ==========

TEST_F(LuaMarshalTest, InvalidObjectBecomesNull) {
	sol::object invalid{};  // 默认构造 = invalid
	auto v = toScriptValue(invalid);
	EXPECT_TRUE(v.type == ScriptValue::Type::Null);
}

TEST_F(LuaMarshalTest, NilBecomesNull) {
	auto v = toScriptValue(sol::make_object(lua_, sol::lua_nil));
	EXPECT_TRUE(v.type == ScriptValue::Type::Null);
}

TEST_F(LuaMarshalTest, BoolRoundTrip) {
	lua_.script("b = true");
	auto v = toScriptValue(lua_["b"]);
	EXPECT_EQ(v.type, ScriptValue::Type::Bool);
	EXPECT_TRUE(v.asBool());
}

TEST_F(LuaMarshalTest, IntegerStaysInteger) {
	// lua_isinteger 分流：42 写进 Lua 是 integer，回来必须是 Int 而非 Float
	lua_.script("n = 42");
	auto v = toScriptValue(lua_["n"]);
	EXPECT_EQ(v.type, ScriptValue::Type::Int);
	EXPECT_EQ(v.asInt(), 42);
}

TEST_F(LuaMarshalTest, NonIntegerNumberBecomesFloat) {
	lua_.script("x = 2.5");
	auto v = toScriptValue(lua_["x"]);
	EXPECT_EQ(v.type, ScriptValue::Type::Float);
	EXPECT_DOUBLE_EQ(v.asFloat(), 2.5);
}

TEST_F(LuaMarshalTest, StringRoundTrip) {
	lua_.script("s = '中文\\nvalue'");
	auto v = toScriptValue(lua_["s"]);
	EXPECT_EQ(v.type, ScriptValue::Type::String);
	EXPECT_EQ(v.asString(), "中文\nvalue");
}

TEST_F(LuaMarshalTest, LuaFunctionBecomesNonThreadSafeCallable) {
	// Lua function 包装为 ScriptValue callable：显式 NOT thread-safe（line 97
	// 契约），实参 toLuaObject 转换后调用、结果 toScriptValue 回收
	lua_.script("function g(a, b) return a .. ':' .. b end");
	auto v = toScriptValue(lua_["g"]);
	ASSERT_TRUE(v.isCallable());
	EXPECT_FALSE(v.callableThreadSafe);

	auto result = v.callableVal({ScriptValue::fromString("x"), ScriptValue::fromString("y")});
	EXPECT_EQ(result.type, ScriptValue::Type::String);
	EXPECT_EQ(result.asString(), "x:y");
}

TEST_F(LuaMarshalTest, LuaFunctionWithNumericArgsAndIntReturn) {
	lua_.script("function add(a, b) return a + b end");
	auto v = toScriptValue(lua_["add"]);
	ASSERT_TRUE(v.isCallable());
	auto result = v.callableVal({ScriptValue::fromInt(40), ScriptValue::fromInt(2)});
	EXPECT_EQ(result.type, ScriptValue::Type::Int);
	EXPECT_EQ(result.asInt(), 42);
}

TEST_F(LuaMarshalTest, ErroringLuaFunctionYieldsNull) {
	// 实证契约（按现状钉）：sol::function 经 as_args 的调用在该版本是 protected
	// 调用——Lua error 不向调用方抛异常，result.valid()==false 腿返回 null。
	// 与 lua_script_engine_test 直调 executeString 的「走 catch 腿」路径不同
	lua_.script("function bad() error('boom') end");
	auto v = toScriptValue(lua_["bad"]);
	ASSERT_TRUE(v.isCallable());
	auto result = v.callableVal({});
	EXPECT_TRUE(result.type == ScriptValue::Type::Null);
}

TEST_F(LuaMarshalTest, UserdataFallsBackToNull) {
	// 非 nil/bool/number/string/table/function 类型（userdata）→ null 兜底
	struct Dummy { int v = 1; };
	auto v = toScriptValue(sol::make_object(lua_, Dummy{}));
	EXPECT_TRUE(v.type == ScriptValue::Type::Null);
}

// ========== tableToScriptValue：数组/对象智能判别 ==========

TEST_F(LuaMarshalTest, SequenceTableBecomesArray) {
	lua_.script("t = {10, 20, 30}");
	auto v = toScriptValue(lua_["t"]);
	ASSERT_EQ(v.type, ScriptValue::Type::Array);
	ASSERT_EQ(v.arrayVal.size(), 3u);
	EXPECT_EQ(v.arrayVal[0].asInt(), 10);
	EXPECT_EQ(v.arrayVal[1].asInt(), 20);
	EXPECT_EQ(v.arrayVal[2].asInt(), 30);
}

TEST_F(LuaMarshalTest, StringKeyTableBecomesObject) {
	lua_.script("t = {name = 'w', n = 3}");
	auto v = toScriptValue(lua_["t"]);
	ASSERT_EQ(v.type, ScriptValue::Type::Object);
	EXPECT_EQ(v.objectVal.at("name").asString(), "w");
	EXPECT_EQ(v.objectVal.at("n").asInt(), 3);
}

TEST_F(LuaMarshalTest, EmptyTableFallsToObjectBranch) {
	// 空表：isArray 保持 true 但 maxIndex==0 → 落对象分支 → 空 Object（按现状钉）
	lua_.script("t = {}");
	auto v = toScriptValue(lua_["t"]);
	EXPECT_EQ(v.type, ScriptValue::Type::Object);
	EXPECT_TRUE(v.objectVal.empty());
}

TEST_F(LuaMarshalTest, MixedStringAndNumberKeysDropNumberKeys) {
	// 混合键 → 非数组 → 对象分支只保留字符串键（数字键丢弃，按现状钉）
	lua_.script("t = {[1] = 'a', b = 'c'}");
	auto v = toScriptValue(lua_["t"]);
	ASSERT_EQ(v.type, ScriptValue::Type::Object);
	EXPECT_EQ(v.objectVal.size(), 1u);
	EXPECT_EQ(v.objectVal.at("b").asString(), "c");
}

TEST_F(LuaMarshalTest, NonPositiveIntegerKeyDisqualifiesArray) {
	// 非正整数键触发 isArray=false（115-116 分支）→ 对象分支、无字符串键 → 空
	lua_.script("t = {[-1] = 'x'}");
	auto v = toScriptValue(lua_["t"]);
	ASSERT_EQ(v.type, ScriptValue::Type::Object);
	EXPECT_TRUE(v.objectVal.empty());
}

TEST_F(LuaMarshalTest, SparseArrayKeepsArrayShapeWithNullHole) {
	// 稀疏数组 {1='a', 3='c'}：仍判数组，maxIndex=3，空洞位为 Null（按现状钉）
	lua_.script("t = {[1] = 'a', [3] = 'c'}");
	auto v = toScriptValue(lua_["t"]);
	ASSERT_EQ(v.type, ScriptValue::Type::Array);
	ASSERT_EQ(v.arrayVal.size(), 3u);
	EXPECT_EQ(v.arrayVal[0].asString(), "a");
	EXPECT_EQ(v.arrayVal[1].type, ScriptValue::Type::Null);
	EXPECT_EQ(v.arrayVal[2].asString(), "c");
}

TEST_F(LuaMarshalTest, NestedTableValuesRecursivelyConverted) {
	lua_.script("t = { {1, 2}, {tag = 'deep'} }");
	auto v = toScriptValue(lua_["t"]);
	ASSERT_EQ(v.type, ScriptValue::Type::Array);
	ASSERT_EQ(v.arrayVal.size(), 2u);
	ASSERT_EQ(v.arrayVal[0].type, ScriptValue::Type::Array);
	EXPECT_EQ(v.arrayVal[0].arrayVal[1].asInt(), 2);
	ASSERT_EQ(v.arrayVal[1].type, ScriptValue::Type::Object);
	EXPECT_EQ(v.arrayVal[1].objectVal.at("tag").asString(), "deep");
}

TEST_F(LuaMarshalTest, DirectTableToScriptValueOnTableReference) {
	lua_.script("t = {5, 6}");
	sol::table tbl = lua_["t"];
	auto v = tableToScriptValue(tbl);
	ASSERT_EQ(v.type, ScriptValue::Type::Array);
	EXPECT_EQ(v.arrayVal[1].asInt(), 6);
}
