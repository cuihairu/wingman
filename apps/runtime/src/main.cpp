#include "wingman/runtime/cli.hpp"
#include "wingman/runtime/event_log_sink.hpp"
#include "wingman/runtime/resource_loader.hpp"
#include "wingman/runtime/commands/start_command.hpp"
#include "wingman/script/module_registry.hpp"
#include "wingman/script/script_engine_factory.hpp"
#ifdef WINGMAN_HAS_LUA
#include "wingman/lua/lua_script_engine.hpp"
#endif
#ifdef WINGMAN_ENABLE_PYTHON
#include "wingman/python/python_script_engine.hpp"
#endif
#include "wingman/version.hpp"
#include <memory>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace {

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

    auto loadedScript = loader.loadScript();
    if (!loadedScript) {
        spdlog::error("Failed to load embedded script");
        return false;
    }

    spdlog::info("Script loaded: {} bytes", loadedScript->data.size());

    try {
#ifdef WINGMAN_HAS_LUA
        auto engine = wingman::script::ScriptEngineFactory::instance().createEngine("lua");
        if (!engine) {
            spdlog::error("Lua script engine is not registered");
            return true;
        }

        if (!engine->initialize()) {
            spdlog::error("Failed to initialize Lua engine: {}", engine->getLastError());
            return true;
        }

        wingman::script::modules::registerAllModules(*engine);
        engine->setGlobal("_EMBEDDED", wingman::script::ScriptValue::fromInt(1));

        std::string scriptCode(loadedScript->data.begin(), loadedScript->data.end());
        if (!engine->executeString(scriptCode)) {
            spdlog::error("Script execution failed: {}", engine->getLastError().empty() ? "unknown error" : engine->getLastError());
            return true;
        }

        spdlog::info("Embedded script completed successfully");
        return true;
#else
        spdlog::error("Embedded script execution requires Lua support");
        return true;
#endif
    } catch (const std::exception& e) {
        spdlog::error("Exception while running embedded script: {}", e.what());
        return true;
    }
}

int runGuiMode() {
    spdlog::info("Starting GUI mode...");
    return wingman::runtime::commands::startCommand("agent.toml");
}

} // namespace

int main(int argc, char** argv) {
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    spdlog::default_logger()->sinks().push_back(consoleSink);
    // 将运行时日志转发到本地 IPC 事件缓冲，供 GUI 实时日志面板轮询拉取
    spdlog::default_logger()->sinks().push_back(wingman::runtime::createEventLogSink());
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");

#ifdef WINGMAN_HAS_LUA
    wingman::lua::registerLuaEngine();
#endif
#ifdef WINGMAN_ENABLE_PYTHON
    wingman::python::registerPythonEngine();
#endif

    if (argc == 1) {
        if (runEmbeddedScript()) {
            return 0;
        }
        return runGuiMode();
    }

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    return wingman::runtime::dispatchCommand(args);
}
