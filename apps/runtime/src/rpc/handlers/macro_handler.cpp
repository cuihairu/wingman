#include "wingman/runtime/rpc/macro_handler.hpp"

namespace wingman::rpc {

void registerMacroHandlers(RpcDispatcher& dispatcher, MacroRecorder& recorder) {
	using json = nlohmann::json;

	dispatcher.registerHandler("macro.start", [&recorder](const json&) -> json {
		recorder.start();
		return {{"success", true}, {"recording", recorder.isRecording()}};
	});

	dispatcher.registerHandler("macro.stop", [&recorder](const json&) -> json {
		recorder.stop();
		return {
			{"success", true},
			{"recording", recorder.isRecording()},
			{"eventCount", recorder.getEventCount()},
		};
	});

	dispatcher.registerHandler("macro.play", [&recorder](const json& params) -> json {
		const int speed = params.value("speed", 100);
		const int repeat = params.value("repeat", 1);
		// 回放在调用线程同步执行（含事件间 sleep）；GUI 侧应异步调用避免阻塞。
		recorder.playback(speed, repeat);
		return {{"success", true}};
	});

	dispatcher.registerHandler("macro.status", [&recorder](const json&) -> json {
		return {
			{"recording", recorder.isRecording()},
			{"paused", recorder.isPaused()},
			{"eventCount", recorder.getEventCount()},
		};
	});

	dispatcher.registerHandler("macro.save", [&recorder](const json& params) -> json {
		const std::string path = params.value("path", "");
		if (path.empty()) {
			return {{"success", false}, {"error", "Missing path"}};
		}
		if (!recorder.saveToJSON(path)) {
			return {{"success", false}, {"error", "Failed to save macro"}};
		}
		return {{"success", true}, {"eventCount", recorder.getEventCount()}};
	});

	dispatcher.registerHandler("macro.load", [&recorder](const json& params) -> json {
		const std::string path = params.value("path", "");
		if (path.empty()) {
			return {{"success", false}, {"error", "Missing path"}};
		}
		if (!recorder.loadFromJSON(path)) {
			return {{"success", false}, {"error", "Failed to load macro"}};
		}
		return {{"success", true}, {"eventCount", recorder.getEventCount()}};
	});

	dispatcher.registerHandler("macro.clear", [&recorder](const json&) -> json {
		recorder.clear();
		return {{"success", true}};
	});
}

} // namespace wingman::rpc
