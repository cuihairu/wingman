#include "screen_module.hpp"
#include "module_helpers.hpp"
#include "wingman/screen.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createScreenModule() {
	ModuleDescriptor mod;
	mod.name = "screen";

	mod.functions.push_back({"capture", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto bitmap = Screen::capture();
		return ScriptValue::fromBool(bitmap != nullptr);
	}, "() -> boolean"});

	mod.functions.push_back({"captureRegion", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Rect region = toRect(args[0]);
		auto bitmap = Screen::capture(region);
		return ScriptValue::fromBool(bitmap != nullptr);
	}, "region:{x,y,width,height} -> boolean"});

	mod.functions.push_back({"getPixel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		Color color = Screen::getPixel(x, y);
		return fromColor(color);
	}, "x:int, y:int -> {r,g,b,a}"});

	mod.functions.push_back({"findColor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Color color = toColor(args[0]);
		Rect region = toRect(args[1]);
		int tolerance = args.size() > 2 ? args[2].asInt(10) : 10;
		Point result;
		if (Screen::findColor(color, region, tolerance, result)) {
			return ScriptValue::fromArray({fromPoint(result), ScriptValue::fromBool(true)});
		}
		return ScriptValue::fromArray({ScriptValue::null(), ScriptValue::fromBool(false)});
	}, "color, region:{x,y,width,height}, tolerance:int -> point, found:bool"});

	mod.functions.push_back({"findColors", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Color color = toColor(args[0]);
		Rect region = toRect(args[1]);
		int tolerance = args.size() > 2 ? args[2].asInt(10) : 10;
		int maxCount = args.size() > 3 ? args[3].asInt(0) : 0;
		auto points = Screen::findColors(color, region, tolerance, maxCount);
		std::vector<ScriptValue> arr;
		for (const auto& p : points) {
			arr.push_back(fromPoint(p));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "color, region:{x,y,width,height}, tolerance:int, maxCount:int -> {point}"});

	mod.functions.push_back({"getScreenWidth", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(Screen::getScreenWidth());
	}, "() -> int"});

	mod.functions.push_back({"getScreenHeight", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(Screen::getScreenHeight());
	}, "() -> int"});

	mod.functions.push_back({"findImage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string imagePath = args[0].asString();
		Rect region = args.size() > 1 ? toRect(args[1]) : Rect(0, 0, Screen::getScreenWidth(), Screen::getScreenHeight());
		double threshold = args.size() > 2 ? args[2].asFloat(0.9) : 0.9;
		Point result;
		if (Screen::findImage(imagePath, region, threshold, result)) {
			return ScriptValue::fromArray({fromPoint(result), ScriptValue::fromBool(true)});
		}
		return ScriptValue::fromArray({ScriptValue::null(), ScriptValue::fromBool(false)});
	}, "imagePath:string, region:{x,y,width,height}, threshold:float -> point, found:bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
