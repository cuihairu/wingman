#include "wingman/script/iscript_engine.hpp"
#include "wingman/system_watcher.hpp"
#include "wingman/event.hpp"
#include <spdlog/spdlog.h>

namespace wingman {
namespace script {
namespace modules {

// systemwatch 模块（development-todo 缺口清单「进程/窗口/文件变化统一事件源」
// 的进程/窗口两腿；文件变化由 filewatcher 模块提供）。
// 回调从 SystemWatcher 后台轮询线程触发，非线程安全 callable（如 Lua）跨线程
// 调用会崩溃，故用 callableThreadSafe 门控（同 hotkey 模块）。
// 命中事件同时经 EventHub 以 "systemwatch.*" 广播（source "systemwatch"），
// 供原生层订阅桥接；注意该事件从后台线程同步分发，脚本侧订阅同受
// callableThreadSafe 约束。
ModuleDescriptor createSystemWatchModule() {
	ModuleDescriptor mod;
	mod.name = "systemwatch";

	// processWatch(name?, callback) -> id（0 = 失败；name 空/缺省 = 全部进程）
	mod.functions.push_back({"processWatch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args.back().isCallable()) {
			return ScriptValue::fromInt(0);
		}
		const std::string name =
			(!args[0].isCallable() && args[0].isString()) ? args[0].asString() : "";
		const auto& callable = args.back();
		if (!callable.callableThreadSafe) {
			EventHub::instance().emit("systemwatch.error", {
				{"error", "systemwatch 回调从后台轮询线程触发，需要线程安全的 callable（如 Python 函数）。Lua callable 非线程安全，请改用 Python 注册观察。"}
			}, "systemwatch");
			return ScriptValue::fromInt(0);
		}
		auto callback = callable.callableVal;  // 拷贝；闭包持有
		const uint64_t id = SystemWatcher::defaultInstance().watchProcess(
			name, [callback](const std::string& action, int64_t pid, const std::string& pname) {
				std::unordered_map<std::string, ScriptValue> payload = {
					{"action", ScriptValue::fromString(action)},
					{"pid", ScriptValue::fromInt(pid)},
					{"name", ScriptValue::fromString(pname)},
				};
				EventHub::instance().emit(
					"systemwatch.process",
					{
						{"action", action},
						{"pid", pid},
						{"name", pname},
					},
					"systemwatch");
				try {
					callback({ScriptValue::fromObject(std::move(payload))});
				} catch (const std::exception& e) {
					spdlog::error("[systemwatch] script callback exception: {}", e.what());
				} catch (...) {
					spdlog::error("[systemwatch] script callback unknown exception");
				}
			});
		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "name?:string, callback:function -> id:int"});

	// windowWatch(title?, callback) -> id（0 = 失败；title 空/缺省 = 全部窗口，子串匹配）
	mod.functions.push_back({"windowWatch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty() || !args.back().isCallable()) {
			return ScriptValue::fromInt(0);
		}
		const std::string title =
			(!args[0].isCallable() && args[0].isString()) ? args[0].asString() : "";
		const auto& callable = args.back();
		if (!callable.callableThreadSafe) {
			EventHub::instance().emit("systemwatch.error", {
				{"error", "systemwatch 回调从后台轮询线程触发，需要线程安全的 callable（如 Python 函数）。Lua callable 非线程安全，请改用 Python 注册观察。"}
			}, "systemwatch");
			return ScriptValue::fromInt(0);
		}
		auto callback = callable.callableVal;  // 拷贝；闭包持有
		const uint64_t id = SystemWatcher::defaultInstance().watchWindow(
			title, [callback](const std::string& action, uint64_t handle, const std::string& wtitle) {
				std::unordered_map<std::string, ScriptValue> payload = {
					{"action", ScriptValue::fromString(action)},
					{"handle", ScriptValue::fromInt(static_cast<int64_t>(handle))},
					{"title", ScriptValue::fromString(wtitle)},
				};
				EventHub::instance().emit(
					"systemwatch.window",
					{
						{"action", action},
						{"handle", handle},
						{"title", wtitle},
					},
					"systemwatch");
				try {
					callback({ScriptValue::fromObject(std::move(payload))});
				} catch (const std::exception& e) {
					spdlog::error("[systemwatch] script callback exception: {}", e.what());
				} catch (...) {
					spdlog::error("[systemwatch] script callback unknown exception");
				}
			});
		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "title?:string, callback:function -> id:int"});

	// unwatch(id) -> boolean
	mod.functions.push_back({"unwatch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isInt()) {
			return ScriptValue::fromBool(false);
		}
		return ScriptValue::fromBool(
			SystemWatcher::defaultInstance().unwatch(static_cast<uint64_t>(args[0].asInt())));
	}, "id:int -> boolean"});

	// clearAll() -> nil
	mod.functions.push_back({"clearAll", [](const std::vector<ScriptValue>&) -> ScriptValue {
		SystemWatcher::defaultInstance().unwatchAll();
		return ScriptValue::null();
	}, "() -> nil"});

	// watchCount() -> int（全部注册数；watchCount("process")/watchCount("window") 按类别）
	mod.functions.push_back({"watchCount", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const auto& watcher = SystemWatcher::defaultInstance();
		if (args.size() >= 1 && args[0].isString()) {
			const auto& kind = args[0].asString();
			if (kind == "process") {
				return ScriptValue::fromInt(static_cast<int64_t>(watcher.count(SystemWatchKind::Process)));
			}
			if (kind == "window") {
				return ScriptValue::fromInt(static_cast<int64_t>(watcher.count(SystemWatchKind::Window)));
			}
			return ScriptValue::fromInt(0);
		}
		return ScriptValue::fromInt(static_cast<int64_t>(watcher.count()));
	}, "kind?:string -> int"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
