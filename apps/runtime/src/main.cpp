#include <clasp/clasp.hpp>
#include "wingman/runtime/resource_loader.hpp"
#include "wingman/runtime/commands/start_command.hpp"
#include "wingman/runtime/commands/stop_command.hpp"
#include "wingman/runtime/commands/script_command.hpp"
#include "wingman/runtime/commands/build_command.hpp"
#include "wingman/runtime/commands/serve_command.hpp"
#include "wingman/lua/lua_engine.hpp"
#include "wingman/lua/lua_script_engine.hpp"
#ifdef WINGMAN_ENABLE_PYTHON
#include "wingman/python/python_script_engine.hpp"
#endif
#include "wingman/version.hpp"
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

/// 运行嵌入的脚本（如果存在）
/// @return 如果运行了嵌入脚本返回 true，否则返回 false
bool runEmbeddedScript() {
    wingman::runtime::ResourceLoader loader;

    if (!loader.hasEmbeddedScript()) {
        spdlog::debug("No embedded script found");
        return false;
    }

    spdlog::info("=== Embedded Script Detected ===");

    wingman::runtime::ResourceInfo info = loader.getResourceInfo();
    spdlog::info("Version: {}", info.version);
    spdlog::info("Original Size: {} bytes", info.originalSize);
    spdlog::info("Compressed Size: {} bytes", info.compressedSize);

    // 加载脚本
    auto loadedScript = loader.loadScript();
    if (!loadedScript) {
        spdlog::error("Failed to load embedded script");
        return false;
    }

    spdlog::info("Script loaded: {} bytes", loadedScript->data.size());

    // 创建 Lua 实例并执行
    try {
        auto lua = std::make_unique<wingman::lua::LuaEngine>();
        if (!lua->initialize()) {
            spdlog::error("Failed to initialize Lua engine");
            return true;
        }

        // 设置嵌入脚本标记
        lua->setGlobal("_EMBEDDED", 1);

        // 将数据转换为字符串
        std::string scriptCode(loadedScript->data.begin(), loadedScript->data.end());

        // 设置错误回调
        std::string lastError;
        lua->setErrorCallback([&lastError](const std::string& err) {
            lastError = err;
        });

        // 执行脚本
        if (!lua->executeString(scriptCode)) {
            spdlog::error("Script execution failed: {}", lastError.empty() ? "unknown error" : lastError);
            return true;  // 即使失败也返回 true，因为已经尝试运行
        }

        spdlog::info("Embedded script completed successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception while running embedded script: {}", e.what());
        return true;
    }
}

/// 运行 GUI 模式
int runGuiMode() {
    spdlog::info("Starting GUI mode...");

    // TODO: 启动 ImGui GUI
    // 目前先启动 agent
    return wingman::runtime::commands::startCommand("agent.toml");
}

int main(int argc, char** argv) {
    // 初始化日志
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    spdlog::default_logger()->sinks().push_back(consoleSink);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

    // 注册脚本引擎
    wingman::lua::registerLuaEngine();
#ifdef WINGMAN_ENABLE_PYTHON
    wingman::python::registerPythonEngine();
#endif

    // 首先检查是否有嵌入的脚本
    if (argc == 1) {
        // 无命令行参数
        if (runEmbeddedScript()) {
            return 0;  // 运行了嵌入脚本，直接退出
        }
        // 没有嵌入脚本，启动 GUI
        return runGuiMode();
    }

    // ========== 有命令行参数，使用 Clasp 解析 ==========

    // 根命令 (wingman-runtime)
    clasp::Command rootCmd("wingman-runtime", "Wingman - Game Automation Programmable Control Engine");

    // ========== start 命令 ==========
    clasp::Command startCmd("start", "Start Wingman Agent service");
    startCmd
        .withFlag("--config", "-c", "config", "Configuration file path", std::string("agent.toml"))
        .action([](clasp::Command& cmd, const clasp::Parser& parser, const std::vector<std::string>& args) {
            const auto configPath = parser.getFlag<std::string>("--config");
            return wingman::runtime::commands::startCommand(configPath.empty() ? "agent.toml" : configPath);
        });

    // ========== stop 命令 ==========
    clasp::Command stopCmd("stop", "Stop Wingman Agent service");
    stopCmd.action([](clasp::Command& cmd, const clasp::Parser& parser, const std::vector<std::string>& args) {
        return wingman::runtime::commands::stopCommand();
    });

    // ========== status 命令 ==========
    clasp::Command statusCmd("status", "Show Wingman Agent service status");
    statusCmd.action([](clasp::Command& cmd, const clasp::Parser& parser, const std::vector<std::string>& args) {
        return wingman::runtime::commands::statusCommand();
    });

    // ========== script 命令 ==========
    clasp::Command scriptCmd("script", "Run a Lua script");
    scriptCmd
        .action([](clasp::Command& cmd, const clasp::Parser& parser, const std::vector<std::string>& args) {
            if (args.empty()) {
                std::cerr << "Error: script path is required\n";
                return 1;
            }
            const auto scriptPath = args[0];

            // 收集额外的参数 (位置参数)
            std::vector<std::string> scriptArgs;
            if (args.size() > 1) {
                scriptArgs.assign(args.begin() + 1, args.end());
            }

            return wingman::runtime::commands::scriptCommand(scriptPath, scriptArgs);
        });

    // ========== build 命令 ==========
    clasp::Command buildCmd("build", "Build standalone executable from Lua script");
    buildCmd
        .withFlag("--script", "-s", "script", "Path to the main Lua script", std::string(""))
        .withFlag("--output", "-o", "output", "Output executable path", std::string(""))
        .withFlag("--icon", "-i", "icon", "Path to icon file (.ico)", std::string(""))
        .withFlag("--no-encrypt", "", "Disable script encryption")
        .withFlag("--no-compress", "", "Disable compression")
        .action([](clasp::Command& cmd, const clasp::Parser& parser, const std::vector<std::string>& args) {
            wingman::runtime::commands::BuildOptions options;
            options.scriptPath = parser.getFlag<std::string>("--script");
            options.outputPath = parser.getFlag<std::string>("--output");
            options.iconPath = parser.getFlag<std::string>("--icon");
            options.encrypt = !parser.hasFlag("--no-encrypt");
            options.compress = !parser.hasFlag("--no-compress");

            if (options.scriptPath.empty() || options.outputPath.empty()) {
                std::cerr << "Error: --script and --output are required\n";
                return 1;
            }

            return wingman::runtime::commands::buildCommand(options);
        });

    // ========== serve 命令 ==========
    clasp::Command serveCmd("serve", "Start WebSocket server for UI communication");
    serveCmd
        .withFlag("--port", "-p", "port", "Server port", 8080)
        .withFlag("--host", "-H", "host", "Server host", std::string("127.0.0.1"))
        .action([](clasp::Command& cmd, const clasp::Parser& parser, const std::vector<std::string>& args) {
            const auto host = parser.getFlag<std::string>("--host");
            const auto port = parser.getFlag<int>("--port");
            return wingman::runtime::commands::serveCommand(host.empty() ? "127.0.0.1" : host, port);
        });

    // 注册所有命令
    rootCmd.addCommand(std::move(startCmd));
    rootCmd.addCommand(std::move(stopCmd));
    rootCmd.addCommand(std::move(statusCmd));
    rootCmd.addCommand(std::move(scriptCmd));
    rootCmd.addCommand(std::move(buildCmd));
    rootCmd.addCommand(std::move(serveCmd));

    // 运行
    return rootCmd.run(argc, argv);
}
