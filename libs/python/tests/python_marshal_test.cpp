/**
 * ScriptValue ↔ py::object 双向转换（python_marshal）测试。
 *
 * 与 python_name_conversion_test（纯函数、不需要解释器）不同：marshal 的
 * 两端都是真实 Python 对象，必须在嵌入式 CPython 里跑。转换函数本身不取
 * GIL（调用方负责），单线程测试在解释器 GIL 下直接调用即可。
 *
 * 覆盖即契约：标量/容器往返、bool 先于 int 判定（Python 里 bool 是 int
 * 子类，判定顺序反了类型就全错）、tuple → array、dict 非字符串键丢弃、
 * 异常回落 null。这些行为此前零测试覆盖，Python 维度进 CI 前先锁住。
 */
#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <memory>

#include "wingman/python/python_marshal.hpp"

namespace py = pybind11;
using namespace pybind11::literals; // NOLINT -- "key"_a 字面量构造 dict
using wingman::script::ScriptValue;
using wingman::python::toPythonObject;
using wingman::python::toScriptValue;

namespace {

// 进程级解释器：整个可执行文件只起一次，测试用例在其 GIL 下运行
class PythonInterpreterEnvironment : public ::testing::Environment {
public:
	void SetUp() override {
		interpreter_ = std::make_unique<py::scoped_interpreter>();
	}
	void TearDown() override {
		interpreter_.reset();
	}

private:
	std::unique_ptr<py::scoped_interpreter> interpreter_;
};

} // namespace

// ========== 标量往返 ==========

TEST(MarshalScalarTest, NullRoundTrip) {
	ScriptValue nullValue;
	auto obj = toPythonObject(nullValue);
	EXPECT_TRUE(obj.is_none());
	auto back = toScriptValue(py::none());
	EXPECT_TRUE(back.isNull());
}

TEST(MarshalScalarTest, BoolRoundTripAndTypeOrder) {
	// Python 里 bool 是 int 的子类：toScriptValue 必须先判 bool_ 再判 int_，
	// 顺序错了 True 会变 1 —— 这里锁的就是判定顺序
	auto backTrue = toScriptValue(py::bool_(true));
	EXPECT_TRUE(backTrue.isBool());
	EXPECT_TRUE(backTrue.boolVal);

	auto backFalse = toScriptValue(py::bool_(false));
	EXPECT_TRUE(backFalse.isBool());
	EXPECT_FALSE(backFalse.boolVal);

	auto pyBool = toPythonObject(ScriptValue::fromBool(true));
	EXPECT_TRUE(py::isinstance<py::bool_>(pyBool));
}

TEST(MarshalScalarTest, IntRoundTrip) {
	const int64_t cases[] = {0, 42, -7, INT64_C(9007199254740993)};
	for (const int64_t v : cases) {
		auto back = toScriptValue(py::int_(v));
		ASSERT_TRUE(back.isInt()) << v;
		EXPECT_EQ(back.intVal, v) << v;

		auto pyInt = toPythonObject(ScriptValue::fromInt(v));
		EXPECT_EQ(pyInt.cast<int64_t>(), v) << v;
	}
}

TEST(MarshalScalarTest, FloatRoundTrip) {
	auto back = toScriptValue(py::float_(2.5));
	EXPECT_TRUE(back.isFloat());
	EXPECT_DOUBLE_EQ(back.floatVal, 2.5);

	// 整数值的 float 必须保持 Float，不被收窄成 Int
	auto pyFloat = toPythonObject(ScriptValue::fromFloat(3.0));
	EXPECT_TRUE(py::isinstance<py::float_>(pyFloat));
	auto backFloat = toScriptValue(py::float_(3.0));
	EXPECT_TRUE(backFloat.isFloat());
}

TEST(MarshalScalarTest, StringRoundTripWithUtf8) {
	const std::string utf8 = "你好 wingman";
	auto back = toScriptValue(py::str(utf8));
	EXPECT_TRUE(back.isString());
	EXPECT_EQ(back.strVal, utf8);

	auto pyStr = toPythonObject(ScriptValue::fromString(utf8));
	EXPECT_EQ(pyStr.cast<std::string>(), utf8);
}

// ========== 容器往返 ==========

TEST(MarshalContainerTest, ArrayRoundTrip) {
	ScriptValue arr = ScriptValue::fromArray(
		{ScriptValue::fromInt(1), ScriptValue::fromString("a"), ScriptValue::fromBool(true)});

	auto obj = toPythonObject(arr);
	ASSERT_TRUE(py::isinstance<py::list>(obj));
	auto pyList = obj.cast<py::list>();
	ASSERT_EQ(pyList.size(), 3);
	EXPECT_EQ(pyList[0].cast<int64_t>(), 1);
	EXPECT_EQ(pyList[1].cast<std::string>(), "a");
	EXPECT_TRUE(pyList[2].cast<bool>());

	auto back = toScriptValue(pyList);
	ASSERT_TRUE(back.isArray());
	ASSERT_EQ(back.arrayVal.size(), 3u);
	EXPECT_TRUE(back.arrayVal[0].isInt());
	EXPECT_EQ(back.arrayVal[0].intVal, 1);
	EXPECT_TRUE(back.arrayVal[1].isString());
	EXPECT_TRUE(back.arrayVal[2].isBool());
}

TEST(MarshalContainerTest, NestedArrayRoundTrip) {
	ScriptValue nested = ScriptValue::fromArray(
		{ScriptValue::fromArray({ScriptValue::fromInt(1), ScriptValue::fromInt(2)}),
		 ScriptValue::fromString("tail")});

	auto back = toScriptValue(toPythonObject(nested));
	ASSERT_TRUE(back.isArray());
	ASSERT_EQ(back.arrayVal.size(), 2u);
	ASSERT_TRUE(back.arrayVal[0].isArray());
	EXPECT_EQ(back.arrayVal[0].arrayVal.size(), 2u);
	EXPECT_EQ(back.arrayVal[0].arrayVal[1].intVal, 2);
	EXPECT_EQ(back.arrayVal[1].strVal, "tail");
}

TEST(MarshalContainerTest, EmptyContainers) {
	auto emptyList = toScriptValue(py::list());
	EXPECT_TRUE(emptyList.isArray());
	EXPECT_EQ(emptyList.size(), 0u);

	auto emptyDict = toScriptValue(py::dict());
	EXPECT_TRUE(emptyDict.isObject());
	EXPECT_TRUE(emptyDict.objectVal.empty());
}

TEST(MarshalContainerTest, ObjectRoundTrip) {
	auto dct = py::dict("alpha"_a = 1, "beta"_a = "two");
	auto back = toScriptValue(dct);

	ASSERT_TRUE(back.isObject());
	ASSERT_EQ(back.objectVal.size(), 2u);
	ASSERT_NE(back.get("alpha"), nullptr);
	EXPECT_EQ(back.get("alpha")->intVal, 1);
	ASSERT_NE(back.get("beta"), nullptr);
	EXPECT_EQ(back.get("beta")->strVal, "two");

	// ScriptValue Object → py::dict
	ScriptValue obj = ScriptValue::fromObject({{"x", ScriptValue::fromInt(9)}});
	auto pyDict = toPythonObject(obj);
	ASSERT_TRUE(py::isinstance<py::dict>(pyDict));
	EXPECT_EQ(pyDict["x"].cast<int64_t>(), 9);
}

TEST(MarshalContainerTest, DictDropsNonStringKeys) {
	// dictToScriptValue 的既定行为：非字符串键不进 ScriptValue（键类型是
	// std::string），静默丢弃而不是报错 —— 锁住防有人改成抛异常或强转。
	// alpha/beta 两个字符串键都保留，int 键被丢
	auto dct = py::dict("alpha"_a = 1, "beta"_a = "two");
	dct[py::int_(7)] = py::str("int key");
	auto back = toScriptValue(dct);

	ASSERT_TRUE(back.isObject());
	ASSERT_EQ(back.objectVal.size(), 2u);
	EXPECT_NE(back.get("alpha"), nullptr);
	EXPECT_EQ(back.get("alpha")->intVal, 1);
	EXPECT_NE(back.get("beta"), nullptr);
	EXPECT_EQ(back.get("7"), nullptr);
}

TEST(MarshalContainerTest, TupleBecomesArray) {
	auto tpl = py::make_tuple(py::int_(1), py::str("x"));
	auto back = toScriptValue(tpl);

	ASSERT_TRUE(back.isArray());
	ASSERT_EQ(back.arrayVal.size(), 2u);
	EXPECT_TRUE(back.arrayVal[0].isInt());
	EXPECT_EQ(back.arrayVal[0].intVal, 1);
	EXPECT_EQ(back.arrayVal[1].strVal, "x");
}

// ========== 可调用对象 ==========

TEST(MarshalCallableTest, PythonFunctionBecomesCallable) {
	py::dict ns;
	py::exec("def add(a, b):\n    return a + b\n", py::globals(), ns);
	auto func = ns["add"];

	auto value = toScriptValue(py::reinterpret_borrow<py::object>(func));
	ASSERT_TRUE(value.isCallable());

	auto result = value.call({ScriptValue::fromInt(2), ScriptValue::fromInt(3)});
	EXPECT_TRUE(result.isInt());
	EXPECT_EQ(result.intVal, 5);
}

TEST(MarshalCallableTest, PythonFunctionExceptionYieldsNull) {
	py::dict ns;
	py::exec("def boom():\n    raise ValueError('nope')\n", py::globals(), ns);

	auto value = toScriptValue(ns["boom"]);
	ASSERT_TRUE(value.isCallable());
	auto result = value.call({});
	// 异常回落为 null（引擎约定：回调抛错不炸宿主）
	EXPECT_TRUE(result.isNull());
}

TEST(MarshalCallableTest, CppCallableBecomesPythonFunction) {
	bool invoked = false;
	auto callable = ScriptValue::fromCallable(
		[&invoked](const std::vector<ScriptValue>& args) -> ScriptValue {
			invoked = true;
			int64_t sum = 0;
			for (const auto& a : args) {
				sum += a.asInt();
			}
			return ScriptValue::fromInt(sum);
		});

	auto func = toPythonObject(callable);
	ASSERT_TRUE(py::isinstance<py::function>(func));
	auto result = func(1, 2, 3);
	EXPECT_TRUE(invoked);
	EXPECT_EQ(result.cast<int64_t>(), 6);
}

// ========== 兜底 ==========

TEST(MarshalFallbackTest, UnknownObjectTypeYieldsNull) {
	// 无类型信息的自定义对象不假装能转 —— 回落 null
	py::object unknown = py::module_::import("types").attr("SimpleNamespace")();
	auto back = toScriptValue(unknown);
	EXPECT_TRUE(back.isNull());
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new PythonInterpreterEnvironment());
	return RUN_ALL_TESTS();
}
