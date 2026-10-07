#include "wingman/script/iscript_engine.hpp"
#include "wingman/crypt.hpp"
#include "wingman/vision.hpp"
#include "wingman/vision_ai.hpp"
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
			// With search region
			Rect region = toRect(args[2]);
			result = Vision::findImage(templatePath, region, threshold);
		} else {
			// Full-screen search
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

	// ===== AI 视觉识别（OpenAI 兼容 provider，WINGMAN_ENABLE_VISION 构建）=====
	mod.functions.push_back({"aiSetup", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isObject()) {
			return ScriptValue::fromBool(false);
		}
		const ScriptValue& v = args[0];
		VisionAiConfig cfg;
		if (auto* p = v.get("baseUrl")) cfg.baseUrl = p->asString();
		if (auto* p = v.get("model")) cfg.model = p->asString();
		if (auto* p = v.get("timeoutSeconds")) cfg.timeoutSeconds = static_cast<int>(p->asInt(60));
		if (auto* p = v.get("apiKey")) cfg.apiKey = p->asString();
		// 凭据加密面（保险箱同型：AES-256-GCM 密文落配置，口令解密后仅驻内存）：
		// apiKeyEnc = crypto.encryptAES(key, passphrase) 的产物，二者成对传入。
		if (auto* enc = v.get("apiKeyEnc")) {
			auto* pass = v.get("passphrase");
			if (!pass) return ScriptValue::fromBool(false);
			cfg.apiKey = crypt::decryptAES(enc->asString(), pass->asString());
			if (cfg.apiKey.empty()) return ScriptValue::fromBool(false);
		}
		if (cfg.baseUrl.empty() || cfg.model.empty()) {
			return ScriptValue::fromBool(false);
		}
		VisionAi::setup(cfg);
		return ScriptValue::fromBool(true);
	}, "cfg:{baseUrl:string, model:string, apiKey:string?|apiKeyEnc:string?+passphrase:string?, timeoutSeconds:int?} -> bool"});

	mod.functions.push_back({"aiSetupStatus", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject({
			{"configured", ScriptValue::fromBool(VisionAi::isConfigured())},
			{"baseUrl", ScriptValue::fromString(VisionAi::config().baseUrl)},
			{"model", ScriptValue::fromString(VisionAi::config().model)},
			{"hasKey", ScriptValue::fromBool(VisionAi::hasApiKey())},
			{"lastError", ScriptValue::fromString(VisionAi::lastError())},
		});
	}, "() -> {configured, baseUrl, model, hasKey, lastError}"});

	mod.functions.push_back({"aiLocate", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string desc = args[0].asString();
		Rect region = args.size() > 1 && !args[1].isNull() ? toRect(args[1]) : Rect();
		VisionAiBox box = VisionAi::locate(desc, region);
		if (box.found) {
			return ScriptValue::fromObject({
				{"found", ScriptValue::fromBool(true)},
				{"x", ScriptValue::fromInt(box.x)},
				{"y", ScriptValue::fromInt(box.y)},
				{"w", ScriptValue::fromInt(box.w)},
				{"h", ScriptValue::fromInt(box.h)},
				{"confidence", ScriptValue::fromFloat(box.confidence)},
				{"label", ScriptValue::fromString(box.label)},
			});
		}
		return ScriptValue::fromObject({
			{"found", ScriptValue::fromBool(false)},
			{"error", ScriptValue::fromString(VisionAi::lastError())},
		});
	}, "desc:string, region? -> {found,x,y,w,h,confidence,label} | {found:false,error}"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
