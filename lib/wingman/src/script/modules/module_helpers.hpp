#pragma once

// Conversion utilities between ScriptValue and Wingman core types
// For internal use by module descriptors, not exposed to script engine

#include "wingman/script/iscript_engine.hpp"
#include "wingman/screen.hpp"
#include "wingman/ml.hpp"
#include <cstring>
#include <string>
#include <unordered_map>

namespace wingman {
namespace script {
namespace modules {

// ScriptValue → Rect
inline Rect toRect(const ScriptValue& v) {
	Rect r;
	if (v.isObject()) {
		r.x = static_cast<int>(v.get("x")->asInt());
		r.y = static_cast<int>(v.get("y")->asInt());
		r.width = static_cast<int>(v.get("width", ScriptValue::fromInt(0)).asInt());
		r.height = static_cast<int>(v.get("height", ScriptValue::fromInt(0)).asInt());
	}
	return r;
}

// Rect → ScriptValue
inline ScriptValue fromRect(const Rect& r) {
	return ScriptValue::fromObject({
		{"x", ScriptValue::fromInt(r.x)},
		{"y", ScriptValue::fromInt(r.y)},
		{"width", ScriptValue::fromInt(r.width)},
		{"height", ScriptValue::fromInt(r.height)}
	});
}

// ScriptValue -> Color (supports 0xRRGGBB integer or {r,g,b} table)
inline Color toColor(const ScriptValue& v) {
	if (v.isInt()) {
		return Color::fromRGB(static_cast<uint32_t>(v.asInt()));
	}
	if (v.isObject()) {
		Color c;
		c.r = static_cast<uint8_t>(v.get("r", ScriptValue::fromInt(0)).asInt());
		c.g = static_cast<uint8_t>(v.get("g", ScriptValue::fromInt(0)).asInt());
		c.b = static_cast<uint8_t>(v.get("b", ScriptValue::fromInt(0)).asInt());
		c.a = static_cast<uint8_t>(v.get("a", ScriptValue::fromInt(255)).asInt());
		return c;
	}
	return Color();
}

// Color → ScriptValue
inline ScriptValue fromColor(const Color& c) {
	return ScriptValue::fromObject({
		{"r", ScriptValue::fromInt(c.r)},
		{"g", ScriptValue::fromInt(c.g)},
		{"b", ScriptValue::fromInt(c.b)},
		{"a", ScriptValue::fromInt(c.a)}
	});
}

// Point → ScriptValue
inline ScriptValue fromPoint(const Point& p) {
	return ScriptValue::fromObject({
		{"x", ScriptValue::fromInt(p.x)},
		{"y", ScriptValue::fromInt(p.y)}
	});
}

// ========== Tensor ↔ ScriptValue（ml 模块用） ==========

// TensorShape → ScriptValue 数组
inline ScriptValue tensorShapeToScriptValue(const TensorShape& shape) {
	std::vector<ScriptValue> arr;
	arr.reserve(shape.size());
	for (int64_t d : shape) {
		arr.push_back(ScriptValue::fromInt(d));
	}
	return ScriptValue::fromArray(std::move(arr));
}

// 张量元素类型的字节数；未知类型返回 0
inline size_t tensorElementSize(TensorDataType type) {
	switch (type) {
		case TensorDataType::FLOAT32: return 4;
		case TensorDataType::FLOAT64: return 8;
		case TensorDataType::INT8:    return 1;
		case TensorDataType::INT16:   return 2;
		case TensorDataType::INT32:   return 4;
		case TensorDataType::INT64:   return 8;
		case TensorDataType::UINT8:   return 1;
		case TensorDataType::UINT16:  return 2;
		case TensorDataType::UINT32:  return 4;
		case TensorDataType::UINT64:  return 8;
		case TensorDataType::BOOL:    return 1;
		default: return 0;
	}
}

// dtype 名称解析："float32"/"int32"/...；返回 false 表示未知名称
inline bool tensorTypeFromString(const std::string& name, TensorDataType& out) {
	static const std::unordered_map<std::string, TensorDataType> kTypes = {
		{"float32", TensorDataType::FLOAT32}, {"float64", TensorDataType::FLOAT64},
		{"int8", TensorDataType::INT8},       {"int16", TensorDataType::INT16},
		{"int32", TensorDataType::INT32},     {"int64", TensorDataType::INT64},
		{"uint8", TensorDataType::UINT8},     {"uint16", TensorDataType::UINT16},
		{"uint32", TensorDataType::UINT32},   {"uint64", TensorDataType::UINT64},
		{"bool", TensorDataType::BOOL}
	};
	auto it = kTypes.find(name);
	if (it == kTypes.end()) return false;
	out = it->second;
	return true;
}

// 把一个脚本数值按张量元素类型追加到字节缓冲
template<typename T>
inline void tensorAppendBytes(std::vector<uint8_t>& bytes, T value) {
	const auto* p = reinterpret_cast<const uint8_t*>(&value);
	bytes.insert(bytes.end(), p, p + sizeof(T));
}

inline void tensorAppendElement(std::vector<uint8_t>& bytes, TensorDataType type, double value) {
	switch (type) {
		case TensorDataType::FLOAT32: tensorAppendBytes(bytes, static_cast<float>(value)); break;
		case TensorDataType::FLOAT64: tensorAppendBytes(bytes, value); break;
		case TensorDataType::INT8:    tensorAppendBytes(bytes, static_cast<int8_t>(value)); break;
		case TensorDataType::INT16:   tensorAppendBytes(bytes, static_cast<int16_t>(value)); break;
		case TensorDataType::INT32:   tensorAppendBytes(bytes, static_cast<int32_t>(value)); break;
		case TensorDataType::INT64:   tensorAppendBytes(bytes, static_cast<int64_t>(value)); break;
		case TensorDataType::UINT8:   tensorAppendBytes(bytes, static_cast<uint8_t>(value)); break;
		case TensorDataType::UINT16:  tensorAppendBytes(bytes, static_cast<uint16_t>(value)); break;
		case TensorDataType::UINT32:  tensorAppendBytes(bytes, static_cast<uint32_t>(value)); break;
		case TensorDataType::UINT64:  tensorAppendBytes(bytes, static_cast<uint64_t>(value)); break;
		case TensorDataType::BOOL:    tensorAppendBytes(bytes, static_cast<uint8_t>(value != 0.0)); break;
		default: break;
	}
}

// 张量字节缓冲中第 byteOffset 字节处的元素 → ScriptValue
inline ScriptValue tensorElementToScriptValue(const TensorData& tensor, size_t byteOffset) {
	const uint8_t* p = tensor.data.data() + byteOffset;
	switch (tensor.dataType) {
		case TensorDataType::FLOAT32: { float v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromFloat(v); }
		case TensorDataType::FLOAT64: { double v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromFloat(v); }
		case TensorDataType::INT8:    { int8_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::INT16:   { int16_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::INT32:   { int32_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::INT64:   { int64_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::UINT8:   { uint8_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::UINT16:  { uint16_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::UINT32:  { uint32_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::UINT64:  { uint64_t v; std::memcpy(&v, p, sizeof(v)); return ScriptValue::fromInt(v); }
		case TensorDataType::BOOL:    return ScriptValue::fromBool(p[0] != 0);
		default: return ScriptValue::null();
	}
}

// ScriptValue 张量描述 → TensorData
// 期望 {data:[number], shape?:[int], dtype?:"float32"}；失败时返回 false 并填 error
inline bool tensorFromScriptValue(const ScriptValue& spec, TensorData& out, std::string& error) {
	if (!spec.isObject()) {
		error = "tensor input must be an object {data, shape?, dtype?}";
		return false;
	}
	const auto* dataVal = spec.get("data");
	if (!dataVal || !dataVal->isArray() || dataVal->arrayVal.empty()) {
		error = "tensor input requires non-empty 'data' array";
		return false;
	}
	TensorDataType type = TensorDataType::FLOAT32;
	if (const auto* dtype = spec.get("dtype")) {
		if (!dtype->isString() || !tensorTypeFromString(dtype->asString(), type)) {
			error = "unknown tensor dtype: " + dtype->asString();
			return false;
		}
	}
	TensorShape shape;
	if (const auto* shapeVal = spec.get("shape")) {
		if (!shapeVal->isArray()) {
			error = "tensor 'shape' must be an int array";
			return false;
		}
		for (const auto& dim : shapeVal->arrayVal) {
			shape.push_back(dim.asInt());
		}
	} else {
		shape.push_back(static_cast<int64_t>(dataVal->arrayVal.size()));
	}
	out = TensorData{};
	out.dataType = type;
	out.shape = std::move(shape);
	for (const auto& v : dataVal->arrayVal) {
		tensorAppendElement(out.data, type, v.asFloat());
	}
	return true;
}

// ModelOutput → ScriptValue {name, shape, data}
inline ScriptValue modelOutputToScriptValue(const ModelOutput& output) {
	const size_t elemSize = tensorElementSize(output.tensor.dataType);
	std::vector<ScriptValue> arr;
	if (elemSize > 0) {
		arr.reserve(output.tensor.data.size() / elemSize);
		for (size_t offset = 0; offset + elemSize <= output.tensor.data.size(); offset += elemSize) {
			arr.push_back(tensorElementToScriptValue(output.tensor, offset));
		}
	}
	return ScriptValue::fromObject({
		{"name", ScriptValue::fromString(output.name)},
		{"shape", tensorShapeToScriptValue(output.tensor.shape)},
		{"data", ScriptValue::fromArray(std::move(arr))}
	});
}

} // namespace modules
} // namespace script
} // namespace wingman
