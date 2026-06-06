#include "wingman/script/iscript_engine.hpp"
#include "wingman/human.hpp"
#include "module_helpers.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createHumanModule() {
	ModuleDescriptor mod;
	mod.name = "human";

	// Mouse sub-module functions
	mod.functions.push_back({"mouse_move", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args[0].asInt()), y = static_cast<int>(args[1].asInt());
		int duration = args.size() > 2 ? static_cast<int>(args[2].asInt(0)) : 0;
		Human::mouse().moveTo(x, y, duration);
		return ScriptValue::null();
	}, "x:int, y:int, duration:int? -> nil"});

	mod.functions.push_back({"mouse_click", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args[0].asInt()), y = static_cast<int>(args[1].asInt());
		Human::mouse().click(x, y);
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_rightClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args[0].asInt()), y = static_cast<int>(args[1].asInt());
		Human::mouse().rightClick(x, y);
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_doubleClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args[0].asInt()), y = static_cast<int>(args[1].asInt());
		Human::mouse().doubleClick(x, y);
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_drag", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int fromX = static_cast<int>(args[0].asInt()), fromY = static_cast<int>(args[1].asInt());
		int toX = static_cast<int>(args[2].asInt()), toY = static_cast<int>(args[3].asInt());
		Human::mouse().drag(fromX, fromY, toX, toY);
		return ScriptValue::null();
	}, "fromX:int, fromY:int, toX:int, toY:int -> nil"});

	mod.functions.push_back({"mouse_scroll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args[0].asInt()), y = static_cast<int>(args[1].asInt());
		int delta = args.size() > 2 ? static_cast<int>(args[2].asInt(-3)) : -3;
		Human::mouse().scroll(x, y, delta);
		return ScriptValue::null();
	}, "x:int, y:int, delta:int? -> nil"});

	// Keyboard sub-module functions
	mod.functions.push_back({"keyboard_press", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().key(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "keyCode:int -> nil"}});

	mod.functions.push_back({"keyboard_type", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().type(args[0].asString());
		return ScriptValue::null();
	}, "text:string -> nil"}});

	mod.functions.push_back({"keyboard_hotkey", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::vector<int> keys;
		for (const auto& arg : args) {
			keys.push_back(static_cast<int>(arg.asInt()));
		}
		Human::keyboard().hotkey(keys);
		return ScriptValue::null();
	}, "...keys:int -> nil"}});

	// Wait/delay
	mod.functions.push_back({"wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int ms = static_cast<int>(args[0].asInt());
		Human::wait(ms);
		return ScriptValue::null();
	}, "ms:int -> nil"}});

	mod.functions.push_back({"random_wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int minMs = static_cast<int>(args[0].asInt());
		int maxMs = static_cast<int>(args[1].asInt());
		Human::randomWait(minMs, maxMs);
		return ScriptValue::null();
	}, "minMs:int, maxMs:int -> nil"}});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
