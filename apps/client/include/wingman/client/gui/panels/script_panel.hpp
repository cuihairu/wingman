#pragma once

#include "wingman/script_manager.hpp"
#include <imgui.h>
#include <string>
#include <vector>
#include <utility>
#include <sstream>

namespace wingman::gui {

// 脚本管理面板
class ScriptPanel {
public:
    explicit ScriptPanel(ScriptManager* manager);
    ~ScriptPanel();

    void render(bool* show);

private:
    void renderScriptList();
    void renderScriptControls();
    void renderScriptOutput();
    void refreshScriptList();
    void addOutput(const std::string& scriptName, const std::string& output);

    ScriptManager* manager_;
    std::string selectedScript_;
    std::vector<ScriptInfo> scriptInfos_;
    int runningCount_;

    // 日志输出 (scriptName, message)
    std::vector<std::pair<std::string, std::string>> outputLines_;
    bool autoScroll_;
    float lastUpdate_;
};

} // namespace wingman::gui
