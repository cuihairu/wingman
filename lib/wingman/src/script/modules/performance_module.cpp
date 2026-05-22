#include "wingman/script/iscript_engine.hpp"
#include "module_helpers.hpp"

// performance.hpp 依赖 OpenCV，当 OpenCV 不可用时提供 stub 模块
#ifdef HAS_OPENCV
#include "wingman/performance.hpp"
#endif

namespace wingman {
namespace script {
namespace modules {

#ifdef HAS_OPENCV

ModuleDescriptor createPerformanceModule() {
	ModuleDescriptor mod;
	mod.name = "perf";

	mod.functions.push_back({"setConfig", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		PerformanceConfig config;
		if (args[0].isObject()) {
			auto* v = args[0].get("enableImageCache");
			if (v) config.enableImageCache = v->asBool();
			v = args[0].get("maxCacheSize");
			if (v) config.maxCacheSize = static_cast<size_t>(v->asInt());
			v = args[0].get("enableParallelProcessing");
			if (v) config.enableParallelProcessing = v->asBool();
			v = args[0].get("numThreads");
			if (v) config.numThreads = static_cast<int>(v->asInt());
		}
		PerformanceManager::instance().setConfig(config);
		return ScriptValue::null();
	}, "config:{enableImageCache,...} -> nil"});

	mod.functions.push_back({"getConfig", [](const std::vector<ScriptValue>&) -> ScriptValue {
		const auto& config = PerformanceManager::instance().getConfig();
		return ScriptValue::fromObject({
			{"enableImageCache", ScriptValue::fromBool(config.enableImageCache)},
			{"maxCacheSize", ScriptValue::fromInt(static_cast<int64_t>(config.maxCacheSize))},
			{"enableParallelProcessing", ScriptValue::fromBool(config.enableParallelProcessing)},
			{"numThreads", ScriptValue::fromInt(config.numThreads)}
		});
	}, "() -> {enableImageCache,...}"});

	mod.functions.push_back({"preloadImage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		PerformanceManager::instance().preloadImage(args[0].asString());
		return ScriptValue::null();
	}, "path:string -> nil"});

	mod.functions.push_back({"clearCache", [](const std::vector<ScriptValue>&) -> ScriptValue {
		PerformanceManager::instance().clearCache();
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"getCacheSize", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(static_cast<int64_t>(PerformanceManager::instance().getCacheSize()));
	}, "() -> int"});

	mod.functions.push_back({"getCacheStats", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& mgr = PerformanceManager::instance();
		size_t total = mgr.getCacheHits() + mgr.getCacheMisses();
		double hitRate = total > 0 ? (double)mgr.getCacheHits() / total * 100 : 0.0;
		return ScriptValue::fromObject({
			{"size", ScriptValue::fromInt(static_cast<int64_t>(mgr.getCacheSize()))},
			{"hits", ScriptValue::fromInt(static_cast<int64_t>(mgr.getCacheHits()))},
			{"misses", ScriptValue::fromInt(static_cast<int64_t>(mgr.getCacheMisses()))},
			{"hitRate", ScriptValue::fromFloat(hitRate)}
		});
	}, "() -> {size,hits,misses,hitRate}"});

	mod.functions.push_back({"fastFindImage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string imagePath = args[0].asString();
		int x = static_cast<int>(args[1].asInt()), y = static_cast<int>(args[2].asInt());
		int w = static_cast<int>(args[3].asInt()), h = static_cast<int>(args[4].asInt());
		double threshold = args[5].asFloat(0.9);
		Point result;
		if (PerformanceManager::instance().fastFindImage(imagePath, Rect(x, y, w, h), threshold, result)) {
			return fromPoint(result);
		}
		return ScriptValue::null();
	}, "path:string, x:int, y:int, w:int, h:int, threshold:float -> {x,y}?"});

	mod.functions.push_back({"parallelFindColors", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		uint32_t color = static_cast<uint32_t>(args[0].asInt());
		int x = static_cast<int>(args[1].asInt()), y = static_cast<int>(args[2].asInt());
		int w = static_cast<int>(args[3].asInt()), h = static_cast<int>(args[4].asInt());
		int tolerance = static_cast<int>(args[5].asInt());
		int maxCount = args.size() > 6 ? static_cast<int>(args[6].asInt(0)) : 0;
		auto points = PerformanceManager::instance().parallelFindColors(
			Color::fromRGB(color), Rect(x, y, w, h), tolerance, maxCount);
		std::vector<ScriptValue> arr;
		for (const auto& p : points) arr.push_back(fromPoint(p));
		return ScriptValue::fromArray(std::move(arr));
	}, "color:int, x:int, y:int, w:int, h:int, tolerance:int, maxCount:int? -> {{x,y}}"});

	mod.functions.push_back({"getStats", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto stats = PerformanceManager::instance().getStats();
		return ScriptValue::fromObject({
			{"totalCaptures", ScriptValue::fromInt(static_cast<int64_t>(stats.totalCaptures))},
			{"totalColorSearches", ScriptValue::fromInt(static_cast<int64_t>(stats.totalColorSearches))},
			{"totalImageSearches", ScriptValue::fromInt(static_cast<int64_t>(stats.totalImageSearches))},
			{"avgCaptureTime", ScriptValue::fromFloat(stats.avgCaptureTime)},
			{"avgColorSearchTime", ScriptValue::fromFloat(stats.avgColorSearchTime)},
			{"avgImageSearchTime", ScriptValue::fromFloat(stats.avgImageSearchTime)}
		});
	}, "() -> {totalCaptures,...}"});

	mod.functions.push_back({"resetStats", [](const std::vector<ScriptValue>&) -> ScriptValue {
		PerformanceManager::instance().resetStats();
		return ScriptValue::null();
	}, "() -> nil"});

	return mod;
}

#else // HAS_OPENCV not defined - provide stub module

ModuleDescriptor createPerformanceModule() {
	ModuleDescriptor mod;
	mod.name = "perf";

	mod.functions.push_back({"setConfig", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "config:{...} -> nil"});

	mod.functions.push_back({"getConfig", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject({
			{"enableImageCache", ScriptValue::fromBool(false)},
			{"maxCacheSize", ScriptValue::fromInt(0)},
			{"enableParallelProcessing", ScriptValue::fromBool(false)},
			{"numThreads", ScriptValue::fromInt(0)}
		});
	}, "() -> {enableImageCache,...}"});

	mod.functions.push_back({"preloadImage", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "path:string -> nil"});

	mod.functions.push_back({"clearCache", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"getCacheSize", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromInt(0);
	}, "() -> int"});

	mod.functions.push_back({"getCacheStats", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject({
			{"size", ScriptValue::fromInt(0)},
			{"hits", ScriptValue::fromInt(0)},
			{"misses", ScriptValue::fromInt(0)},
			{"hitRate", ScriptValue::fromFloat(0.0)}
		});
	}, "() -> {size,hits,misses,hitRate}"});

	mod.functions.push_back({"fastFindImage", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "path:string, x:int, y:int, w:int, h:int, threshold:float -> {x,y}?"});

	mod.functions.push_back({"parallelFindColors", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromArray({});
	}, "color:int, x:int, y:int, w:int, h:int, tolerance:int, maxCount:int? -> {{x,y}}"});

	mod.functions.push_back({"getStats", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromObject({
			{"totalCaptures", ScriptValue::fromInt(0)},
			{"totalColorSearches", ScriptValue::fromInt(0)},
			{"totalImageSearches", ScriptValue::fromInt(0)},
			{"avgCaptureTime", ScriptValue::fromFloat(0.0)},
			{"avgColorSearchTime", ScriptValue::fromFloat(0.0)},
			{"avgImageSearchTime", ScriptValue::fromFloat(0.0)}
		});
	}, "() -> {totalCaptures,...}"});

	mod.functions.push_back({"resetStats", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::null();
	}, "() -> nil"});

	return mod;
}

#endif // HAS_OPENCV

} // namespace modules
} // namespace script
} // namespace wingman
