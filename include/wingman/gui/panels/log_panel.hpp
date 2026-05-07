#pragma once

#include "wingman/gui/config.hpp"
#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>

namespace wingman::gui {

// 日志面板
class LogPanel {
public:
    LogPanel();
    ~LogPanel() = default;

    void render(bool* show);

    // 添加日志
    void addLog(const std::string& log);
    void clear();

    // 设置配置指针（用于获取默认值）
    void setConfig(const GuiConfig* config);

private:
    void renderLogFilters();
    void renderLogList();

    struct LogEntry {
        std::string message;
        std::string level;
        float time;
    };

    std::vector<LogEntry> logs_;
    std::mutex logsMutex_;

    // 过滤器
    bool filterInfo_;
    bool filterWarning_;
    bool filterError_;
    bool filterDebug_;
    char searchBuffer_[256];

    // 显示选项
    bool autoScroll_;
    size_t maxLogs_;

    // 配置引用
    const GuiConfig* config_;
};

} // namespace wingman::gui
