#pragma once

// Conversion utilities between ScriptValue and Wingman core types
// For internal use by module descriptors, not exposed to script engine

#include "wingman/script/iscript_engine.hpp"
#include "wingman/screen.hpp"
#include <string>

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

} // namespace modules
} // namespace script
} // namespace wingman
