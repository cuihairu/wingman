#include "wingman/script/iscript_engine.hpp"
#include "wingman/hotkey.hpp"
#include "wingman/event.hpp"
#include <spdlog/spdlog.h>

namespace wingman {
namespace script {
namespace modules {

// hotkey 模块（todo 2026-10-04 P1-5：全局热键监听）。
// 轮询式全局键态监听经 HotkeyManager（wingman/hotkey.hpp）。回调从
// manager 后台线程触发，非线程安全 callable（如 Lua）跨线程调用会崩溃，
// 故用 callableThreadSafe 门控（仿 misc_modules on_property_changed）。
ModuleDescriptor createHotkeyModule() {
	ModuleDescriptor mod;
	mod.name = "hotkey";

	// register(combo, callback) -> id（0 = 失败）
	mod.functions.push_back({"register", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2 || !args[0].isString() || !args[1].isCallable()) {
			return ScriptValue::fromInt(0);
		}
		// 回调从后台轮询线程触发，拒绝非线程安全 callable（如 Lua）
		if (!args[1].callableThreadSafe) {
			EventHub::instance().emit("hotkey.error", {
				{"error", "热键回调从后台线程触发，需要线程安全的 callable（如 Python 函数）。Lua callable 非线程安全，请改用 Python 注册热键。"}
			}, "hotkey");
			return ScriptValue::fromInt(0);
		}
		auto callback = args[1].callableVal;  // 拷贝；闭包持有
		const uint64_t id = HotkeyManager::defaultInstance().registerHotkey(
			args[0].asString(), [callback]() {
				try {
					callback({});
				} catch (const std::exception& e) {
					spdlog::error("[hotkey] script callback exception: {}", e.what());
				} catch (...) {
					spdlog::error("[hotkey] script callback unknown exception");
				}
			});
		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "combo:string, callback:function -> id:int"});

	// unregister(id) -> boolean
	mod.functions.push_back({"unregister", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 1 || !args[0].isInt()) {
			return ScriptValue::fromBool(false);
		}
		return ScriptValue::fromBool(
			HotkeyManager::defaultInstance().unregister(static_cast<uint64_t>(args[0].asInt())));
	}, "id:int -> boolean"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
