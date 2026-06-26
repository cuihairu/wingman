#include "clipboard_module.hpp"
#include "wingman/clipboard.hpp"
#include "module_helpers.hpp"

namespace wingman {
namespace script {
namespace modules {

ModuleDescriptor createClipboardModule() {
	ModuleDescriptor mod;
	mod.name = "clipboard";

	// setText(text) -> boolean
	mod.functions.push_back({"setText", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const std::string text = args[0].asString();
		return ScriptValue::fromBool(Clipboard::setText(text));
	}, "text:string -> boolean"});

	// getText() -> string
	mod.functions.push_back({"getText", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromString(Clipboard::getText());
	}, "() -> string"});

	// hasText() -> boolean
	mod.functions.push_back({"hasText", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(Clipboard::hasText());
	}, "() -> boolean"});

	// setHTML(html) -> boolean
	mod.functions.push_back({"setHTML", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		const std::string html = args[0].asString();
		return ScriptValue::fromBool(Clipboard::setHTML(html));
	}, "html:string -> boolean"});

	// getHTML() -> string
	mod.functions.push_back({"getHTML", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromString(Clipboard::getHTML());
	}, "() -> string"});

	// hasHTML() -> boolean
	mod.functions.push_back({"hasHTML", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(Clipboard::hasHTML());
	}, "() -> boolean"});

	// setImage(imageData, width, height) -> boolean
	mod.functions.push_back({"setImage", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (!args[0].isString() || !args[1].isInt() || !args[2].isInt()) {
			return ScriptValue::fromBool(false);
		}
		const std::string data = args[0].asString();
		const int width = static_cast<int>(args[1].asInt());
		const int height = static_cast<int>(args[2].asInt());

		// Convert base64 string to bytes
		std::vector<uint8_t> imageData(data.begin(), data.end());
		return ScriptValue::fromBool(Clipboard::setImage(imageData, width, height));
	}, "imageData:string, width:int, height:int -> boolean"});

	// getImage() -> {data:string, width:int, height:int} | null
	mod.functions.push_back({"getImage", [](const std::vector<ScriptValue>&) -> ScriptValue {
		int width = 0, height = 0;
		std::vector<uint8_t> data = Clipboard::getImage(&width, &height);
		if (data.empty()) {
			return ScriptValue::null();
		}
		return ScriptValue::fromObject({
			{"data", ScriptValue::fromString(std::string(data.begin(), data.end()))},
			{"width", ScriptValue::fromInt(width)},
			{"height", ScriptValue::fromInt(height)}
		});
	}, "() -> {data:string, width:int, height:int} | null"});

	// hasImage() -> boolean
	mod.functions.push_back({"hasImage", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(Clipboard::hasImage());
	}, "() -> boolean"});

	// setFiles(files...) -> boolean
	mod.functions.push_back({"setFiles", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::vector<std::string> files;
		for (const auto& arg : args) {
			if (arg.isString()) {
				files.push_back(arg.asString());
			} else if (arg.isArray()) {
				const auto& arr = arg.arrayVal;
				for (const auto& item : arr) {
					if (item.isString()) {
						files.push_back(item.asString());
					}
				}
			}
		}
		return ScriptValue::fromBool(Clipboard::setFiles(files));
	}, "files:string[] -> boolean"});

	// getFiles() -> string[]
	mod.functions.push_back({"getFiles", [](const std::vector<ScriptValue>&) -> ScriptValue {
		std::vector<std::string> files = Clipboard::getFiles();
		std::vector<ScriptValue> arr;
		arr.reserve(files.size());
		for (const auto& file : files) {
			arr.push_back(ScriptValue::fromString(file));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> string[]"});

	// hasFiles() -> boolean
	mod.functions.push_back({"hasFiles", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(Clipboard::hasFiles());
	}, "() -> boolean"});

	// clear() -> nil
	mod.functions.push_back({"clear", [](const std::vector<ScriptValue>&) -> ScriptValue {
		Clipboard::clear();
		return ScriptValue::null();
	}, "() -> nil"});

	// isEmpty() -> boolean
	mod.functions.push_back({"isEmpty", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return ScriptValue::fromBool(Clipboard::isEmpty());
	}, "() -> boolean"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
