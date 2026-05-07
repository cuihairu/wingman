#include "wingman/client/gui/panels/script_panel.hpp"
#include "wingman/client/gui/file_dialog.hpp"
#include <spdlog/spdlog.h>
#include <imgui.h>
#include <chrono>

namespace wingman::gui {

// 脚本状态转字符串
static const char* scriptStateToString(ScriptState state) {
    switch (state) {
        case ScriptState::unloaded: return "未加载";
        case ScriptState::loaded: return "已加载";
        case ScriptState::running: return "运行中";
        case ScriptState::paused: return "已暂停";
        case ScriptState::error: return "错误";
        default: return "未知";
    }
}

// 脚本状态颜色
static ImVec4 scriptStateColor(ScriptState state) {
    switch (state) {
        case ScriptState::unloaded: return ImVec4(0.5f, 0.5f, 0.5f, 1);
        case ScriptState::loaded: return ImVec4(0, 0.5f, 1, 1);
        case ScriptState::running: return ImVec4(0, 1, 0, 1);
        case ScriptState::paused: return ImVec4(1, 0.8f, 0, 1);
        case ScriptState::error: return ImVec4(1, 0.2f, 0.2f, 1);
        default: return ImVec4(0.7f, 0.7f, 0.7f, 1);
    }
}

ScriptPanel::ScriptPanel(ScriptManager* manager)
    : manager_(manager)
    , autoScroll_(true)
    , lastUpdate_(0)
{
    // 设置输出回调
    manager_->setOutputCallback([this](const std::string& scriptName, const std::string& output) {
        addOutput(scriptName, output);
    });

    // 初始化时加载脚本列表
    refreshScriptList();
}

ScriptPanel::~ScriptPanel() {
    // 清除输出回调
    if (manager_) {
        manager_->setOutputCallback(nullptr);
    }
}

void ScriptPanel::render(bool* show) {
    if (!ImGui::Begin("脚本", show)) {
        ImGui::End();
        return;
    }

    // 工具栏
    if (ImGui::Button("刷新列表")) {
        refreshScriptList();
    }
    ImGui::SameLine();
    if (ImGui::Button("加载脚本...")) {
        if (auto file = OpenFileDialog("选择 Lua 脚本", "Lua Files (*.lua)|*.lua|All Files (*.*)|*.*||")) {
            try {
                // 从文件路径提取脚本名称
                std::string scriptName = file.value();
                size_t lastSlash = scriptName.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    scriptName = scriptName.substr(lastSlash + 1);
                }
                size_t lastDot = scriptName.find_last_of('.');
                if (lastDot != std::string::npos) {
                    scriptName = scriptName.substr(0, lastDot);
                }

                manager_->loadScript(scriptName, file.value());
                spdlog::info("Loaded script: {} from {}", scriptName, file.value());
                refreshScriptList();
            } catch (const std::exception& e) {
                spdlog::error("Failed to load script: {}", e.what());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("清除输出")) {
        outputLines_.clear();
    }

    ImGui::SameLine();
    ImGui::Text("总计: %zu | 运行中: %d", scriptInfos_.size(), runningCount_);

    ImGui::Separator();

    // 分割窗口：上部为脚本列表和控制，下部为输出
    float topHeight = ImGui::GetContentRegionAvail().y * 0.5f;
    ImGui::BeginChild("ScriptList", ImVec2(0, topHeight), true);

    renderScriptList();
    renderScriptControls();

    ImGui::EndChild();

    ImGui::Separator();

    // 脚本输出
    renderScriptOutput();

    ImGui::End();
}

void ScriptPanel::renderScriptList() {
    // 定期刷新状态（每秒一次）
    float currentTime = ImGui::GetTime();
    if (currentTime - lastUpdate_ > 1.0f) {
        refreshScriptList();
        lastUpdate_ = currentTime;
    }

    if (ImGui::BeginTable("ScriptTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("脚本名");
        ImGui::TableSetupColumn("路径");
        ImGui::TableSetupColumn("状态");
        ImGui::TableSetupColumn("操作");
        ImGui::TableHeadersRow();

        // 显示脚本列表
        for (const auto& info : scriptInfos_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            bool isSelected = selectedScript_ == info.config.name;
            if (ImGui::Selectable(info.config.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedScript_ = info.config.name;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", info.config.path.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(scriptStateColor(info.state), "%s", scriptStateToString(info.state));

            ImGui::TableSetColumnIndex(3);
            char runId[64], stopId[64], unloadId[64];
            snprintf(runId, sizeof(runId), "运行##%s", info.config.name.c_str());
            snprintf(stopId, sizeof(stopId), "停止##%s", info.config.name.c_str());
            snprintf(unloadId, sizeof(unloadId), "卸载##%s", info.config.name.c_str());

            if (info.state == ScriptState::running || info.state == ScriptState::paused) {
                if (ImGui::SmallButton(stopId)) {
                    if (info.state == ScriptState::paused) {
                        manager_->resumeScript(info.config.name);
                    } else {
                        manager_->pauseScript(info.config.name);
                    }
                }
            } else {
                if (ImGui::SmallButton(runId)) {
                    manager_->runScript(info.config.name);
                    spdlog::info("Running script: {}", info.config.name);
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(unloadId)) {
                manager_->unloadScript(info.config.name);
                spdlog::info("Unloaded script: {}", info.config.name);
            }
        }

        ImGui::EndTable();
    }
}

void ScriptPanel::renderScriptControls() {
    if (!selectedScript_.empty()) {
        ImGui::Separator();

        // 查找选中的脚本信息
        const ScriptInfo* selectedInfo = nullptr;
        for (const auto& info : scriptInfos_) {
            if (info.config.name == selectedScript_) {
                selectedInfo = &info;
                break;
            }
        }

        if (selectedInfo) {
            ImGui::Text("选中: %s", selectedScript_.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", selectedInfo->config.path.c_str());

            ImGui::Separator();

            // 根据状态显示不同按钮
            if (selectedInfo->state == ScriptState::running) {
                if (ImGui::Button("暂停")) {
                    manager_->pauseScript(selectedScript_);
                }
                ImGui::SameLine();
                if (ImGui::Button("停止")) {
                    manager_->stopScript(selectedScript_);
                }
            } else if (selectedInfo->state == ScriptState::paused) {
                if (ImGui::Button("继续")) {
                    manager_->resumeScript(selectedScript_);
                }
                ImGui::SameLine();
                if (ImGui::Button("停止")) {
                    manager_->stopScript(selectedScript_);
                }
            } else {
                if (ImGui::Button("运行")) {
                    manager_->runScript(selectedScript_);
                }
                ImGui::SameLine();
                if (ImGui::Button("重新加载")) {
                    manager_->reloadScript(selectedScript_);
                }
            }

            // 显示错误信息
            if (selectedInfo->state == ScriptState::error && !selectedInfo->lastError.empty()) {
                ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "错误: %s", selectedInfo->lastError.c_str());
            }

            // 热加载选项
            bool autoReload = selectedInfo->config.autoReload;
            if (ImGui::Checkbox("自动重载", &autoReload)) {
                manager_->setAutoReload(selectedScript_, autoReload);
            }
        }
    } else {
        ImGui::TextDisabled("未选择脚本");
    }
}

void ScriptPanel::renderScriptOutput() {
    ImGui::Text("输出:");
    ImGui::SameLine();
    ImGui::Checkbox("自动滚动", &autoScroll_);
    ImGui::SameLine();
    ImGui::TextDisabled("(共 %zu 条)", outputLines_.size());

    ImGui::BeginChild("ScriptOutput", ImVec2(0, 0), true);
    for (const auto& line : outputLines_) {
        // 根据脚本名设置颜色
        if (line.first == selectedScript_) {
            ImGui::TextColored(ImVec4(1, 1, 0.8f, 1), "[%s] %s", line.first.c_str(), line.second.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "[%s] %s", line.first.c_str(), line.second.c_str());
        }
    }
    if (autoScroll_) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void ScriptPanel::refreshScriptList() {
    scriptInfos_ = manager_->getAllScriptInfos();
    runningCount_ = 0;
    for (const auto& info : scriptInfos_) {
        if (info.state == ScriptState::running) {
            runningCount_++;
        }
    }
}

void ScriptPanel::addOutput(const std::string& scriptName, const std::string& output) {
    // 按行分割输出
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty()) {
            outputLines_.push_back({scriptName, line});
        }
    }

    // 限制输出行数
    while (outputLines_.size() > 1000) {
        outputLines_.erase(outputLines_.begin());
    }
}

} // namespace wingman::gui
