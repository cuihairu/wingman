#include "wingman/runtime/notify_bridge.hpp"

#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/event.hpp"

namespace wingman::runtime {

void installNotifyBridge() {
	// EventHub 按订阅名去重定位：已存在（脚本/前次 start 重复装配）则跳过
	if (EventHub::instance().subscriptionByName("notify.tray_bridge").has_value()) {
		return;
	}

	static const char* kTypes[] = {
		"notify.tray.show", "notify.tray.hide", "notify.tray.badge", "notify.tray.tooltip",
	};
	for (const char* type : kTypes) {
		// 意图事件 method 去掉 "notify." 前缀（notify.tray.show → tray.show）
		EventHub::instance().subscribe(type, [type](const EventMessage& msg) {
			const std::string method = std::string(type).substr(std::string("notify.").size());
			EventBuffer::instance().push(method, msg.payload);
		}, "notify.tray_bridge");
	}
}

} // namespace wingman::runtime
