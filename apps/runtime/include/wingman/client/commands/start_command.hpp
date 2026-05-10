#pragma once

#include <string>

namespace wingman::client::commands {

/// 启动 Wingman Agent 服务
/// @param configPath 配置文件路径
/// @return 退出码
int startCommand(const std::string& configPath = "agent.toml");

} // namespace wingman::client::commands
