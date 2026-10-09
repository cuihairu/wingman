#include "wingman/runtime/notify_bridge.hpp"

#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/event.hpp"

namespace wingman::runtime {

// 统一系统事件源 → 本地 IPC 事件流桥接（GUI/Dashboard 可见性）。
//
// systemwatch / filewatcher / trigger / macro 各自把变化同步 emit 到 EventHub
// （见 docs/api/event.md「内置事件源」）；本桥接把这些事件原样转投 EventBuffer，
// GUI 经 RPC `events.drain` 拉取。method 与 EventHub 事件名一一对应，不做改名。
//
// 与 installNotifyBridge 的差异：托盘桥接把 notify.* 前缀剥掉（语义改写），
// 本桥接保持原样（事件名即 method）。二者订阅名不同，互不干扰。
//
// 注意：这些事件由各模块的后台线程（轮询/平台后端/钩子线程）触发，订阅回调
// 在 emit 时同步执行；本桥接回调只做一次 EventBuffer::push（线程安全），无
// 脚本交互，故不受 callableThreadSafe 约束影响。
void installSystemEventBridge() {
	// EventHub 按订阅名去重定位：已装配（脚本/前次 start 重复装配）则跳过
	if (EventHub::instance().subscriptionByName("system_event_bridge").has_value()) {
		return;
	}

	static const char* kTypes[] = {
		"systemwatch.process",
		"systemwatch.window",
		"systemwatch.error",
		"filewatcher.changed",
		"filewatcher.error",
		"trigger.fired",
		"trigger.action",
		"macro.state",
		"macro.recorded",
	};
	for (const char* type : kTypes) {
		// 事件名即 method，原样透传
		EventHub::instance().subscribe(type, [type](const EventMessage& msg) {
			EventBuffer::instance().push(type, msg.payload);
		}, "system_event_bridge");
	}
}

} // namespace wingman::runtime
