#pragma once

#include "wingman/client/gui/panels/log_panel.hpp"
#include <spdlog/sinks/base_sink.h>
#include <mutex>

namespace wingman::gui {

// ImGui 日志 sink - 将 spdlog 日志输出到 LogPanel
class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit ImGuiLogSink(LogPanel* panel) : panel_(panel) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override {}

private:
    LogPanel* panel_;
};

} // namespace wingman::gui
