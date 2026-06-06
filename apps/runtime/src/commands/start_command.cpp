#include "wingman/runtime/commands/start_command.hpp"
#include "wingman/runtime/agent.hpp"
#include "wingman/version.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <csignal>
#include <exception>
#include <memory>
#include <thread>

namespace wingman::runtime::commands {

namespace {

Agent* g_agent = nullptr;

void signalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    if (g_agent) {
        g_agent->stop();
    }
}

} // anonymous namespace

int startCommand(const std::string& configPath) {
    StartOptions options;
    options.configPath = configPath;
    return startCommand(options);
}

int startCommand(const StartOptions& options) {
    // 初始化日志
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    spdlog::default_logger()->sinks().push_back(consoleSink);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Wingman Agent {}", WINGMAN_VERSION);
    spdlog::info("====================");

    // 创建 Agent
    wingman::runtime::Agent agent;

    // 设置信号处理
    g_agent = &agent;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif

    // 初始化
    if (options.forceStandalone) {
        AgentConfig config;
        try {
            config = AgentConfig::loadFromFile(options.configPath);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to load config: {}, using default standalone configuration", e.what());
        }
        config.enableRemote = false;
        if (!agent.initialize(config)) {
            spdlog::error("Failed to initialize standalone runtime");
            return 1;
        }
    } else {
        if (!agent.initialize(options.configPath)) {
            spdlog::error("Failed to initialize agent");
            return 1;
        }
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

} // namespace wingman::runtime::commands
