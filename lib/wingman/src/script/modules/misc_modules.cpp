#include "wingman/script/iscript_engine.hpp"
#include "wingman/ocr.hpp"
#include "wingman/qrcode.hpp"
#include "wingman/ui_automation.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/node_status.hpp"
#include "wingman/window.hpp"
#include "module_helpers.hpp"

namespace wingman {
namespace script {
namespace modules {

// ============================================================================
// OCR Module
// ============================================================================
ModuleDescriptor createOcrModule() {
	ModuleDescriptor mod;
	mod.name = "ocr";

	mod.functions.push_back({"recognize", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Rect region = toRect(args[0]);
		auto result = OCR::recognize(region);
		return ScriptValue::fromObject({
			{"success", ScriptValue::fromBool(result.success)},
			{"text", ScriptValue::fromString(result.text)},
			{"confidence", ScriptValue::fromFloat(result.confidence)}
		});
	}, "region:{x,y,width,height} -> {success,text,confidence}"});

	mod.functions.push_back({"recognizeText", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Rect region = toRect(args[0]);
		auto result = OCR::recognize(region);
		if (result.success) return ScriptValue::fromString(result.text);
		return ScriptValue::null();
	}, "region:{x,y,width,height} -> string?"});

	return mod;
}

// ============================================================================
// QRCode Module
// ============================================================================
ModuleDescriptor createQrcodeModule() {
	ModuleDescriptor mod;
	mod.name = "qrcode";

	// Module-level shared QRLoginManager instance
	static QRLoginManager qrManager;

	mod.functions.push_back({"get", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		QRLoginConfig config = QRLoginManager::genericConfig(args[0].asString(), args[1].asString());
		auto qr = qrManager.getQRCode(config);
		if (qr) return ScriptValue::fromString(*qr);
		return ScriptValue::null();
	}, "qrUrl:string, statusUrl:string -> string?"});

	mod.functions.push_back({"login", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		QRLoginConfig config = QRLoginManager::genericConfig(args[0].asString(), args[1].asString());
		if (args.size() > 2) {
			config.pollInterval = static_cast<int>(args[2].asInt(2000));
		}
		auto result = qrManager.login(config);
		auto obj = std::unordered_map<std::string, ScriptValue>{
			{"state", ScriptValue::fromString(qrLoginStateToString(result.state))},
			{"message", ScriptValue::fromString(result.message)}
		};
		if (!result.token.empty()) obj["token"] = ScriptValue::fromString(result.token);
		if (!result.sessionId.empty()) obj["sessionId"] = ScriptValue::fromString(result.sessionId);
		return ScriptValue::fromObject(std::move(obj));
	}, "qrUrl:string, statusUrl:string, pollInterval:int? -> {state,message,...}"});

	mod.functions.push_back({"cancel", [](const std::vector<ScriptValue>&) -> ScriptValue {
		qrManager.cancel();
		return ScriptValue::null();
	}, "() -> nil"});

	mod.functions.push_back({"detect", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Rect region = toRect(args[0]);
		auto result = qrManager.detectQRCode(region.x, region.y, region.width, region.height);
		if (result) return ScriptValue::fromString(*result);
		return ScriptValue::null();
	}, "region:{x,y,width,height} -> string?"});

	return mod;
}

// ============================================================================
// SmartTrigger Module
// ============================================================================
ModuleDescriptor createSmartTriggerModule() {
	ModuleDescriptor mod;
	mod.name = "smarttrigger";

	mod.functions.push_back({"create", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto trigger = SmartTriggerManager::instance().createTrigger(args[0].asString());
		return ScriptValue::fromBool(static_cast<bool>(trigger));
	}, "name:string -> bool"});

	mod.functions.push_back({"start", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto trigger = SmartTriggerManager::instance().getTrigger(args[0].asString());
		if (trigger) { trigger->start(); return ScriptValue::fromBool(true); }
		return ScriptValue::fromBool(false);
	}, "name:string -> bool"});

	mod.functions.push_back({"stop", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto trigger = SmartTriggerManager::instance().getTrigger(args[0].asString());
		if (trigger) { trigger->stop(); return ScriptValue::fromBool(true); }
		return ScriptValue::fromBool(false);
	}, "name:string -> bool"});

	mod.functions.push_back({"remove", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		SmartTriggerManager::instance().removeTrigger(args[0].asString());
		return ScriptValue::null();
	}, "name:string -> nil"});

	return mod;
}

// ============================================================================
// BehaviorTree Module
// ============================================================================
ModuleDescriptor createBehaviorTreeModule() {
	ModuleDescriptor mod;
	mod.name = "bt";

	mod.functions.push_back({"create", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto tree = BehaviorTreeManager::instance().createTree(args[0].asString());
		return ScriptValue::fromBool(static_cast<bool>(tree));
	}, "name:string -> bool"});

	mod.functions.push_back({"tick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto tree = BehaviorTreeManager::instance().getTree(args[0].asString());
		if (tree) {
			NodeStatus status = tree->tick();
			switch (status) {
			case NodeStatus::SUCCESS: return ScriptValue::fromString("SUCCESS");
			case NodeStatus::FAILURE: return ScriptValue::fromString("FAILURE");
			case NodeStatus::RUNNING: return ScriptValue::fromString("RUNNING");
			}
		}
		return ScriptValue::fromString("FAILURE");
	}, "name:string -> string"});

	mod.functions.push_back({"remove", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		BehaviorTreeManager::instance().removeTree(args[0].asString());
		return ScriptValue::null();
	}, "name:string -> nil"});

	return mod;
}

// ============================================================================
// NodeStatus Module
// ============================================================================
ModuleDescriptor createNodeModule() {
	ModuleDescriptor mod;
	mod.name = "node";

	mod.functions.push_back({"createHeartbeat", [](const std::vector<ScriptValue>&) -> ScriptValue {
		NodeHeartbeat hb;
		return ScriptValue::fromObject({
			{"json", ScriptValue::fromString(hb.toJson())},
			{"nodeId", ScriptValue::fromString(hb.nodeId)},
			{"version", ScriptValue::fromString(hb.version)}
		});
	}, "() -> {json,nodeId,version}"});

	mod.functions.push_back({"sendHeartbeat", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		// Simplified implementation: log heartbeat
		return ScriptValue::null();
	}, "heartbeat:{...} -> nil"});

#ifdef _WIN32
	mod.functions.push_back({"getWindows", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto windows = Window::enumerate();
		std::vector<ScriptValue> arr;
		for (const auto& w : windows) {
			arr.push_back(ScriptValue::fromObject({
				{"title", ScriptValue::fromString(w.title)},
				{"handle", ScriptValue::fromInt(reinterpret_cast<int64_t>(w.handle))},
				{"isForeground", ScriptValue::fromBool(w.isForeground)},
				{"bounds", fromRect(w.bounds)}
			}));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {{title,handle,isForeground,bounds}}"});
#endif

	return mod;
}

// ============================================================================
// UIAutomation Module (simplified - object-oriented access needs engine support)
// ============================================================================
ModuleDescriptor createUIAutomationModule() {
	ModuleDescriptor mod;
	mod.name = "uia";

	mod.functions.push_back({"findByName", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto elem = uia().findByName(args[0].asString());
		if (elem) return ScriptValue::fromInt(reinterpret_cast<int64_t>(elem.get()));
		return ScriptValue::null();
	}, "name:string -> handle?"});

	mod.functions.push_back({"findById", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto elem = uia().findById(args[0].asString());
		if (elem) return ScriptValue::fromInt(reinterpret_cast<int64_t>(elem.get()));
		return ScriptValue::null();
	}, "id:string -> handle?"});

	mod.functions.push_back({"find", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		UIASelector selector;
		if (args[0].isObject()) {
			const auto* name = args[0].get("name");
			if (name && name->isString()) selector.withName(name->asString());
			const auto* id = args[0].get("id");
			if (id && id->isString()) selector.withId(id->asString());
			const auto* className = args[0].get("className");
			if (className && className->isString()) selector.withClassName(className->asString());
		}
		auto elem = uia().find(selector);
		if (elem) return ScriptValue::fromInt(reinterpret_cast<int64_t>(elem.get()));
		return ScriptValue::null();
	}, "selector:{name?,id?,className?} -> handle?"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
