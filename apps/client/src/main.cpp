#include "wingman/client/agent.hpp"
#include "wingman/version.hpp"
#include <iostream>
#include <csignal>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

wingman::client::Agent* g_agent = nullptr;

void signalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
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

    spdlog::info("Agent is running. Press Ctrl+C to stop.");

    // 主循环
    while (agent.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 清理
    agent.shutdown();
    g_agent = nullptr;

    spdlog::info("Agent stopped. Goodbye!");
    return 0;
}
