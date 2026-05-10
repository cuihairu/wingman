#pragma once

#include <string>
#include <vector>

namespace wingman::client::commands {

/// 运行 Lua 脚本
/// @param scriptPath 脚本文件路径
/// @param args 命令行参数
/// @return 退出码
int scriptCommand(const std::string& scriptPath, const std::vector<std::string>& args);

} // namespace wingman::client::commands
