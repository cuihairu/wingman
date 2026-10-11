#include "wingman/script/iscript_engine.hpp"
#include "wingman/crypt.hpp"
#include "wingman/vision.hpp"
#include "wingman/vision_ai.hpp"
#include "module_helpers.hpp"

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace wingman {
namespace script {
namespace modules {

namespace {

// 具名模板注册表（进程内全局）：批量管理 + 批量识别的登记面。v1 驻内存
// （进程生命周期，与工作流注册表同口径）；登记时校验模板文件存在，防
// 脚本把坏路径拖到识别期才暴露。识别（截屏+匹配）在锁外逐个执行——
// findImage 是重操作，不得持锁。
class TemplateRegistry {
public:
	bool add(const std::string& name, const std::string& path) {
		if (name.empty() || path.empty()) return false;
		std::error_code ec;
		if (!std::filesystem::exists(path, ec) || ec) return false;
		std::lock_guard<std::mutex> lock(mutex_);
		return templates_.emplace(name, path).second;  // 重名拒绝：显式 remove 后再登记
	}

	bool remove(const std::string& name) {
		std::lock_guard<std::mutex> lock(mutex_);
		return templates_.erase(name) > 0;
	}

	bool has(const std::string& name) const {
		std::lock_guard<std::mutex> lock(mutex_);
		return templates_.count(name) > 0;
	}

	// 登记快照（name → path，名序确定）；names 为空 = 全部，未知名跳过
	std::vector<std::pair<std::string, std::string>> resolve(
		const std::vector<std::string>& names) const {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<std::pair<std::string, std::string>> snapshot;
		if (names.empty()) {
			snapshot.assign(templates_.begin(), templates_.end());
		} else {
			for (const auto& n : names) {
				auto it = templates_.find(n);
				if (it != templates_.end()) snapshot.emplace_back(it->first, it->second);
			}
		}
		return snapshot;
	}

private:
	std::map<std::string, std::string> templates_;  // 有序容器：list/批量天然名序确定
	mutable std::mutex mutex_;
};

TemplateRegistry g_templateRegistry;

// ImageMatch → 脚本对象（findImage 与批量识别共用同一形状；name 空串 =
// 单模板调用，不带 name 键——findImage 既有契约不增字段）
ScriptValue matchToScriptValue(const std::string& name, const ImageMatch& result) {
	if (result.found) {
		std::unordered_map<std::string, ScriptValue> obj;
		if (!name.empty()) obj["name"] = ScriptValue::fromString(name);
		obj["found"] = ScriptValue::fromBool(true);
		obj["position"] = fromPoint(result.position);
		obj["confidence"] = ScriptValue::fromFloat(result.confidence);
		obj["region"] = fromRect(result.region);
		return ScriptValue::fromObject(std::move(obj));
	}
	std::unordered_map<std::string, ScriptValue> obj;
	if (!name.empty()) obj["name"] = ScriptValue::fromString(name);
	obj["found"] = ScriptValue::fromBool(false);
	return ScriptValue::fromObject(std::move(obj));
}

} // namespace

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
		return matchToScriptValue("", result);
	}, "templatePath:string, threshold:float?, region? -> {found,position?,confidence?}"});

	// ===== 图像模板批量管理 =====

	// templateAdd(name, path) -> bool：登记具名模板（路径须存在；重名拒绝）
	mod.functions.push_back({"templateAdd", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_templateRegistry.add(args[0].asString(), args[1].asString()));
	}, "name:string, path:string -> bool"});

	mod.functions.push_back({"templateRemove", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_templateRegistry.remove(args[0].asString()));
	}, "name:string -> bool"});

	mod.functions.push_back({"templateHas", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		return ScriptValue::fromBool(g_templateRegistry.has(args[0].asString()));
	}, "name:string -> bool"});

	// templateList() -> [{name, path}]：全部登记，名序确定
	mod.functions.push_back({"templateList", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto snapshot = g_templateRegistry.resolve({});
		std::vector<ScriptValue> arr;
		arr.reserve(snapshot.size());
		for (auto& [name, path] : snapshot) {
			arr.push_back(ScriptValue::fromObject({
				{"name", ScriptValue::fromString(name)},
				{"path", ScriptValue::fromString(path)}
			}));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "-> [{name,path}]"});

	// templateFindAll(names?, threshold?, region?) -> [{name,found,position?,confidence?,region?}]
	// 批量识别：names 空/缺省 = 全部登记模板，未知名跳过；输出名序确定。
	// 匹配逐个在锁外执行，单模板失败（截屏异常等）按未命中落账不中断整批。
	mod.functions.push_back({"templateFindAll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::vector<std::string> names;
		if (!args.empty() && args[0].isArray()) {
			for (size_t i = 0; i < args[0].size(); ++i) {
				names.push_back(args[0].at(i).asString());
			}
		}
		double threshold = args.size() > 1 ? args[1].asFloat(0.9) : 0.9;
		auto snapshot = g_templateRegistry.resolve(names);
		std::vector<ScriptValue> arr;
		arr.reserve(snapshot.size());
		for (auto& [name, path] : snapshot) {
			ImageMatch result;
			try {
				if (args.size() > 2 && !args[2].isNull()) {
					result = Vision::findImage(path, toRect(args[2]), threshold);
				} else {
					result = Vision::findImage(path, threshold);
				}
			} catch (...) {
				result = ImageMatch{};  // 未命中形状落账，不废整批
			}
			arr.push_back(matchToScriptValue(name, result));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "names:array?, threshold:float?, region? -> [{name,found,position?,confidence?,region?}]"});

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
			{"localMode", ScriptValue::fromBool(VisionAi::isLocalMode())},
			{"baseUrl", ScriptValue::fromString(VisionAi::config().baseUrl)},
			{"model", ScriptValue::fromString(VisionAi::config().model)},
			{"hasKey", ScriptValue::fromBool(VisionAi::hasApiKey())},
			{"lastError", ScriptValue::fromString(VisionAi::lastError())},
		});
	}, "() -> {configured, localMode, baseUrl, model, hasKey, lastError}"});

	// 本地 ONNX 检测 provider（WINGMAN_ENABLE_ML 构建；未构建恒 false）
	mod.functions.push_back({"aiSetupLocal", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args[0].isObject()) {
			return ScriptValue::fromBool(false);
		}
		const ScriptValue& v = args[0];
		VisionAiLocalConfig cfg;
		if (auto* p = v.get("modelPath")) cfg.modelPath = p->asString();
		if (auto* labels = v.get("labels")) {
			if (labels->isArray()) {
				for (size_t i = 0; i < labels->size(); ++i) {
					cfg.labels.push_back(labels->at(i).asString());
				}
			}
		}
		if (auto* p = v.get("minConfidence")) {
			cfg.minConfidence = static_cast<float>(p->asFloat(0.5));
		}
		return ScriptValue::fromBool(VisionAi::setupLocal(cfg));
	}, "cfg:{modelPath:string, labels:[string]?, minConfidence:float?} -> bool（本地 ONNX 检测模型，需 ML 构建）"});

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

	mod.functions.push_back({"aiElements", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string desc = args[0].asString();
		Rect region = args.size() > 1 && !args[1].isNull() ? toRect(args[1]) : Rect();
		std::vector<VisionAiElement> elems = VisionAi::elements(desc, region);
		if (elems.empty() && !VisionAi::lastError().empty()) {
			// 失败与「屏幕无匹配」在返回序列上同形，靠 lastError 区分
			return ScriptValue::fromObject({
				{"found", ScriptValue::fromBool(false)},
				{"error", ScriptValue::fromString(VisionAi::lastError())},
			});
		}
		std::vector<ScriptValue> arr;
		arr.reserve(elems.size());
		for (const auto& e : elems) {
			arr.push_back(ScriptValue::fromObject({
				{"label", ScriptValue::fromString(e.label)},
				{"x", ScriptValue::fromInt(e.x)},
				{"y", ScriptValue::fromInt(e.y)},
				{"w", ScriptValue::fromInt(e.w)},
				{"h", ScriptValue::fromInt(e.h)},
				{"confidence", ScriptValue::fromFloat(e.confidence)},
			}));
		}
		return ScriptValue::fromObject({
			{"found", ScriptValue::fromBool(!arr.empty())},
			{"elements", ScriptValue::fromArray(std::move(arr))},
		});
	}, "desc:string, region? -> {found, elements:[{label,x,y,w,h,confidence}]} | {found:false,error} (desc 空=列出全部可交互元素)"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
