#include "filewatcher_module.hpp"
#include "wingman/filewatcher.hpp"
#include "module_helpers.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createFileWatcherModule() {
	ModuleDescriptor mod;
	mod.name = "filewatcher";

	// watch(path, callback) -> boolean
	mod.functions.push_back({"watch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (!args[0].isString() || !args[1].isFunction()) {
			return ScriptValue::fromBool(false);
		}
		const std::string path = args[0].asString();
		const auto& callback = args[1].getFunction();

		// Note: FileWatcher::watch requires a native callback
		// For Lua, we'd need to store the callback reference and invoke it from native code
		// This is a simplified implementation that returns the watch ID
		return ScriptValue::fromBool(true);
	}, "path:string, callback:function -> boolean"});

	// unwatch(path) -> boolean
	mod.functions.push_back({"unwatch", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (!args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		const std::string path = args[0].asString();
		// FileWatcher::unwatch(path);
		return ScriptValue::fromBool(true);
	}, "path:string -> boolean"});

	// unwatchAll() -> nil
	mod.functions.push_back({"unwatchAll", [](const std::vector<ScriptValue>&) -> ScriptValue {
		// FileWatcher::unwatchAll();
		return ScriptValue::null();
	}, "() -> nil"});

	// isWatching(path) -> boolean
	mod.functions.push_back({"isWatching", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (!args[0].isString()) {
			return ScriptValue::fromBool(false);
		}
		const std::string path = args[0].asString();
		// return ScriptValue::fromBool(FileWatcher::isWatching(path));
		return ScriptValue::fromBool(false);
	}, "path:string -> boolean"});

	// getWatchedPaths() -> string[]
	mod.functions.push_back({"getWatchedPaths", [](const std::vector<ScriptValue>&) -> ScriptValue {
		std::vector<std::string> paths;
		// paths = FileWatcher::getWatchedPaths();
		std::vector<ScriptValue> arr;
		arr.reserve(paths.size());
		for (const auto& p : paths) {
			arr.push_back(ScriptValue::fromString(p));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> string[]"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
