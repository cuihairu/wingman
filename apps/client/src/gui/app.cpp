#include "wingman/client/gui/app.hpp"
#include "wingman/client/gui/panels/trigger_panel.hpp"
#include "wingman/client/gui/panels/script_panel.hpp"
#include "wingman/client/gui/panels/preview_panel.hpp"
#include "wingman/client/gui/panels/log_panel.hpp"
#include "wingman/client/gui/spdlog_sink.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <imgui.h>
#include <Windows.h>

namespace wingman::gui {

// 静态成员
GuiApp* GuiApp::s_instance = nullptr;

// ========== GuiApp 实现 ==========

GuiApp::GuiApp()
    : initialized_(false)
    , hwnd_(nullptr)
    , windowWidth_(1280)
    , windowHeight_(720)
    , showDemoWindow_(false)
    , showTriggerPanel_(true)
    , showScriptPanel_(true)
    , showPreviewPanel_(true)
    , showLogPanel_(true)
{
    s_instance = this;

    // 创建核心模块
    triggerManager_ = std::make_unique<TriggerManager>();
    scriptManager_ = std::make_unique<ScriptManager>();
    configManager_ = std::make_unique<ConfigManager>("config");
}

GuiApp::~GuiApp() {
    shutdown();
    s_instance = nullptr;
}

bool GuiApp::initialize() {
    if (initialized_) return true;

    // 创建窗口
    hwnd_ = CreateImGuiWindow(L"Wingman GUI", windowWidth_, windowHeight_, WindowProc);
    if (!hwnd_) {
        spdlog::error("Failed to create GUI window");
        return false;
    }

    // 创建后端
    backend_ = std::make_unique<ImGuiBackend>();
    if (!backend_->initialize(hwnd_, windowWidth_, windowHeight_)) {
        spdlog::error("Failed to initialize ImGui backend");
        return false;
    }

    // 初始化面板
    initPanels();

    // 集成 spdlog 到 LogPanel
    auto logSink = std::make_shared<ImGuiLogSink>(logPanel_.get());
    logSink->set_level(spdlog::level::debug);
    spdlog::default_logger()->sinks().push_back(logSink);

    // 显示窗口
    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    // 加载配置
    try {
        configManager_->load();
        loadConfig();
        applyConfig();
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load config: {}", e.what());
    }

    initialized_ = true;
    spdlog::info("Wingman GUI initialized successfully");
    return true;
}

void GuiApp::run() {
    if (!initialized_) return;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        // 渲染
        render();
    }
}

void GuiApp::shutdown() {
    if (!initialized_) return;

    // 保存配置
    try {
        saveConfig();
        configManager_->save();
    } catch (const std::exception& e) {
        spdlog::warn("Failed to save config: {}", e.what());
    }

    // 清理面板
    triggerPanel_.reset();
    scriptPanel_.reset();
    previewPanel_.reset();
    logPanel_.reset();

    // 清理后端
    backend_.reset();

    // 清理窗口
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    // 清理核心模块
    triggerManager_.reset();
    scriptManager_.reset();
    configManager_.reset();

    initialized_ = false;
}

LRESULT WINAPI GuiApp::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (s_instance) {
        return s_instance->handleWindowMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT GuiApp::handleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // ImGui 处理
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE: {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            if (width > 0 && height > 0 && backend_) {
                backend_->resize(width, height);
                windowWidth_ = width;
                windowHeight_ = height;
            }
            return 0;
        }

        case WM_SYSCOMMAND: {
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_DPICHANGED: {
            if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports) {
                const float dpi = HIWORD(wParam);
                ImGui::GetIO().DisplayFramebufferScale = ImVec2(dpi / 96.0f, dpi / 96.0f);
            }
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void GuiApp::render() {
    if (!backend_) return;

    // 开始新帧
    backend_->newFrame();

    // 渲染主界面
    renderMainDockSpace();
    renderMainMenu();

    // 渲染各个面板
    if (showDemoWindow_) {
        ImGui::ShowDemoWindow(&showDemoWindow_);
    }

    if (showTriggerPanel_ && triggerPanel_) {
        triggerPanel_->render(&showTriggerPanel_);
    }

    if (showScriptPanel_ && scriptPanel_) {
        scriptPanel_->render(&showScriptPanel_);
    }

    if (showPreviewPanel_ && previewPanel_) {
        previewPanel_->render(&showPreviewPanel_);
    }

    if (showLogPanel_ && logPanel_) {
        logPanel_->render(&showLogPanel_);
    }

    // 提交渲染
    backend_->render();
}

void GuiApp::renderMainMenu() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("文件")) {
            if (ImGui::MenuItem("退出", "Alt+F4")) {
                PostMessage(hwnd_, WM_CLOSE, 0, 0);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("视图")) {
            ImGui::MenuItem("触发器面板", nullptr, &showTriggerPanel_);
            ImGui::MenuItem("脚本面板", nullptr, &showScriptPanel_);
            ImGui::MenuItem("预览面板", nullptr, &showPreviewPanel_);
            ImGui::MenuItem("日志面板", nullptr, &showLogPanel_);
            ImGui::Separator();
            ImGui::MenuItem("Demo 窗口", nullptr, &showDemoWindow_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("工具")) {
            if (ImGui::MenuItem("重新加载配置")) {
                try {
                    configManager_->load();
                    loadConfig();
                    applyConfig();
                    spdlog::info("Config reloaded");
                } catch (const std::exception& e) {
                    spdlog::error("Failed to reload config: {}", e.what());
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("主题")) {
                if (ImGui::MenuItem("深色", nullptr, guiConfig_.themeIndex == 0)) {
                    guiConfig_.themeIndex = 0;
                    ImGui::StyleColorsDark();
                }
                if (ImGui::MenuItem("浅色", nullptr, guiConfig_.themeIndex == 1)) {
                    guiConfig_.themeIndex = 1;
                    ImGui::StyleColorsLight();
                }
                if (ImGui::MenuItem("经典", nullptr, guiConfig_.themeIndex == 2)) {
                    guiConfig_.themeIndex = 2;
                    ImGui::StyleColorsClassic();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("帮助")) {
            if (ImGui::MenuItem("关于")) {
                ImGui::OpenPopup("About");
            }
            ImGui::EndMenu();
        }

        // 关于对话框
        if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Wingman GUI");
            ImGui::Separator();
            ImGui::Text("Game Automation Programmable Control Engine");
            ImGui::Separator();
            ImGui::Text("版本: 0.1.0");
            if (ImGui::Button("关闭")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndMainMenuBar();
    }
}

void GuiApp::renderMainDockSpace() {
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    ImGui::End();
}

void GuiApp::initPanels() {
    triggerPanel_ = std::make_unique<TriggerPanel>(triggerManager_.get());
    scriptPanel_ = std::make_unique<ScriptPanel>(scriptManager_.get());
    previewPanel_ = std::make_unique<PreviewPanel>();
    logPanel_ = std::make_unique<LogPanel>();

    // 设置配置指针到各个面板
    previewPanel_->setConfig(&guiConfig_);
    logPanel_->setConfig(&guiConfig_);
}

void GuiApp::loadConfig() {
    guiConfig_ = GuiConfig::load(configManager_.get());
    spdlog::info("GUI config loaded");
}

void GuiApp::saveConfig() {
    // 保存当前 UI 状态到配置
    guiConfig_.showTriggerPanel = showTriggerPanel_;
    guiConfig_.showScriptPanel = showScriptPanel_;
    guiConfig_.showPreviewPanel = showPreviewPanel_;
    guiConfig_.showLogPanel = showLogPanel_;
    guiConfig_.showDemoWindow = showDemoWindow_;
    guiConfig_.windowWidth = windowWidth_;
    guiConfig_.windowHeight = windowHeight_;

    if (guiConfig_.save(configManager_.get())) {
        spdlog::info("GUI config saved");
    }
}

void GuiApp::applyConfig() {
    showTriggerPanel_ = guiConfig_.showTriggerPanel;
    showScriptPanel_ = guiConfig_.showScriptPanel;
    showPreviewPanel_ = guiConfig_.showPreviewPanel;
    showLogPanel_ = guiConfig_.showLogPanel;
    showDemoWindow_ = guiConfig_.showDemoWindow;

    // 应用主题
    switch (guiConfig_.themeIndex) {
        case 0:
            ImGui::StyleColorsDark();
            break;
        case 1:
            ImGui::StyleColorsLight();
            break;
        case 2:
            ImGui::StyleColorsClassic();
            break;
    }
}

// ========== 全局函数 ==========

int RunGuiApplication() {
    auto app = std::make_unique<GuiApp>();

    if (!app->initialize()) {
        spdlog::error("Failed to initialize GUI application");
        return 1;
    }

    app->run();
    return 0;
}

} // namespace wingman::gui
