/**
 * module_helpers 张量转换层缺口补测。
 *
 * 此前该头文件 68% 覆盖，缺的 58 行几乎全部是张量 dtype 矩阵：11 个枚举值
 * 里只有 FLOAT32/FLOAT64/INT8/BOOL 被既有 ml/misc 用例顺带踩过，其余
 * dtype 的 elementSize / 类型名解析 / tensorAppendBytes<T> 各实例化 /
 * tensorAppendElement / tensorElementToScriptValue 分支全零覆盖。
 *
 * 覆盖即契约：一个 dtype 全矩阵往返（ScriptValue spec → tensorFromScriptValue
 * → TensorData → modelOutputToScriptValue → ScriptValue）同时驱动所有 dtype
 * 分支，再补错误矩阵（非对象/缺 data/空 data/非数组 data/未知与非字符串
 * dtype/shape 非数组）与未知 dtype 兜底（elementSize==0 → 空数据、直读 → null）。
 */
#include <gtest/gtest.h>

#include "script/modules/module_helpers.hpp"

#include <cstdint>
#include <string>

using namespace wingman;
using namespace wingman::script;
using namespace wingman::script::modules;

namespace {

// 全部 11 个 dtype（无值域外兜底），按枚举声明序
constexpr TensorDataType kAllDtypes[] = {
	TensorDataType::FLOAT32, TensorDataType::FLOAT64,
	TensorDataType::INT8,    TensorDataType::INT16,
	TensorDataType::INT32,   TensorDataType::INT64,
	TensorDataType::UINT8,   TensorDataType::UINT16,
	TensorDataType::UINT32,  TensorDataType::UINT64,
	TensorDataType::BOOL,
};

const char* dtypeName(TensorDataType t) {
	switch (t) {
	case TensorDataType::FLOAT32: return "float32";
	case TensorDataType::FLOAT64: return "float64";
	case TensorDataType::INT8:    return "int8";
	case TensorDataType::INT16:   return "int16";
	case TensorDataType::INT32:   return "int32";
	case TensorDataType::INT64:   return "int64";
	case TensorDataType::UINT8:   return "uint8";
	case TensorDataType::UINT16:  return "uint16";
	case TensorDataType::UINT32:  return "uint32";
	case TensorDataType::UINT64:  return "uint64";
	case TensorDataType::BOOL:    return "bool";
	}
	return "?";
}

// 构造 {data:[...], dtype:"x"} 规格（shape 缺省 → 一维）
ScriptValue specWithDtype(std::vector<ScriptValue> data, const char* dtype) {
	return ScriptValue::fromObject(std::unordered_map<std::string, ScriptValue>{
		{"data", ScriptValue::fromArray(std::move(data))},
		{"dtype", ScriptValue::fromString(dtype)},
	});
}

} // namespace

// ========== tensorElementSize / tensorTypeFromString 全枚举 ==========

TEST(ModuleHelpersTensorTest, ElementSizeCoversAllElevenDtypes) {
	EXPECT_EQ(tensorElementSize(TensorDataType::FLOAT32), 4u);
	EXPECT_EQ(tensorElementSize(TensorDataType::FLOAT64), 8u);
	EXPECT_EQ(tensorElementSize(TensorDataType::INT8), 1u);
	EXPECT_EQ(tensorElementSize(TensorDataType::INT16), 2u);
	EXPECT_EQ(tensorElementSize(TensorDataType::INT32), 4u);
	EXPECT_EQ(tensorElementSize(TensorDataType::INT64), 8u);
	EXPECT_EQ(tensorElementSize(TensorDataType::UINT8), 1u);
	EXPECT_EQ(tensorElementSize(TensorDataType::UINT16), 2u);
	EXPECT_EQ(tensorElementSize(TensorDataType::UINT32), 4u);
	EXPECT_EQ(tensorElementSize(TensorDataType::UINT64), 8u);
	EXPECT_EQ(tensorElementSize(TensorDataType::BOOL), 1u);
	// 未知 dtype 兜底 0（驱动 modelOutputToScriptValue 的空数据腿）
	EXPECT_EQ(tensorElementSize(static_cast<TensorDataType>(99)), 0u);
}

TEST(ModuleHelpersTensorTest, TypeFromStringAcceptsAllElevenNames) {
	for (TensorDataType expected : kAllDtypes) {
		TensorDataType out = TensorDataType::FLOAT32;
		EXPECT_TRUE(tensorTypeFromString(dtypeName(expected), out)) << dtypeName(expected);
		EXPECT_EQ(out, expected) << dtypeName(expected);
	}
}

TEST(ModuleHelpersTensorTest, TypeFromStringRejectsUnknownNamesCaseSensitively) {
	for (const char* bad : {"float16", "FLOAT32", "int", "", "uint128"}) {
		TensorDataType out = TensorDataType::INT64; // 哨兵：失败时必须保持不动
		EXPECT_FALSE(tensorTypeFromString(bad, out)) << bad;
		EXPECT_EQ(out, TensorDataType::INT64) << bad;
	}
}

// ========== dtype 全矩阵往返：spec → TensorData → ModelOutput → ScriptValue ==========

TEST(ModuleHelpersTensorTest, RoundTripAllDtypesThroughModelOutput) {
	struct Case {
		TensorDataType type;
		ScriptValue input;
	};
	const std::vector<Case> cases = {
		{TensorDataType::FLOAT32, ScriptValue::fromFloat(0.5)},   // float32 精确可表示
		{TensorDataType::FLOAT64, ScriptValue::fromFloat(3.25)},
		{TensorDataType::INT8,    ScriptValue::fromInt(-42)},
		{TensorDataType::INT16,   ScriptValue::fromInt(-300)},
		{TensorDataType::INT32,   ScriptValue::fromInt(-70000)},
		{TensorDataType::INT64,   ScriptValue::fromInt(5000000000LL)},
		{TensorDataType::UINT8,   ScriptValue::fromInt(255)},
		{TensorDataType::UINT16,  ScriptValue::fromInt(65535)},
		{TensorDataType::UINT32,  ScriptValue::fromInt(4000000000LL)},
		{TensorDataType::UINT64,  ScriptValue::fromInt(4600000000LL)},
		{TensorDataType::BOOL,    ScriptValue::fromInt(1)},
	};

	for (const auto& c : cases) {
		TensorData td;
		std::string err;
		ASSERT_TRUE(tensorFromScriptValue(specWithDtype({c.input}, dtypeName(c.type)), td, err))
			<< dtypeName(c.type) << ": " << err;
		EXPECT_EQ(td.dataType, c.type);
		EXPECT_EQ(td.shape, (TensorShape{1}));
		EXPECT_EQ(td.data.size(), tensorElementSize(c.type));

		ModelOutput out;
		out.name = "m";
		out.tensor = td;
		ScriptValue back = modelOutputToScriptValue(out);
		ASSERT_TRUE(back.isObject());
		EXPECT_EQ(back.get("name")->asString(), "m");
		const ScriptValue* shape = back.get("shape");
		ASSERT_NE(shape, nullptr);
		ASSERT_TRUE(shape->isArray());
		EXPECT_EQ(shape->arrayVal.size(), 1u);
		EXPECT_EQ(shape->arrayVal[0].asInt(), 1);

		const ScriptValue* data = back.get("data");
		ASSERT_NE(data, nullptr);
		ASSERT_TRUE(data->isArray());
		ASSERT_EQ(data->arrayVal.size(), 1u) << dtypeName(c.type);
		switch (c.type) {
		case TensorDataType::FLOAT32:
		case TensorDataType::FLOAT64:
			EXPECT_DOUBLE_EQ(data->arrayVal[0].asFloat(), c.input.asFloat()) << dtypeName(c.type);
			break;
		case TensorDataType::BOOL:
			EXPECT_TRUE(data->arrayVal[0].asBool()) << dtypeName(c.type);
			break;
		default:
			EXPECT_EQ(data->arrayVal[0].asInt(), c.input.asInt()) << dtypeName(c.type);
			break;
		}
	}
}

TEST(ModuleHelpersTensorTest, BoolZeroEncodesFalse) {
	TensorData td;
	std::string err;
	ASSERT_TRUE(tensorFromScriptValue(
		specWithDtype({ScriptValue::fromInt(0)}, "bool"), td, err));
	ModelOutput out;
	out.name = "b";
	out.tensor = td;
	ScriptValue back = modelOutputToScriptValue(out);
	EXPECT_FALSE(back.get("data")->arrayVal[0].asBool());
}

TEST(ModuleHelpersTensorTest, IntSpecValuesCoerceThroughAsFloat) {
	// 实参统一走 asFloat()：整数 2 → 2.0 → 各 dtype 截断/转换路径
	TensorData td;
	std::string err;
	ASSERT_TRUE(tensorFromScriptValue(
		specWithDtype({ScriptValue::fromInt(7), ScriptValue::fromInt(-1)}, "int32"), td, err));
	EXPECT_EQ(td.dataType, TensorDataType::INT32);
	ASSERT_EQ(td.data.size(), 8u);
	ModelOutput out;
	out.name = "i";
	out.tensor = td;
	ScriptValue back = modelOutputToScriptValue(out);
	ASSERT_EQ(back.get("data")->arrayVal.size(), 2u);
	EXPECT_EQ(back.get("data")->arrayVal[0].asInt(), 7);
	EXPECT_EQ(back.get("data")->arrayVal[1].asInt(), -1);
}

// ========== tensorFromScriptValue 错误矩阵 ==========

TEST(ModuleHelpersTensorTest, RejectsNonObjectSpec) {
	TensorData td;
	std::string err;
	EXPECT_FALSE(tensorFromScriptValue(ScriptValue::fromInt(5), td, err));
	EXPECT_EQ(err, "tensor input must be an object {data, shape?, dtype?}");
}

TEST(ModuleHelpersTensorTest, RejectsMissingOrEmptyOrNonArrayData) {
	TensorData td;
	std::string err;
	// 缺 data
	EXPECT_FALSE(tensorFromScriptValue(ScriptValue::fromObject(
		std::unordered_map<std::string, ScriptValue>{
			{"dtype", ScriptValue::fromString("float32")}}), td, err));
	EXPECT_EQ(err, "tensor input requires non-empty 'data' array");
	// 空 data 数组
	err.clear();
	EXPECT_FALSE(tensorFromScriptValue(specWithDtype({}, "float32"), td, err));
	EXPECT_EQ(err, "tensor input requires non-empty 'data' array");
	// data 非数组
	err.clear();
	EXPECT_FALSE(tensorFromScriptValue(ScriptValue::fromObject(
		std::unordered_map<std::string, ScriptValue>{
			{"data", ScriptValue::fromInt(1)}}), td, err));
	EXPECT_EQ(err, "tensor input requires non-empty 'data' array");
}

TEST(ModuleHelpersTensorTest, RejectsUnknownAndNonStringDtype) {
	TensorData td;
	std::string err;
	// 未知 dtype 名
	EXPECT_FALSE(tensorFromScriptValue(specWithDtype({ScriptValue::fromInt(1)}, "float16"), td, err));
	EXPECT_EQ(err, "unknown tensor dtype: float16");
	// dtype 非字符串：asString() 走默认值（空串）拼进错误信息（按现状钉）
	err.clear();
	EXPECT_FALSE(tensorFromScriptValue(ScriptValue::fromObject(
		std::unordered_map<std::string, ScriptValue>{
			{"data", ScriptValue::fromArray({ScriptValue::fromInt(1)})},
			{"dtype", ScriptValue::fromInt(3)}}), td, err));
	EXPECT_EQ(err, "unknown tensor dtype: ");
}

TEST(ModuleHelpersTensorTest, RejectsNonArrayShape) {
	TensorData td;
	std::string err;
	EXPECT_FALSE(tensorFromScriptValue(ScriptValue::fromObject(
		std::unordered_map<std::string, ScriptValue>{
			{"data", ScriptValue::fromArray({ScriptValue::fromInt(1)})},
			{"shape", ScriptValue::fromInt(3)}}), td, err));
	EXPECT_EQ(err, "tensor 'shape' must be an int array");
}

// ========== shape 语义 ==========

TEST(ModuleHelpersTensorTest, DefaultShapeIsOneDimensionOfDataLength) {
	TensorData td;
	std::string err;
	ASSERT_TRUE(tensorFromScriptValue(specWithDtype(
		{ScriptValue::fromFloat(1.0), ScriptValue::fromFloat(2.0), ScriptValue::fromFloat(3.0)},
		"float32"), td, err));
	EXPECT_EQ(td.shape, (TensorShape{3}));
	EXPECT_EQ(td.data.size(), 12u);
}

TEST(ModuleHelpersTensorTest, ExplicitShapeIsHonoredVerbatim) {
	TensorData td;
	std::string err;
	ASSERT_TRUE(tensorFromScriptValue(ScriptValue::fromObject(
		std::unordered_map<std::string, ScriptValue>{
			{"data", ScriptValue::fromArray({ScriptValue::fromFloat(1.0),
			                                 ScriptValue::fromFloat(2.0),
			                                 ScriptValue::fromFloat(3.0)})},
			{"shape", ScriptValue::fromArray({ScriptValue::fromInt(1),
			                                  ScriptValue::fromInt(3)})},
			{"dtype", ScriptValue::fromString("float64")}}), td, err));
	EXPECT_EQ(td.shape, (TensorShape{1, 3}));
	EXPECT_EQ(td.dataType, TensorDataType::FLOAT64);
	EXPECT_EQ(td.data.size(), 24u);
}

// ========== 未知 dtype 兜底 ==========

TEST(ModuleHelpersTensorTest, UnknownDtypeYieldsEmptyDataInModelOutput) {
	// elementSize==0 → modelOutputToScriptValue 不产出任何元素（守卫腿），shape 照常
	ModelOutput out;
	out.name = "weird";
	out.tensor.dataType = static_cast<TensorDataType>(99);
	out.tensor.shape = {2, 2};
	out.tensor.data.assign(8, 0xAB);
	ScriptValue back = modelOutputToScriptValue(out);
	EXPECT_EQ(back.get("name")->asString(), "weird");
	EXPECT_EQ(back.get("shape")->arrayVal.size(), 2u);
	EXPECT_TRUE(back.get("data")->arrayVal.empty());
}

TEST(ModuleHelpersTensorTest, UnknownDtypeDirectElementReadYieldsNull) {
	TensorData td;
	td.dataType = static_cast<TensorDataType>(42);
	td.data.assign(8, 0);
	EXPECT_TRUE(tensorElementToScriptValue(td, 0).type == ScriptValue::Type::Null);
}

TEST(ModuleHelpersTensorTest, AppendElementUnknownDtypeAppendsNothing) {
	// tensorFromScriptValue 入口拦住了未知 dtype，default:break 只有直调可达
	std::vector<uint8_t> bytes = {0x11};
	tensorAppendElement(bytes, static_cast<TensorDataType>(99), 1.0);
	EXPECT_EQ(bytes.size(), 1u);
	EXPECT_EQ(bytes[0], 0x11);
}
