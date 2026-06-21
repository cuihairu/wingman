#include "wingman/runtime/event_buffer.hpp"

#include <chrono>

namespace wingman::runtime {

EventBuffer& EventBuffer::instance() {
	static EventBuffer buffer;
	return buffer;
}

void EventBuffer::push(std::string method, nlohmann::json payload) {
	const auto now = std::chrono::system_clock::now();
	const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		now.time_since_epoch()).count();

	IpcEvent evt;
	evt.method = std::move(method);
	evt.payload = std::move(payload);
	evt.timestamp = static_cast<uint64_t>(ms);

	std::lock_guard<std::mutex> lock(mutex_);
	events_.emplace_back(std::move(evt));
	while (events_.size() > kMaxEvents) {
		// 公平性：容量超限时优先丢弃高频的 log.line，保护低频重要事件
		// （trigger.fired / script.state_changed / connection.state_changed）。
		bool evicted = false;
		for (auto it = events_.begin(); it != events_.end(); ++it) {
			if (it->method == "log.line") {
				events_.erase(it);
				evicted = true;
				break;
			}
		}
		if (!evicted) {
			events_.pop_front();
		}
		++dropped_;
	}
}

std::vector<IpcEvent> EventBuffer::drain(std::size_t max) {
	std::vector<IpcEvent> out;
	std::lock_guard<std::mutex> lock(mutex_);
	const std::size_t n = std::min(max, events_.size());
	out.reserve(n);
	for (std::size_t i = 0; i < n; ++i) {
		out.push_back(std::move(events_.front()));
		events_.pop_front();
	}
	return out;
}

void EventBuffer::clear() {
	std::lock_guard<std::mutex> lock(mutex_);
	events_.clear();
}

std::size_t EventBuffer::size() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return events_.size();
}

std::size_t EventBuffer::dropped() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return dropped_;
}

} // namespace wingman::runtime
