#pragma once

#include <string>

namespace wingman::gui {

// GUI 配置
struct GuiConfig {
    // 面板显示状态
    bool showTriggerPanel = true;
    bool showScriptPanel = true;
    bool showPreviewPanel = true;
    bool showLogPanel = true;
    bool showDemoWindow = false;

    // 日志面板过滤
    bool logFilterInfo = true;
    bool logFilterWarning = true;
    bool logFilterError = true;
    bool logFilterDebug = false;

    // 预览面板设置
    float previewRefreshInterval = 0.5f;

    // 窗口设置
    int windowWidth = 1280;
    int windowHeight = 720;

    // 主题设置
    int themeIndex = 0;  // 0=深色, 1=浅色, 2=经典

    // 转换为 JSON
    std::string toJson() const;

    // 从 JSON 解析
    static GuiConfig fromJson(const std::string& json);

    // 从 ConfigManager 加载
    static GuiConfig load(class ConfigManager* configMgr);

    // 保存到 ConfigManager
    bool save(class ConfigManager* configMgr) const;
};

} // namespace wingman::gui
