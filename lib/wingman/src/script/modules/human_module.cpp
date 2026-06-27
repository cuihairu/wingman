#include "wingman/script/iscript_engine.hpp"
#include "wingman/human.hpp"
#include "module_helpers.hpp"

namespace wingman {
namespace script {
namespace modules {

// ========== Plan 5: 高层配置模型 + 后端映射 ==========
// 文档（human.md）暴露扁平高层字段：delay_min/delay_max/move_speed/typing_variance。
// 后端配置分散在 HumanMouseConfig/HumanKeyboardConfig，这里做语义映射。

struct HumanScriptConfig {
	int delayMin = 100;          // 随机延迟下限 ms（-> get_config.delay_min）
	int delayMax = 300;          // 随机延迟上限 ms（-> get_config.delay_max）
	double moveSpeed = 1.0;      // 移动速度系数 0.1-2.0（-> get_config.move_speed）
	double typingVariance = 0.5; // 输入变异度 0.0-1.0（-> get_config.typing_variance）
};

static HumanScriptConfig& humanScriptConfig() {
	static HumanScriptConfig instance;
	return instance;
}

// 把高层配置映射写入后端 mouse/keyboard config（仅改相关字段，保留其余字段）
static void applyHumanConfigToBackend(const HumanScriptConfig& cfg) {
	// mouse: move_speed -> moveDuration；delay -> clickDelay
	HumanMouseConfig mc = Human::mouse().getConfig();
	double speed = cfg.moveSpeed < 0.1 ? 0.1 : (cfg.moveSpeed > 2.0 ? 2.0 : cfg.moveSpeed);
	mc.minMoveDuration = static_cast<int>(100.0 / speed);
	mc.maxMoveDuration = static_cast<int>(300.0 / speed);
	mc.clickDelayMin = cfg.delayMin;
	mc.clickDelayMax = cfg.delayMax;
	Human::setMouseConfig(mc);

	// keyboard: typing_variance -> typeDelay 范围宽度
	HumanKeyboardConfig kc = Human::keyboard().getConfig();
	double v = cfg.typingVariance < 0.0 ? 0.0 : (cfg.typingVariance > 1.0 ? 1.0 : cfg.typingVariance);
	kc.typeDelayMin = 50;                              // 基准下限
	kc.typeDelayMax = 50 + static_cast<int>(100.0 * v); // v=0 -> 50/50 固定；v=1 -> 50/150
	Human::setKeyboardConfig(kc);
}

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
		Human::mouse().click(static_cast<int>(args[0].asInt()), static_cast<int>(args[1].asInt()));
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_rightClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::mouse().rightClick(static_cast<int>(args[0].asInt()), static_cast<int>(args[1].asInt()));
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_doubleClick", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::mouse().doubleClick(static_cast<int>(args[0].asInt()), static_cast<int>(args[1].asInt()));
		return ScriptValue::null();
	}, "x:int, y:int -> nil"});

	mod.functions.push_back({"mouse_drag", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::mouse().drag(static_cast<int>(args[0].asInt()), static_cast<int>(args[1].asInt()), static_cast<int>(args[2].asInt()), static_cast<int>(args[3].asInt()));
		return ScriptValue::null();
	}, "fromX:int, fromY:int, toX:int, toY:int -> nil"});

	mod.functions.push_back({"mouse_scroll", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int delta = args.size() > 2 ? static_cast<int>(args[2].asInt(-3)) : -3;
		Human::mouse().scroll(static_cast<int>(args[0].asInt()), static_cast<int>(args[1].asInt()), delta);
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

	// ========== Plan 5: 文档声明的高层拟人化 API（camelCase；Python 自动转 snake_case）==========

	mod.functions.push_back({"getConfig", [](const std::vector<ScriptValue>&) -> ScriptValue {
		auto& c = humanScriptConfig();
		return ScriptValue::fromObject({
			{"delay_min", ScriptValue::fromInt(c.delayMin)},
			{"delay_max", ScriptValue::fromInt(c.delayMax)},
			{"move_speed", ScriptValue::fromFloat(c.moveSpeed)},
			{"typing_variance", ScriptValue::fromFloat(c.typingVariance)}
		});
	}, "() -> {delay_min,delay_max,move_speed,typing_variance}"});

	mod.functions.push_back({"setConfig", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		std::string key = args.size() > 0 ? args[0].asString() : std::string();
		const ScriptValue& value = args.size() > 1 ? args[1] : ScriptValue::null();
		if (key == "delay_min") {
			c.delayMin = static_cast<int>(value.asInt(c.delayMin));
		} else if (key == "delay_max") {
			c.delayMax = static_cast<int>(value.asInt(c.delayMax));
		} else if (key == "move_speed") {
			c.moveSpeed = value.asFloat(c.moveSpeed);
		} else if (key == "typing_variance") {
			c.typingVariance = value.asFloat(c.typingVariance);
		}
		// 未知 key 静默忽略
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "key:string, value:any -> nil"});

	mod.functions.push_back({"setDelayRange", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		c.delayMin = static_cast<int>(args.size() > 0 ? args[0].asInt(c.delayMin) : c.delayMin);
		c.delayMax = static_cast<int>(args.size() > 1 ? args[1].asInt(c.delayMax) : c.delayMax);
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "min:int, max:int -> nil"});

	mod.functions.push_back({"setMoveSpeed", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		c.moveSpeed = args.size() > 0 ? args[0].asFloat(c.moveSpeed) : c.moveSpeed;
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "speed:float -> nil"});

	mod.functions.push_back({"setTypingVariance", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		auto& c = humanScriptConfig();
		c.typingVariance = args.size() > 0 ? args[0].asFloat(c.typingVariance) : c.typingVariance;
		applyHumanConfigToBackend(c);
		return ScriptValue::null();
	}, "variance:float -> nil"});

	mod.functions.push_back({"randomDelay", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int minMs = args.size() > 0 ? static_cast<int>(args[0].asInt(100)) : 100;
		int maxMs = args.size() > 1 ? static_cast<int>(args[1].asInt(300)) : 300;
		Human::mouse().randomDelay(minMs, maxMs);
		return ScriptValue::null();
	}, "min:int?, max:int? -> nil"});

	mod.functions.push_back({"naturalType", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		Human::keyboard().type(args.size() > 0 ? args[0].asString() : std::string());
		return ScriptValue::null();
	}, "text:string -> nil"});

	mod.functions.push_back({"moveMouse", [](const std::vector<ScriptValue>& args) -> ScriptValue {
		int x1 = static_cast<int>(args.size() > 0 ? args[0].asInt(0) : 0);
		int y1 = static_cast<int>(args.size() > 1 ? args[1].asInt(0) : 0);
		int x2 = static_cast<int>(args.size() > 2 ? args[2].asInt(0) : 0);
		int y2 = static_cast<int>(args.size() > 3 ? args[3].asInt(0) : 0);
		int duration = args.size() > 4 ? static_cast<int>(args[4].asInt(500)) : 500;
		Human::mouse().moveTo(Point(x1, y1), Point(x2, y2), duration);
		return ScriptValue::null();
	}, "x1:int, y1:int, x2:int, y2:int, duration:int? -> nil"});

	return mod;
}

} // namespace modules
} // namespace script
} // namespace wingman
