#pragma once

#include "wingman/agentcore/event_buffer.hpp"

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace wingman::runtime {

/// 将 spdlog 日志转发到 EventBuffer（method = "log.line"）的 sink。
///
/// 仅捕获运行时进程内的日志输出，作为 GUI 实时日志面板的数据源。
/// 避免在 payload 中包含敏感信息；该 sink 与控制台 sink 并列挂载。
class EventLogSink : public spdlog::sinks::base_sink<std::mutex> {
public:
	explicit EventLogSink(std::size_t max_level = static_cast<std::size_t>(spdlog::level::info))
		: max_level_(max_level) {}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override {
		// spdlog 级别数值：trace=0 < debug=1 < info=2 < warn=3 < err=4。
		// 低于配置级别的（如 info 下限时的 debug）不下发；原实现写成
		// `> max_level_`，方向反了——warn/error 全被滤掉、GUI 日志面板
		// 永远收不到告警与错误，反而放行低级别噪音。
		if (static_cast<std::size_t>(msg.level) < max_level_) {
			return; // 仅下发 >= 配置级别（info/warn/error）的日志
		}

		const auto level_sv = spdlog::level::to_string_view(msg.level);
		std::string text(msg.payload.data(), msg.payload.size());
		// 截断超长日志（如误输出的 base64/二进制），避免单条事件撑大有界 EventBuffer。
		if (text.size() > kMaxMessageLen) {
			text.resize(kMaxMessageLen);
			text += "...[truncated]";
		}
		nlohmann::json payload = {
			{"level", std::string(level_sv.data(), level_sv.size())},
			{"message", std::move(text)},
		};
		EventBuffer::instance().push("log.line", std::move(payload));
	}

	void flush_() override {}

private:
	static constexpr std::size_t kMaxMessageLen = 4096;
	std::size_t max_level_;
};

inline std::shared_ptr<spdlog::sinks::sink> createEventLogSink(
	std::size_t max_level = static_cast<std::size_t>(spdlog::level::info)) {
	return std::make_shared<EventLogSink>(max_level);
}

} // namespace wingman::runtime
