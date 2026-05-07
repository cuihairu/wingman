#pragma once

#include "wingman/gui/imgui_backend.hpp"
#include "wingman/gui/config.hpp"
#include "wingman/trigger.hpp"
#include "wingman/script_manager.hpp"
#include "wingman/config.hpp"
#include "wingman/screen.hpp"
#include <memory>
#include <string>
#include <vector>

namespace wingman::gui {

// 前向声明
class TriggerPanel;
class ScriptPanel;
class PreviewPanel;
class LogPanel;

// 主 GUI 应用类
class GuiApp {
public:
    GuiApp();
    ~GuiApp();

    // 禁止拷贝和移动
    GuiApp(const GuiApp&) = delete;
    GuiApp& operator=(const GuiApp&) = delete;
    GuiApp(GuiApp&&) = delete;
    GuiApp& operator=(GuiApp&&) = delete;

    // 初始化和运行
    bool initialize();
    void run();
    void shutdown();

    // 状态查询
    bool isInitialized() const { return initialized_; }

private:
    // 窗口消息处理
    static LRESULT WINAPI WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static GuiApp* s_instance;

    LRESULT handleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 渲染
    void render();
    void renderMainMenu();
    void renderMainDockSpace();

    // 初始化各个面板
    void initPanels();

    // 配置管理
    void loadConfig();
    void saveConfig();
    void applyConfig();

    // 成员变量
    bool initialized_;
    HWND hwnd_;
    int windowWidth_;
    int windowHeight_;

    std::unique_ptr<ImGuiBackend> backend_;

    // 面板
    std::unique_ptr<TriggerPanel> triggerPanel_;
    std::unique_ptr<ScriptPanel> scriptPanel_;
    std::unique_ptr<PreviewPanel> previewPanel_;
    std::unique_ptr<LogPanel> logPanel_;

    // 核心模块
    std::unique_ptr<TriggerManager> triggerManager_;
    std::unique_ptr<ScriptManager> scriptManager_;
    std::unique_ptr<ConfigManager> configManager_;

    // GUI 配置
    GuiConfig guiConfig_;

    // UI 状态
    bool showDemoWindow_;
    bool showTriggerPanel_;
    bool showScriptPanel_;
    bool showPreviewPanel_;
    bool showLogPanel_;
};

// 全局函数：启动 GUI 应用
int RunGuiApplication();

} // namespace wingman::gui
