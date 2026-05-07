#include "wingman/client/gui/spdlog_sink.hpp"
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>

namespace wingman::gui {

void ImGuiLogSink::sink_it_(const spdlog::details::log_msg& msg) {
    if (!panel_) return;

    // 格式化日志消息
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string log_msg = fmt::to_string(formatted);

    // 根据 level 添加到面板
    std::string level;
    switch (msg.level) {
        case spdlog::level::trace:
        case spdlog::level::debug:
            level = "DEBUG";
            break;
        case spdlog::level::info:
            level = "INFO";
            break;
        case spdlog::level::warn:
            level = "WARNING";
            break;
        case spdlog::level::err:
        case spdlog::level::critical:
            level = "ERROR";
            break;
        default:
            level = "INFO";
            break;
    }

    panel_->addLog(level + ": " + log_msg);
}

} // namespace wingman::gui
