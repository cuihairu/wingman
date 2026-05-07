#include "wingman/gui/panels/log_panel.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>

namespace wingman::gui {

LogPanel::LogPanel()
    : filterInfo_(true)
    , filterWarning_(true)
    , filterError_(true)
    , filterDebug_(false)
    , autoScroll_(true)
    , maxLogs_(1000)
    , config_(nullptr)
{
    memset(searchBuffer_, 0, sizeof(searchBuffer_));
}

void LogPanel::setConfig(const GuiConfig* config) {
    config_ = config;
    // 应用配置中的过滤器设置
    if (config) {
        filterInfo_ = config->logFilterInfo;
        filterWarning_ = config->logFilterWarning;
        filterError_ = config->logFilterError;
        filterDebug_ = config->logFilterDebug;
    }
}

void LogPanel::render(bool* show) {
    if (!ImGui::Begin("日志", show)) {
        ImGui::End();
        return;
    }

    // 过滤器
    renderLogFilters();

    ImGui::Separator();

    // 日志列表
    renderLogList();

    ImGui::End();
}

void LogPanel::renderLogFilters() {
    if (ImGui::Button("清除")) {
        clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Info", &filterInfo_);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &filterWarning_);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &filterError_);
    ImGui::SameLine();
    ImGui::Checkbox("Debug", &filterDebug_);
    ImGui::SameLine();
    ImGui::Checkbox("自动滚动", &autoScroll_);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("搜索", searchBuffer_, sizeof(searchBuffer_));
}

void LogPanel::renderLogList() {
    ImGui::BeginChild("LogList", ImVec2(0, 0), true);

    std::lock_guard<std::mutex> lock(logsMutex_);

    for (const auto& log : logs_) {
        // 过滤日志级别
        if (log.level == "INFO" && !filterInfo_) continue;
        if (log.level == "WARNING" && !filterWarning_) continue;
        if (log.level == "ERROR" && !filterError_) continue;
        if (log.level == "DEBUG" && !filterDebug_) continue;

        // 搜索过滤
        if (searchBuffer_[0] != '\0') {
            if (log.message.find(searchBuffer_) == std::string::npos) {
                continue;
            }
        }

        // 设置颜色
        ImVec4 color = ImVec4(1, 1, 1, 1);
        if (log.level == "WARNING") {
            color = ImVec4(1, 0.8f, 0, 1);
        } else if (log.level == "ERROR") {
            color = ImVec4(1, 0.2f, 0.2f, 1);
        }

        // 显示时间戳和消息
        char timeBuffer[64];
        int seconds = static_cast<int>(log.time);
        int millis = static_cast<int>((log.time - seconds) * 1000);
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d.%03d",
            seconds / 3600, (seconds % 3600) / 60, seconds % 60, millis);

        ImGui::TextColored(color, "[%s] [%s] %s", timeBuffer, log.level.c_str(), log.message.c_str());
    }

    if (autoScroll_) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void LogPanel::addLog(const std::string& log) {
    std::lock_guard<std::mutex> lock(logsMutex_);

    // 解析日志级别
    std::string level = "INFO";
    if (log.find("[WARNING]") != std::string::npos) {
        level = "WARNING";
    } else if (log.find("[ERROR]") != std::string::npos) {
        level = "ERROR";
    } else if (log.find("[DEBUG]") != std::string::npos) {
        level = "DEBUG";
    }

    LogEntry entry;
    entry.message = log;
    entry.level = level;
    entry.time = ImGui::GetTime();

    logs_.push_back(entry);

    // 限制日志数量
    if (logs_.size() > maxLogs_) {
        logs_.erase(logs_.begin());
    }
}

void LogPanel::clear() {
    std::lock_guard<std::mutex> lock(logsMutex_);
    logs_.clear();
}

} // namespace wingman::gui
