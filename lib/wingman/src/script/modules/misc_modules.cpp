#include "wingman/script/iscript_engine.hpp"
#include "wingman/ocr.hpp"
#include "wingman/ui_automation.hpp"
#include "wingman/smart_trigger.hpp"
#include "wingman/behavior_tree.hpp"
#include "wingman/node_status.hpp"
#include "wingman/window.hpp"
#include "wingman/event.hpp"
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
// SmartTrigger Module
// ============================================================================
namespace {
// ScriptValue 表 -> TriggerCondition（type 兼容 snake_case 与原大写枚举名）
TriggerCondition parseCondition(const ScriptValue& v) {
	TriggerCondition c;
	const ScriptValue* typeVal = v.get("type");
	const std::string type = typeVal ? typeVal->asString() : "";
	if (type == "color_found" || type == "COLOR_FOUND") c.type = TriggerConditionType::COLOR_FOUND;
	else if (type == "color_not_found" || type == "COLOR_NOT_FOUND") c.type = TriggerConditionType::COLOR_NOT_FOUND;
	else if (type == "image_found" || type == "IMAGE_FOUND") c.type = TriggerConditionType::IMAGE_FOUND;
	else if (type == "image_not_found" || type == "IMAGE_NOT_FOUND") c.type = TriggerConditionType::IMAGE_NOT_FOUND;
	else if (type == "text_found" || type == "TEXT_FOUND") c.type = TriggerConditionType::TEXT_FOUND;
	else if (type == "text_not_found" || type == "TEXT_NOT_FOUND") c.type = TriggerConditionType::TEXT_NOT_FOUND;
	else if (type == "edge_detected" || type == "EDGE_DETECTED") c.type = TriggerConditionType::EDGE_DETECTED;
	else if (type == "color_changed" || type == "COLOR_CHANGED") c.type = TriggerConditionType::COLOR_CHANGED;
	else if (type == "ocr_contains" || type == "OCR_CONTAINS") c.type = TriggerConditionType::OCR_CONTAINS;
	else if (type == "ocr_equals" || type == "OCR_EQUALS") c.type = TriggerConditionType::OCR_EQUALS;

	if (const ScriptValue* color = v.get("color")) c.targetColor = toColor(*color);
	if (const ScriptValue* tol = v.get("tolerance")) c.tolerance = static_cast<int>(tol->asInt());
	if (const ScriptValue* thr = v.get("threshold")) c.threshold = thr->asFloat(0.8);
	if (const ScriptValue* region = v.get("region")) c.searchRegion = toRect(*region);
	if (const ScriptValue* text = v.get("text")) c.targetText = text->asString();
	if (const ScriptValue* tpl = v.get("template")) c.templatePath = tpl->asString();
	if (const ScriptValue* tplPath = v.get("templatePath")) c.templatePath = tplPath->asString();
	return c;
}

// ScriptValue 表 -> TriggerAction
TriggerAction parseAction(const ScriptValue& v) {
	TriggerAction a;
	const ScriptValue* typeVal = v.get("type");
	const std::string type = typeVal ? typeVal->asString() : "";
	if (type == "click" || type == "CLICK") a.type = TriggerActionType::CLICK;
	else if (type == "key_press" || type == "KEY_PRESS" || type == "keypress") a.type = TriggerActionType::KEY_PRESS;
	else if (type == "wait" || type == "WAIT" || type == "delay") a.type = TriggerActionType::WAIT;
	else if (type == "lua_script" || type == "LUA_SCRIPT") a.type = TriggerActionType::LUA_SCRIPT;
	else if (type == "log" || type == "LOG") a.type = TriggerActionType::LOG;
	else if (type == "stop" || type == "STOP") a.type = TriggerActionType::STOP;

	if (const ScriptValue* pos = v.get("position")) {
		if (pos->isObject()) {
			if (const ScriptValue* px = pos->get("x")) a.clickPosition.x = static_cast<int>(px->asInt());
			if (const ScriptValue* py = pos->get("y")) a.clickPosition.y = static_cast<int>(py->asInt());
		}
	}
	if (const ScriptValue* x = v.get("x")) a.clickPosition.x = static_cast<int>(x->asInt());
	if (const ScriptValue* y = v.get("y")) a.clickPosition.y = static_cast<int>(y->asInt());
	if (const ScriptValue* key = v.get("key")) a.keyCode = static_cast<int>(key->asInt());
	if (const ScriptValue* keyCode = v.get("keyCode")) a.keyCode = static_cast<int>(keyCode->asInt());
	if (const ScriptValue* waitMs = v.get("waitMs")) a.waitMs = static_cast<int>(waitMs->asInt());
	if (const ScriptValue* delay = v.get("delay")) a.waitMs = static_cast<int>(delay->asInt());
	if (const ScriptValue* script = v.get("script")) a.luaScript = script->asString();
	if (const ScriptValue* msg = v.get("message")) a.logMessage = msg->asString();
	return a;
}
} // namespace

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

	mod.functions.push_back({"addCondition", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);
		auto t = SmartTriggerManager::instance().getTrigger(args[0].asString());
		if (!t) return ScriptValue::fromBool(false);
		t->addCondition(parseCondition(args[1]));
		return ScriptValue::fromBool(true);
	}, "name:string, condition:{type:string, color?, tolerance?, threshold?, region?, text?, template?} -> bool"});

	mod.functions.push_back({"addAction", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);
		auto t = SmartTriggerManager::instance().getTrigger(args[0].asString());
		if (!t) return ScriptValue::fromBool(false);
		t->addAction(parseAction(args[1]));
		return ScriptValue::fromBool(true);
	}, "name:string, action:{type:string, x?, y?, key?, waitMs?, script?, message?} -> bool"});

	mod.functions.push_back({"setCheckInterval", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.size() < 2) return ScriptValue::fromBool(false);
		auto t = SmartTriggerManager::instance().getTrigger(args[0].asString());
		if (!t) return ScriptValue::fromBool(false);
		t->setCheckInterval(static_cast<int>(args[1].asInt()));
		return ScriptValue::fromBool(true);
	}, "name:string, ms:int -> bool"});

	mod.functions.push_back({"isRunning", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromBool(false);
		auto t = SmartTriggerManager::instance().getTrigger(args[0].asString());
		return ScriptValue::fromBool(t && t->isRunning());
	}, "name:string -> bool"});

	mod.functions.push_back({"getTriggerCount", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		if (args.empty()) return ScriptValue::fromInt(0);
		auto t = SmartTriggerManager::instance().getTrigger(args[0].asString());
		return ScriptValue::fromInt(t ? t->getTriggerCount() : 0);
	}, "name:string -> int"});

	return mod;
}

// ============================================================================
// BehaviorTree Module
// ============================================================================

// ========== Plan 7: 行为树节点句柄注册表 ==========
// 脚本层通过 int handle 引用构造的 BehaviorNode（shared_ptr 生命周期管理）。
// handle 从 1 自增，0 表示无效。用于 addChild / setRoot 组装行为树。
static std::map<int, std::shared_ptr<BehaviorNode>>& btNodeRegistry() {
	static std::map<int, std::shared_ptr<BehaviorNode>> registry;
	return registry;
}
static int btStoreNode(std::shared_ptr<BehaviorNode> node) {
	static int nextId = 0;
	int id = ++nextId;
	btNodeRegistry()[id] = std::move(node);
	return id;
}
static std::shared_ptr<BehaviorNode> btGetNode(int handle) {
	auto& reg = btNodeRegistry();
	auto it = reg.find(handle);
	return it != reg.end() ? it->second : nullptr;
}

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

	// ========== Plan 7: 节点构造（复合 / 装饰 / 叶子）==========

	mod.functions.push_back({"sequence", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Sequence") : "Sequence";
		return ScriptValue::fromInt(btStoreNode(BehaviorTree::sequence(name)));
	}, "name:string? -> handle:int"});

	mod.functions.push_back({"selector", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Selector") : "Selector";
		return ScriptValue::fromInt(btStoreNode(BehaviorTree::selector(name)));
	}, "name:string? -> handle:int"});

	mod.functions.push_back({"parallel", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Parallel") : "Parallel";
		std::string policyStr = args.size() > 1 ? args[1].asString("SUCCEED_ON_ALL") : "SUCCEED_ON_ALL";
		ParallelNode::Policy policy = ParallelNode::Policy::SUCCEED_ON_ALL;
		if (policyStr == "SUCCEED_ON_ONE") policy = ParallelNode::Policy::SUCCEED_ON_ONE;
		else if (policyStr == "FAIL_ON_ALL") policy = ParallelNode::Policy::FAIL_ON_ALL;
		else if (policyStr == "FAIL_ON_ONE") policy = ParallelNode::Policy::FAIL_ON_ONE;
		return ScriptValue::fromInt(btStoreNode(BehaviorTree::parallel(name, policy)));
	}, "name:string?, policy:string? -> handle:int"});

	mod.functions.push_back({"wait", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int ms = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		return ScriptValue::fromInt(btStoreNode(std::make_shared<WaitNode>(ms)));
	}, "milliseconds:int -> handle:int"});

	mod.functions.push_back({"inverter", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int childHandle = static_cast<int>(args.size() > 0 ? args[0].asInt(-1) : -1);
		auto child = btGetNode(childHandle);
		if (!child) return ScriptValue::fromInt(0); // 无效 child
		return ScriptValue::fromInt(btStoreNode(std::make_shared<InverterNode>(child)));
	}, "childHandle:int -> handle:int"});

	mod.functions.push_back({"repeat", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int childHandle = static_cast<int>(args.size() > 0 ? args[0].asInt(-1) : -1);
		int count = static_cast<int>(args.size() > 1 ? args[1].asInt(-1) : -1); // -1 = 无限
		auto child = btGetNode(childHandle);
		if (!child) return ScriptValue::fromInt(0);
		return ScriptValue::fromInt(btStoreNode(std::make_shared<RepeatNode>(child, count)));
	}, "childHandle:int, count:int? -> handle:int"});

	mod.functions.push_back({"addChild", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int parentHandle = static_cast<int>(args.size() > 0 ? args[0].asInt(-1) : -1);
		int childHandle = static_cast<int>(args.size() > 1 ? args[1].asInt(-1) : -1);
		auto parent = btGetNode(parentHandle);
		auto child = btGetNode(childHandle);
		if (!parent || !child) return ScriptValue::fromBool(false);
		// 仅复合节点支持 addChild
		if (auto seq = std::dynamic_pointer_cast<SequenceNode>(parent)) { seq->addChild(child); return ScriptValue::fromBool(true); }
		if (auto sel = std::dynamic_pointer_cast<SelectorNode>(parent)) { sel->addChild(child); return ScriptValue::fromBool(true); }
		if (auto par = std::dynamic_pointer_cast<ParallelNode>(parent)) { par->addChild(child); return ScriptValue::fromBool(true); }
		return ScriptValue::fromBool(false); // 装饰/叶子节点不支持
	}, "parentHandle:int, childHandle:int -> bool"});

	mod.functions.push_back({"condition", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Condition") : "Condition";
		if (args.size() < 2 || !args[1].isCallable()) return ScriptValue::fromInt(0); // 无效 callable
		ScriptValue::CallableFunc cb = args[1].callableVal; // 拷贝；闭包持有，节点存活期间有效
		auto node = BehaviorTree::condition(name, [cb]() -> bool {
			return cb({}).asBool(false); // 无参回调脚本，返回 bool
		});
		return ScriptValue::fromInt(btStoreNode(node));
	}, "name:string, callback:callable -> handle:int"});

	mod.functions.push_back({"action", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString("Action") : "Action";
		if (args.size() < 2 || !args[1].isCallable()) return ScriptValue::fromInt(0);
		ScriptValue::CallableFunc cb = args[1].callableVal;
		auto node = BehaviorTree::action(name, [cb]() -> NodeStatus {
			std::string s = cb({}).asString("FAILURE");
			if (s == "SUCCESS") return NodeStatus::SUCCESS;
			if (s == "RUNNING") return NodeStatus::RUNNING;
			return NodeStatus::FAILURE;
		});
		return ScriptValue::fromInt(btStoreNode(node));
	}, "name:string, callback:callable -> handle:int"});

	mod.functions.push_back({"setRoot", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string treeName = args.size() > 0 ? args[0].asString() : std::string();
		int nodeHandle = static_cast<int>(args.size() > 1 ? args[1].asInt(-1) : -1);
		auto tree = BehaviorTreeManager::instance().getTree(treeName);
		auto node = btGetNode(nodeHandle);
		if (!tree || !node) return ScriptValue::fromBool(false);
		tree->setRoot(node);
		return ScriptValue::fromBool(true);
	}, "treeName:string, nodeHandle:int -> bool"});

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

	mod.functions.push_back({"sendHeartbeat", [](const std::vector<ScriptValue>& /*args*/) -> ScriptValue {
		// Simplified implementation: log heartbeat
		return ScriptValue::null();
	}, "heartbeat:{...} -> nil"});

	mod.functions.push_back({"getWindows", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto windows = Window::enumerate();
		std::vector<ScriptValue> arr;
		for (const auto& w : windows) {
			arr.push_back(ScriptValue::fromObject({
				{"title", ScriptValue::fromString(w.title)},
				{"handle", ScriptValue::fromInt(
#ifdef _WIN32
					reinterpret_cast<int64_t>(w.handle)
#else
					static_cast<int64_t>(w.handle)
#endif
				)},
				{"isForeground", ScriptValue::fromBool(w.isForeground)},
				{"bounds", fromRect(w.bounds)}
			}));
		}
		return ScriptValue::fromArray(std::move(arr));
	}, "() -> {{title,handle,isForeground,bounds}}"});

	return mod;
}

// ============================================================================
// UIAutomation Module (Plan 6: OO API with registry)
// ============================================================================

// ========== Plan 6: UIA 元素句柄注册表 ==========
// 脚本层通过 int handle 引用 IUIAElement（shared_ptr 生命周期管理）。
// handle 从 1 自增，0 表示无效。UIElement OO 对象的方法 Callable 闭包捕获 handle 查元素。
static std::map<int, std::shared_ptr<IUIAElement>>& uiaElementRegistry() {
	static std::map<int, std::shared_ptr<IUIAElement>> registry;
	return registry;
}
static int uiaStoreElement(std::shared_ptr<IUIAElement> e) {
	static int nextId = 0;
	int id = ++nextId;
	uiaElementRegistry()[id] = std::move(e);
	return id;
}
static std::shared_ptr<IUIAElement> uiaGetElement(int handle) {
	auto& reg = uiaElementRegistry();
	auto it = reg.find(handle);
	return it != reg.end() ? it->second : nullptr;
}

// 构造 UIElement OO 对象（ScriptValue::Object）：_handle + 12 方法 Callable。
// 每个方法闭包捕获 handle，通过 uiaGetElement(handle) 查元素；无效 handle 返回 null/false。
static ScriptValue makeUiaElementObject(std::shared_ptr<IUIAElement> e) {
	if (!e) return ScriptValue::null();
	int handle = uiaStoreElement(e);
	std::unordered_map<std::string, ScriptValue> obj;
	obj["_handle"] = ScriptValue::fromInt(handle);

	obj["get_info"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		if (!el) return ScriptValue::null();
		UIAElementInfo info = el->getInfo();
		return ScriptValue::fromObject({
			{"name", ScriptValue::fromString(info.name)},
			{"id", ScriptValue::fromString(info.id)},
			{"className", ScriptValue::fromString(info.className)},
			{"role", ScriptValue::fromInt(static_cast<int64_t>(info.role))},
			{"text", ScriptValue::fromString(info.text)},
			{"is_enabled", ScriptValue::fromBool(info.isEnabled)},
			{"is_visible", ScriptValue::fromBool(info.isVisible)},
			{"has_focus", ScriptValue::fromBool(info.hasFocus)},
			{"bounds", fromRect(info.bounds)}
		});
	});

	obj["click"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->click()) : ScriptValue::fromBool(false);
	});

	obj["double_click"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->doubleClick()) : ScriptValue::fromBool(false);
	});

	obj["focus"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->setFocus()) : ScriptValue::fromBool(false);
	});

	obj["get_value"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromString(el->getText()) : ScriptValue::null();
	});

	obj["set_value"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto el = uiaGetElement(handle);
		if (!el) return ScriptValue::fromBool(false);
		std::string text = args.size() > 0 ? args[0].asString() : std::string();
		return ScriptValue::fromBool(el->setText(text));
	});

	obj["get_children"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		if (!el) return ScriptValue::fromArray({});
		auto children = el->getChildren();
		std::vector<ScriptValue> arr;
		for (auto& c : children) arr.push_back(makeUiaElementObject(c));
		return ScriptValue::fromArray(std::move(arr));
	});

	obj["expand"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->expand()) : ScriptValue::fromBool(false);
	});

	obj["collapse"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->collapse()) : ScriptValue::fromBool(false);
	});

	obj["is_expanded"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->isExpanded()) : ScriptValue::fromBool(false);
	});

	obj["is_visible"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->isVisible()) : ScriptValue::fromBool(false);
	});

	obj["is_enabled"] = ScriptValue::fromCallable([handle](const std::vector<ScriptValue>&) -> ScriptValue {
		auto el = uiaGetElement(handle);
		return el ? ScriptValue::fromBool(el->isEnabled()) : ScriptValue::fromBool(false);
	});

	return ScriptValue::fromObject(std::move(obj));
}

ModuleDescriptor createUIAutomationModule() {
	ModuleDescriptor mod;
	mod.name = "uia";

	// ===== 根元素获取 =====
	mod.functions.push_back({"from_foreground", [](const std::vector<ScriptValue>&) -> ScriptValue {
		return makeUiaElementObject(uia().getFocusedElement());
	}, "() -> UIElement?"});

	mod.functions.push_back({"from_point", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		int y = static_cast<int>(args.size() > 1 ? args[1].asInt(0) : 0);
		return makeUiaElementObject(uia().getElementFromPoint(x, y));
	}, "x:int, y:int -> UIElement?"});

	mod.functions.push_back({"from_window", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		uint64_t hwnd = static_cast<uint64_t>(args.size() > 0 ? args[0].asInt(0) : 0);
		return makeUiaElementObject(uia().fromWindow(hwnd));
	}, "hwnd:int -> UIElement?"});

	// ===== 通用查找 =====
	mod.functions.push_back({"find_by_name", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		return makeUiaElementObject(uia().findByName(name));
	}, "name:string -> UIElement?"});

	mod.functions.push_back({"find_by_id", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string id = args.size() > 0 ? args[0].asString() : std::string();
		return makeUiaElementObject(uia().findById(id));
	}, "id:string -> UIElement?"});

	mod.functions.push_back({"find_all_by_control_type", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int roleInt = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		UIARole role = static_cast<UIARole>(roleInt);
		auto elems = uia().findAllByRole(role);
		std::vector<ScriptValue> arr;
		for (auto& e : elems) arr.push_back(makeUiaElementObject(e));
		return ScriptValue::fromArray(std::move(arr));
	}, "controlType:int -> [UIElement]"});

	mod.functions.push_back({"wait_for_name", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		int timeout = static_cast<int>(args.size() > 1 ? args[1].asInt(3000) : 3000);
		return makeUiaElementObject(uia().waitForName(name, timeout));
	}, "name:string, timeout:int -> UIElement?"});

	// ===== 专用查找 =====
	mod.functions.push_back({"find_button", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		return makeUiaElementObject(uia().find(UIASelector{}.withRole(UIARole::Button).withName(name)));
	}, "name:string -> UIElement?"});

	mod.functions.push_back({"find_edit", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		return makeUiaElementObject(uia().find(UIASelector{}.withRole(UIARole::TextBox).withName(name)));
	}, "name:string -> UIElement?"});

	mod.functions.push_back({"find_text", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		// UIARole 枚举未收录 Text 控件类型，按名称查找
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		return makeUiaElementObject(uia().find(UIASelector{}.withName(name)));
	}, "name:string -> UIElement?"});

	// ===== 事件监听 =====
	// UIA 事件回调从后台线程触发（Win UIA RPC / Mac AXObserver run loop），
	// 非线程安全 callable（如 Lua）跨线程调用会崩溃，故用 callableThreadSafe 门控（仿 task_module async 检查）。
	mod.functions.push_back({"on_property_changed", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		if (args.size() < 2 || !args[1].isCallable()) {
			return ScriptValue::fromInt(0);
		}
		// 事件回调跨线程，拒绝非线程安全 callable（如 Lua）
		if (!args[1].callableThreadSafe) {
			EventHub::instance().emit("uia.error", {
				{"error", "UIA 事件回调跨线程触发，需要线程安全的 callable（如 Python 函数）。Lua callable 非线程安全，请改用 Python 注册 UIA 事件。"}
			}, "uia");
			return ScriptValue::fromInt(0);
		}
		ScriptValue::CallableFunc userCb = args[1].callableVal;  // 拷贝；闭包持有
		UIASelector selector = UIASelector{}.withName(name);
		// 后端传元素 → 转为文档签名 callback(prop=元素name, value=元素当前文本)
		uint64_t id = uia().addPropertyChangedListener(selector, [userCb](std::shared_ptr<IUIAElement> e) {
			if (!userCb) return;
			std::vector<ScriptValue> cbArgs;
			cbArgs.push_back(ScriptValue::fromString(e ? e->getName() : std::string()));
			cbArgs.push_back(ScriptValue::fromString(e ? e->getText() : std::string()));
			userCb(cbArgs);
		});
		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "name:string, callback:function -> listenerId:int"});

	mod.functions.push_back({"on_structure_changed", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		std::string name = args.size() > 0 ? args[0].asString() : std::string();
		if (args.size() < 2 || !args[1].isCallable()) {
			return ScriptValue::fromInt(0);
		}
		if (!args[1].callableThreadSafe) {
			EventHub::instance().emit("uia.error", {
				{"error", "UIA 事件回调跨线程触发，需要线程安全的 callable（如 Python 函数）。Lua callable 非线程安全，请改用 Python 注册 UIA 事件。"}
			}, "uia");
			return ScriptValue::fromInt(0);
		}
		ScriptValue::CallableFunc userCb = args[1].callableVal;
		UIASelector selector = UIASelector{}.withName(name);
		uint64_t id = uia().addStructureChangedListener(selector, [userCb](std::shared_ptr<IUIAElement>) {
			if (!userCb) return;
			userCb({});
		});
		return ScriptValue::fromInt(static_cast<int64_t>(id));
	}, "name:string, callback:function -> listenerId:int"});

	mod.functions.push_back({"remove_event_listener", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		uint64_t id = static_cast<uint64_t>(args.size() > 0 ? args[0].asInt(0) : 0);
		return ScriptValue::fromBool(uia().removeEventListener(id));
	}, "listenerId:int -> bool"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
