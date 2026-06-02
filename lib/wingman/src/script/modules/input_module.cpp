#include "input_module.hpp"
#include "wingman/platform/input_factory.hpp"

#include <chrono>
#include <memory>
#include <random>
#include <thread>

namespace wingman {
namespace script {
namespace modules {

namespace {

platform::IInput& getInput() {
	static std::shared_ptr<platform::IInput> input = platform::defaultSharedInput();
	return *input;
}

void sleepMs(int ms) {
	if (ms > 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
}

}

ModuleDescriptor createInputModule() {
	ModuleDescriptor mod;
	mod.name = "input";

	mod.functions.push_back({"click", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		int button = args.size() > 2 ? args[2].asInt(0) : 0;
		platform::MouseButton btn = platform::MouseButton::Left;
		if (button == 1) btn = platform::MouseButton::Middle;
		else if (button == 2) btn = platform::MouseButton::Right;
		getInput().mouseMove(x, y);
		getInput().mouseClick(btn);
		return ScriptValue::null();
	}, "x:int, y:int, button:int -> nil"});

	mod.functions.push_back({"move", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		int duration = args.size() > 2 ? args[2].asInt(0) : 0;
		getInput().mouseMove(x, y);
		sleepMs(duration);
		return ScriptValue::null();
	}, "x:int, y:int, duration:int -> nil"});

	mod.functions.push_back({"scroll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = args[0].asInt();
		int y = args[1].asInt();
		int delta = args[2].asInt();
		getInput().mouseMove(x, y);
		getInput().mouseWheel(delta);
		return ScriptValue::null();
	}, "x:int, y:int, delta:int -> nil"});

	mod.functions.push_back({"keyDown", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		getInput().keyDown(static_cast<platform::KeyCode>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"keyUp", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		getInput().keyUp(static_cast<platform::KeyCode>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"key", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		getInput().keyPress(static_cast<platform::KeyCode>(args[0].asInt()));
		return ScriptValue::null();
	}, "vkCode:int -> nil"});

	mod.functions.push_back({"type", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string text = args[0].asString();
		int delay = args.size() > 1 ? args[1].asInt(10) : 10;
		getInput().textInput(text);
		sleepMs(delay);
		return ScriptValue::null();
	}, "text:string, delay:int -> nil"});

	mod.functions.push_back({"delay", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		sleepMs(static_cast<int>(args[0].asInt()));
		return ScriptValue::null();
	}, "ms:int -> nil"});

	mod.functions.push_back({"randomDelay", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int minMs = static_cast<int>(args[0].asInt());
		int maxMs = static_cast<int>(args[1].asInt());
		if (maxMs < minMs) {
			std::swap(minMs, maxMs);
		}
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dist(minMs, maxMs);
		sleepMs(dist(gen));
		return ScriptValue::null();
	}, "min:int, max:int -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
