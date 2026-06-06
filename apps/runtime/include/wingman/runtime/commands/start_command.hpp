#pragma once

#include <string>

namespace wingman::runtime::commands {

struct StartOptions {
    std::string configPath = "agent.toml";
    bool forceStandalone = false;
};

/// 启动 Wingman Agent 服务
/// @param configPath 配置文件路径
/// @return 退出码
int startCommand(const std::string& configPath = "agent.toml");
int startCommand(const StartOptions& options);

} // namespace wingman::runtime::commands
