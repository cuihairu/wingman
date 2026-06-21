#include "wingman/runtime/rpc/event_handler.hpp"
#include "wingman/runtime/event_buffer.hpp"

namespace wingman::rpc {

void registerEventHandlers(RpcDispatcher& dispatcher) {
	using json = nlohmann::json;

	dispatcher.registerHandler("events.drain", [](const json& params) -> json {
		// 单次拉取上限，防止 GUI 长时间离线后一次性传输过多事件。
		const std::size_t max = params.value("max", 500u);
		auto events = wingman::runtime::EventBuffer::instance().drain(max);

		json arr = json::array();
		for (const auto& evt : events) {
			arr.push_back(evt.toJson());
		}

		return {
			{"events", std::move(arr)},
			{"remaining", wingman::runtime::EventBuffer::instance().size()},
			{"dropped", wingman::runtime::EventBuffer::instance().dropped()},
		};
	});
}

} // namespace wingman::rpc
