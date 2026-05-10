#include "script_command.hpp"
#include "wingman/lua.hpp"
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
        auto lua = std::make_unique<wingman::lua::LuaState>();

        // 设置命令行参数
        lua->setGlobal("arg", args);

        // 执行脚本
        if (!lua->doString(content)) {
            spdlog::error("Script execution failed: {}", lua->getLastError());
            return 1;
        }

        spdlog::info("Script completed successfully");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Exception while running script: {}", e.what());
        return 1;
    }
}

} // namespace wingman::runtime::commands
