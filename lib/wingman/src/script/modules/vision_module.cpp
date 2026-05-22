#include "wingman/script/iscript_engine.hpp"
#include "wingman/vision.hpp"
#include "module_helpers.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createVisionModule() {
	ModuleDescriptor mod;
	mod.name = "vision";

	mod.functions.push_back({"findColor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Color color = toColor(args[0]);
		int tolerance = args.size() > 1 ? static_cast<int>(args[1].asInt(10)) : 10;
		Rect region = args.size() > 2 ? toRect(args[2]) : Rect(0, 0, Screen::getScreenWidth(), Screen::getScreenHeight());
		auto point = Vision::findColor(color, tolerance, region);
		if (point) return fromPoint(*point);
		return ScriptValue::null();
	}, "color, tolerance:int?, region? -> {x,y}?"});

	mod.functions.push_back({"findAllColors", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Color color = toColor(args[0]);
		int tolerance = args.size() > 1 ? static_cast<int>(args[1].asInt(10)) : 10;
		Rect region = args.size() > 2 ? toRect(args[2]) : Rect(0, 0, Screen::getScreenWidth(), Screen::getScreenHeight());
		auto points = Vision::findAllColors(color, tolerance, region);
		std::vector<ScriptValue> arr;
		for (const auto& p : points) arr.push_back(fromPoint(p));
		return ScriptValue::fromArray(std::move(arr));
	}, "color, tolerance:int?, region? -> {{x,y}}"});

	mod.functions.push_back({"hasColor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Color color = toColor(args[0]);
		int tolerance = args.size() > 1 ? static_cast<int>(args[1].asInt(10)) : 10;
		Rect region = args.size() > 2 ? toRect(args[2]) : Rect(0, 0, Screen::getScreenWidth(), Screen::getScreenHeight());
		return ScriptValue::fromBool(Vision::hasColor(color, tolerance, region));
	}, "color, tolerance:int?, region? -> bool"});

	mod.functions.push_back({"getDominantColor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Rect region = args.size() > 0 ? toRect(args[0]) : Rect(0, 0, Screen::getScreenWidth(), Screen::getScreenHeight());
		auto color = Vision::getDominantColor(region);
		return fromColor(color);
	}, "region? -> {r,g,b,a}"});

	mod.functions.push_back({"findImage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string templatePath = args[0].asString();
		double threshold = args.size() > 1 ? args[1].asFloat(0.9) : 0.9;

		ImageMatch result;
		if (args.size() > 2 && !args[2].isNull()) {
			// 带搜索区域
			Rect region = toRect(args[2]);
			result = Vision::findImage(templatePath, region, threshold);
		} else {
			// 全屏搜索
			result = Vision::findImage(templatePath, threshold);
		}

		if (result.found) {
			return ScriptValue::fromObject({
				{"found", ScriptValue::fromBool(true)},
				{"position", fromPoint(result.position)},
				{"confidence", ScriptValue::fromFloat(result.confidence)},
				{"region", fromRect(result.region)}
			});
		}
		return ScriptValue::fromObject({{"found", ScriptValue::fromBool(false)}});
	}, "templatePath:string, threshold:float?, region? -> {found,position?,confidence?}"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
