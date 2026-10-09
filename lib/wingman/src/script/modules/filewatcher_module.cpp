#include "filewatcher_module.hpp"
#include "wingman/filewatcher.hpp"
#include "wingman/platform/ifilewatcher.hpp"
#include "wingman/event.hpp"
#include <spdlog/spdlog.h>

#include <mutex>
#include <unordered_map>

namespace wingman {
namespace script {
namespace modules {

namespace {

// 模块级簿记：原生 IFileWatcher 只提供按 id 注销与计数，脚本面的
// isWatching/getWatchedPaths 按路径查询语义由本表支撑；重复注册同路径
// 先注销旧观察再挂新（不泄漏原生计数）。
struct WatchEntry {
	uint64_t id = 0;
	bool recursive = true;
};
std::mutex g_watchMutex;
std::unordered_map<std::string, WatchEntry> g_watches;

std::string changeTypeName(platform::FileChangeType type) {
	switch (type) {
	case platform::FileChangeType::Added: return "added";
	case platform::FileChangeType::Removed: return "removed";
	case platform::FileChangeType::Modified: return "modified";
	case platform::FileChangeType::RenamedOld: return "renamed_old";
	case platform::FileChangeType::RenamedNew: return "renamed_new";
	}
	return "modified";
}

} // namespace

// filewatcher 模块（脚本桥接接真版）：原生 FileWatcher（inotify/FSEvents/
// Win32 平台后端）直连。回调从平台后端线程触发，非线程安全 callable
// （如 Lua）跨线程调用会崩溃，用 callableThreadSafe 门控（同 hotkey/
// systemwatch）。命中变化同时以 "filewatcher.changed" 进 EventHub
// （source "filewatcher"，同受线程安全约束）。
ModuleDescriptor createFileWatcherModule() {
	ModuleDescriptor mod;
	mod.name = "filewatcher";

	// watch(path, callback, recursive?) -> id（0 = 失败；重复注册同路径替换旧观察）
	mod.functions.push_back({"watch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isCallable()) {
			return ScriptValue::fromInt(0);
		}
		if (!args[1].callableThreadSafe) {
			EventHub::instance().emit("filewatcher.error", {
				{"error", "文件监听回调从平台后端线程触发，需要线程安全的 callable（如 Python 函数）。Lua callable 非线程安全，请改用 Python 注册监听。"}
			}, "filewatcher");
			return ScriptValue::fromInt(0);
		}
		const std::string path = args[0].asString();
		const bool recursive = args.size() < 3 || !args[2].isBool() || args[2].asBool();
		auto callback = args[1].callableVal;  // 拷贝；闭包持有
		const uint64_t id = FileWatcher::watch(path,
			[callback](const platform::FileChange& change) {
				const std::string type = changeTypeName(change.type);
				std::unordered_map<std::string, ScriptValue> payload = {
					{"type", ScriptValue::fromString(type)},
					{"path", ScriptValue::fromString(change.path)},
					{"oldPath", ScriptValue::fromString(change.oldPath)},
					{"timestamp", ScriptValue::fromInt(static_cast<int64_t>(change.timestamp))},
				};
				EventHub::instance().emit(
					"filewatcher.changed",
					{
						{"type", type},
						{"path", change.path},
						{"oldPath", change.oldPath},
						{"timestamp", change.timestamp},
					},
					"filewatcher");
				try {
					callback({ScriptValue::fromObject(std::move(payload))});
				} catch (const std::exception& e) {
					spdlog::error("[filewatcher] script callback exception: {}", e.what());
				} catch (...) {
					spdlog::error("[filewatcher] script callback unknown exception");
				}
			}, recursive);
		if (id == 0) {
			return ScriptValue::fromInt(0);
		}
		{
			std::lock_guard<std::mutex> lock(g_watchMutex);
			auto it = g_watches.find(path);
			if (it != g_watches.end()) {
				const uint64_t oldId = it->second.id;
				if (oldId != id) {
					FileWatcher::unwatch(oldId); // 替换注册，不留旧计数
				}
			}
			g_watches[path] = WatchEntry{id, recursive};
		}
		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "path:string, callback:function, recursive?:boolean -> id:int"});

	// unwatch(path) -> boolean（注销该路径的全部观察）
	mod.functions.push_back({"unwatch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		const std::string path = args[0].asString();
		{
			std::lock_guard<std::mutex> lock(g_watchMutex);
			g_watches.erase(path);
		}
		return ScriptValue::fromBool(FileWatcher::unwatchPath(path) > 0);
	}, "path:string -> boolean"});

	// unwatchAll() -> nil
	mod.functions.push_back({"unwatchAll", [](const std::vector<ScriptValue>&) -> ScriptValue {
		std::lock_guard<std::mutex> lock(g_watchMutex);
		for (const auto& [path, entry] : g_watches) {
			FileWatcher::unwatch(entry.id);
		}
		g_watches.clear();
		return ScriptValue::null();
	}, "() -> nil"});

	// isWatching(path) -> boolean
	mod.functions.push_back({"isWatching", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		std::lock_guard<std::mutex> lock(g_watchMutex);
		return ScriptValue::fromBool(g_watches.count(args[0].asString()) > 0);
	}, "path:string -> boolean"});

	// getWatchedPaths() -> string[]
	mod.functions.push_back({"getWatchedPaths", [](const std::vector<ScriptValue>&) -> ScriptValue {
		std::vector<ScriptValue> arr;
		{
			std::lock_guard<std::mutex> lock(g_watchMutex);
			arr.reserve(g_watches.size());
			for (const auto& [path, entry] : g_watches) {
				arr.push_back(ScriptValue::fromString(path));
			}
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> string[]"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
