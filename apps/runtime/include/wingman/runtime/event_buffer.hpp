#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace wingman::runtime {

/// 单条通过 local IPC 下发到 GUI 的事件。
struct IpcEvent {
	std::string method;        ///< 事件类型，如 "log.line" / "trigger.fired" / "screenshot.frame"
	nlohmann::json payload;    ///< 事件负载
	uint64_t timestamp = 0;    ///< 毫秒级 epoch 时间戳

	nlohmann::json toJson() const {
		return {
			{"method", method},
			{"payload", payload},
			{"timestamp", timestamp},
		};
	}
};

/// 线程安全的事件缓冲区。
///
/// runtime 各子系统（日志 sink、触发器、截图）将事件 push 进来，
/// GUI 通过 RPC `events.drain` 拉取并清空。采用 pull 模型而非主动推送，
/// 避免 Rust IPC 客户端引入异步读取循环（Windows 阻塞 IO 下的帧错位风险）。
class EventBuffer {
public:
	/// 缓冲区容量上限，超出后丢弃最旧事件。
	static constexpr std::size_t kMaxEvents = 1000;

	static EventBuffer& instance();

	/// 追加一条事件（线程安全）。
	void push(std::string method, nlohmann::json payload);

	/// 拉取并移除最多 max 条事件（线程安全）。
	std::vector<IpcEvent> drain(std::size_t max = 500);

	/// 清空缓冲区。
	void clear();

	/// 当前缓冲区内事件数（线程安全，近似值）。
	std::size_t size() const;

	/// 累计因容量上限被丢弃的事件数。
	std::size_t dropped() const;

	/// 注册远程转发 sink：每次 push 时（在锁外）回调，用于把事件转发到 Go server。
	/// sink 自行决定转发哪些 method（避免把高频 log.line 全量推到服务端）。
	using RemoteSink = std::function<void(const std::string& method, const nlohmann::json& payload)>;
	void setRemoteSink(RemoteSink sink);

private:
	EventBuffer() = default;
	EventBuffer(const EventBuffer&) = delete;
	EventBuffer& operator=(const EventBuffer&) = delete;

	mutable std::mutex mutex_;
	std::deque<IpcEvent> events_;
	std::size_t dropped_ = 0;
	RemoteSink remoteSink_;
};

} // namespace wingman::runtime
