#include "wingman/runtime/commands/serve_command.hpp"
#include "wingman/runtime/controllers/websocket.hpp"
#include "wingman/version.hpp"
#include <drogon/drogon.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>
#include <cstdlib>

namespace wingman::runtime::commands {

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down server...", signal);
    g_running = false;
}

} // anonymous namespace

int serveCommand(const std::string& host, int port) {
    // 初始化日志
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    spdlog::default_logger()->sinks().push_back(consoleSink);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    spdlog::info("Wingman WebSocket Server {}", WINGMAN_VERSION);
    spdlog::info("============================");

    // 设置信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif

    try {
        // 获取当前可执行文件目录，用于查找配置文件
        std::string configPath = "config.json";

        // 检查环境变量
        if (const char* envPath = std::getenv("WINGMAN_CONFIG")) {
            configPath = envPath;
        }

        // 检查配置文件是否存在
        if (!std::filesystem::exists(configPath)) {
            spdlog::warn("Config file not found: {}, using defaults", configPath);

            // 使用程序化配置
            drogon::app().setLogPath("./logs")
                .setLogLevel(trantor::Logger::kInfo)
                .addListener(host, port)
                .setThreadNum(4)
                .setDocumentRoot("./static")
                .setIdleConnectionTimeout(60)
                .setKeepaliveRequestsNumber(1000)
                // .setMaxBodySize(10 * 1024 * 1024)  // 10MB - not available in this version
                .setMaxConnectionNumPerIP(0);  // 无限制

        } else {
            spdlog::info("Loading config from: {}", configPath);
            drogon::app().loadConfigFile(configPath);
        }

        spdlog::info("WebSocket server starting on ws://{}:{}", host, port);
        spdlog::info("WebSocket endpoint: ws://{}:{}/ws", host, port);

        // 注册信号处理器，用于优雅退出
        drogon::app().registerBeginningAdvice([]() {
            spdlog::info("Drogon server started successfully");
        });

        drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr& req,
                                                   drogon::AdviceCallback&& acb,
                                                   drogon::AdviceChainCallback&& accb) {
            spdlog::debug("[{}] {}", req->methodString(), req->path());
            accb();
        });

        // 启动服务器（阻塞模式）
        drogon::app().run();

        spdlog::info("Server stopped gracefully");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Server error: {}", e.what());
        return 1;
    }
}

} // namespace wingman::runtime::commands
