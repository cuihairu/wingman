#pragma once

#include <string>

namespace wingman::runtime::commands {

struct BuildOptions {
    std::string scriptPath;
    std::string outputPath;
    std::string iconPath;
    bool encrypt = true;
    bool compress = true;
};

/// 构建独立可执行文件
/// @param options 构建选项
/// @return 退出码
int buildCommand(const BuildOptions& options);

} // namespace wingman::runtime::commands
