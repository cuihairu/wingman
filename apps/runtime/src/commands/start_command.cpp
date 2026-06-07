#include "wingman/runtime/commands/start_command.hpp"
#include "wingman/runtime/agent.hpp"
#include "wingman/version.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <memory>

namespace wingman::runtime::commands {

namespace {

// Async-signal-safe: only sets an atomic flag. The main loop polls
// this flag and performs the actual shutdown (spdlog, agent.stop)
// outside the signal handler.
std::atomic<bool> g_shutdownRequested{false};

void signalHandler(int /*signal*/) {
    g_shutdownRequested.store(true);
}

} // anonymous namespace

int startCommand(const std::string& configPath) {
    StartOptions options;
    options.configPath = configPath;
    return startCommand(options);
}

int startCommand(const StartOptions& options) {
    // Initialize logging using spdlog's default logger
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // Create and set a console logger using spdlog factory
    try {
        auto logger = spdlog::stdout_logger_st("wingman");
        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
    } catch (const spdlog::spdlog_ex& ex) {
        // Logger already registered, use existing
        auto existing = spdlog::get("wingman");
        if (existing) {
            spdlog::set_default_logger(existing);
        }
    }

    spdlog::info("Wingman Agent {}", WINGMAN_VERSION);
    spdlog::info("====================");

    // Create Agent
    wingman::runtime::Agent agent;

    // Set signal handlers — only writes to an atomic flag
    g_shutdownRequested.store(false);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif

    // Initialize
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

    // Start
    if (!agent.start()) {
        spdlog::warn("Some modes failed to start, but agent will continue running");
    }

    spdlog::info("Agent is running. Press Ctrl+C to stop.");

    // Main loop — wait on condition variable with 250ms timeout.
    // Responds to shutdown within 250ms instead of 1s, and doesn't
    // need busy-polling.
    std::mutex mtx;
    std::condition_variable cv;

    while (agent.isRunning() && !g_shutdownRequested.load()) {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::milliseconds(250), [] {
            return g_shutdownRequested.load();
        });
    }

    // Cleanup
    if (g_shutdownRequested.load()) {
        spdlog::info("Shutdown signal received, stopping agent...");
    }

    agent.shutdown();

    spdlog::info("Agent stopped. Goodbye!");
    return 0;
}

} // namespace wingman::runtime::commands
