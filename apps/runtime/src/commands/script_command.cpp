#include "wingman/runtime/commands/script_command.hpp"
#ifdef WINGMAN_HAS_LUA
#include "wingman/lua/lua_engine.hpp"
#endif
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace wingman::runtime::commands {

int scriptCommand(const std::string& scriptPath, const std::vector<std::string>& args) {
    // 检查脚本文件是否存在
    if (!std::filesystem::exists(scriptPath)) {
        spdlog::error("Script file not found: {}", scriptPath);
        return 1;
    }

    spdlog::info("Running script: {}", scriptPath);

    // 读取脚本内容
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
        spdlog::error("Failed to open script file: {}", scriptPath);
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // 创建 Lua 实例并执行
    try {
#ifdef WINGMAN_HAS_LUA
        auto lua = std::make_unique<wingman::lua::LuaEngine>();
        if (!lua->initialize()) {
            spdlog::error("Failed to initialize Lua engine");
            return 1;
        }

        // 设置命令行参数
        // TODO: 传递参数到 Lua
        (void)args;  // 暂时忽略参数

        // 设置错误回调
        std::string lastError;
        lua->setErrorCallback([&lastError](const std::string& err) {
            lastError = err;
        });

        // 执行脚本
        if (!lua->executeString(content)) {
            spdlog::error("Script execution failed: {}", lastError.empty() ? "unknown error" : lastError);
            return 1;
        }

        spdlog::info("Script completed successfully");
        return 0;
#else
        spdlog::error("Script execution requires Lua support");
        return 1;
#endif

    } catch (const std::exception& e) {
        spdlog::error("Exception while running script: {}", e.what());
        return 1;
    }
}

} // namespace wingman::runtime::commands
