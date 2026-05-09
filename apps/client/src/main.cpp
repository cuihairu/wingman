#include "wingman/client/agent.hpp"
#include "wingman/version.hpp"
#include "wingman/tray.hpp"
#include <iostream>
#include <csignal>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace {

wingman::client::Agent* g_agent = nullptr;
std::shared_ptr<wingman::TrayIcon> g_trayIcon = nullptr;

void signalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    if (g_agent) {
        g_agent->stop();
    }
}

// 退出应用
void quitApplication() {
    if (g_agent) {
        g_agent->stop();
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // 初始化日志
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::default_logger()->sinks().push_back(consoleSink);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Wingman Agent {}", WINGMAN_VERSION);
    spdlog::info("====================");

    // 确定配置文件路径
    std::string configPath = "agent.toml";
    if (argc > 1) {
        configPath = argv[1];
    }

    // 创建 Agent
    wingman::client::Agent agent;

    // 设置信号处理
    g_agent = &agent;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif

    // 初始化
    if (!agent.initialize(configPath)) {
        spdlog::error("Failed to initialize agent");
        return 1;
    }

    // 启动
    if (!agent.start()) {
        spdlog::warn("Some modes failed to start, but agent will continue running");
    }

    // 创建系统托盘图标
    g_trayIcon = wingman::TrayManager::instance().createIcon("main", "Wingman Agent " WINGMAN_VERSION);

    // 添加托盘菜单项
    if (g_trayIcon) {
        g_trayIcon->addItem("status", "状态: 运行中", []() {
            spdlog::info("Agent status: {}", g_agent && g_agent->isRunning() ? "Running" : "Stopped");
        });
        g_trayIcon->addSeparator("sep1");
        g_trayIcon->addItem("quit", "退出", quitApplication);
        g_trayIcon->show();
        spdlog::info("System tray icon created");
    }

    spdlog::info("Agent is running. Press Ctrl+C to stop.");

    // 主循环
    while (agent.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 清理
    if (g_trayIcon) {
        g_trayIcon->hide();
        wingman::TrayManager::instance().removeIcon("main");
        g_trayIcon.reset();
    }
    agent.shutdown();
    g_agent = nullptr;

    spdlog::info("Agent stopped. Goodbye!");
    return 0;
}
