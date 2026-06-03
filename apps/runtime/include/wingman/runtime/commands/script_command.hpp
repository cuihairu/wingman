#pragma once

#include <string>
#include <vector>

namespace wingman::runtime::commands {

/// 运行已注册脚本引擎支持的脚本
/// @param scriptPath 脚本文件路径
/// @param args 命令行参数
/// @return 退出码
int scriptCommand(const std::string& scriptPath, const std::vector<std::string>& args);

} // namespace wingman::runtime::commands
