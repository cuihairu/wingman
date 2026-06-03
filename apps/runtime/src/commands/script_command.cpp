#include "wingman/runtime/commands/script_command.hpp"
#include "wingman/runtime/runtime_context.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>

namespace wingman::runtime::commands {

int scriptCommand(const std::string& scriptPath, const std::vector<std::string>& args) {
    // 检查脚本文件是否存在
    if (!std::filesystem::exists(scriptPath)) {
        spdlog::error("Script file not found: {}", scriptPath);
        return 1;
    }

    spdlog::info("Running script: {}", scriptPath);

    try {
        auto& manager = wingman::runtime::getScriptManager();
        const std::string scriptName = "__cli__:" + std::filesystem::absolute(scriptPath).string();

        wingman::ScriptConfig config;
        config.name = scriptName;
        config.path = scriptPath;
        for (size_t i = 0; i < args.size(); ++i) {
            config.env["arg" + std::to_string(i + 1)] = args[i];
        }

        if (!manager.loadScript(scriptName, scriptPath, config)) {
            spdlog::error("Failed to load script: {}", scriptPath);
            return 1;
        }

        if (!manager.runScript(scriptName)) {
            auto info = manager.getScriptInfo(scriptName);
            spdlog::error("Script execution failed: {}",
                          info && !info->lastError.empty() ? info->lastError : "unknown error");
            manager.unloadScript(scriptName);
            return 1;
        }

        manager.unloadScript(scriptName);
        spdlog::info("Script completed successfully");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Exception while running script: {}", e.what());
        return 1;
    }
}

} // namespace wingman::runtime::commands
