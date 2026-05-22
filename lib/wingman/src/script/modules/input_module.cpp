#include "input_module.hpp"
#include "wingman/input.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createInputModule() {
	ModuleDescriptor mod;
	mod.name = "input";

	mod.functions.push_back({"click", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		int button = args.size() > 2 ? args[2].asInt(0) : 0;
		MouseButton btn = MouseButton::Left;
		if (button == 1) btn = MouseButton::Middle;
		else if (button == 2) btn = MouseButton::Right;
		Input::click(x, y, btn);
		return ScriptValue::null();
	}, "x:int, y:int, button:int -> nil"});

	mod.functions.push_back({"move", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		int duration = args.size() > 2 ? args[2].asInt(0) : 0;
		if (duration > 0) {
			Input::move(x, y, duration);
		} else {
			Input::move(x, y);
		}
		return ScriptValue::null();
	}, "x:int, y:int, duration:int -> nil"});

	mod.functions.push_back({"scroll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		int delta = args[2].asInt();
		Input::scroll(x, y, delta);
		return ScriptValue::null();
	}, "x:int, y:int, delta:int -> nil"});

	mod.functions.push_back({"keyDown", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Input::keyDown(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"keyUp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Input::keyUp(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"key", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Input::key(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"type", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string text = args[0].asString();
		int delay = args.size() > 1 ? args[1].asInt(10) : 10;
		Input::type(text, delay);
		return ScriptValue::null();
	}, "text:string, delay:int -> nil"});

	mod.functions.push_back({"delay", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Input::delay(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "ms:int -> nil"});

	mod.functions.push_back({"randomDelay", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int minMs = static_cast<int>(args[0].asInt());
		int maxMs = static_cast<int>(args[1].asInt());
		Input::randomDelay(minMs, maxMs);
		return ScriptValue::null();
	}, "min:int, max:int -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
