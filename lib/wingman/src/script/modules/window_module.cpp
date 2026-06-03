#include "window_module.hpp"
#include "module_helpers.hpp"
#include "wingman/window.hpp"

namespace wingman {
namespace script {
namespace modules {

namespace {

int64_t encodeWindowHandle(WindowHandle handle) {
#ifdef _WIN32
	return static_cast<int64_t>(reinterpret_cast<uintptr_t>(handle));
#else
	return static_cast<int64_t>(handle);
#endif
}

WindowHandle decodeWindowHandle(int64_t value) {
#ifdef _WIN32
	return reinterpret_cast<WindowHandle>(static_cast<uintptr_t>(value));
#else
	return static_cast<WindowHandle>(value);
#endif
}

} // namespace

ModuleDescriptor createWindowModule() {
	ModuleDescriptor mod;
	mod.name = "window";

	mod.functions.push_back({"find", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string title = args[0].asString();
		WindowHandle hwnd = Window::find(title);
		if (hwnd) {
			return ScriptValue::fromArray({
				ScriptValue::fromInt(encodeWindowHandle(hwnd)),
				ScriptValue::fromBool(true)
			});
		}
		return ScriptValue::fromArray({ScriptValue::null(), ScriptValue::fromBool(false)});
	}, "title:string -> handle:int, found:bool"});

	mod.functions.push_back({"activate", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		WindowHandle hwnd = decodeWindowHandle(args[0].asInt());
		return ScriptValue::fromBool(Window::activate(hwnd));
	}, "handle:int -> bool"});

	mod.functions.push_back({"getForeground", [](const std::vector<ScriptValue>&) -> ScriptValue {
		WindowHandle hwnd = Window::getForeground();
		return ScriptValue::fromInt(encodeWindowHandle(hwnd));
	}, "() -> handle:int"});

	mod.functions.push_back({"getTitle", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		WindowHandle hwnd = decodeWindowHandle(args[0].asInt());
		return ScriptValue::fromString(Window::getTitle(hwnd));
	}, "handle:int -> string"});

	mod.functions.push_back({"getBounds", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		WindowHandle hwnd = decodeWindowHandle(args[0].asInt());
		return fromRect(Window::getBounds(hwnd));
	}, "handle:int -> {x,y,width,height}"});

	mod.functions.push_back({"waitFor", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string title = args[0].asString();
		int timeout = args.size() > 1 ? args[1].asInt(5000) : 5000;
		return ScriptValue::fromBool(Window::waitFor(title, timeout));
	}, "title:string, timeout:int -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
