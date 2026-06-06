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
		Human::mouse().click(args[0].asInt(), args[1].asInt());
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_rightClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::mouse().rightClick(args[0].asInt(), args[1].asInt());
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_doubleClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::mouse().doubleClick(args[0].asInt(), args[1].asInt());
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_drag", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::mouse().drag(args[0].asInt(), args[1].asInt(), args[2].asInt(), args[3].asInt());
		return ScriptValue::null();
	}, "fromX:int, fromY:int, toX:int, toY:int -> nil"});

	mod.functions.push_back({"mouse_scroll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int delta = args.size() > 2 ? args[2].asInt(-3) : -3;
		Human::mouse().scroll(args[0].asInt(), args[1].asInt(), delta);
		return ScriptValue::null();
	}, "x:int, y:int, delta:int? -> nil"});

	// Keyboard sub-module functions
	mod.functions.push_back({"keyboard_press", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().key(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"keyboard_down", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().keyDown(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"keyboard_up", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().keyUp(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"keyboard_type", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		bool randomCase = args.size() > 1 ? args[1].asBool() : false;
		Human::keyboard().type(args[0].asString(), randomCase);
		return ScriptValue::null();
	}, "text:string, randomCase:bool? -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
