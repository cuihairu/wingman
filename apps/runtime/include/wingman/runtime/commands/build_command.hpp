#pragma once

#include <string>

namespace wingman::runtime::commands {

struct BuildOptions {
    std::string scriptPath;
    std::string outputPath;
    std::string iconPath;
    // 保持与 PackerOptions 一致：默认生成可被 ResourceLoader 无口令直接加载的资源。
    bool encrypt = false;
    bool compress = true;
    std::string password;  // 加密口令；仅 encrypt=true 时需要（缺失即拒绝打包）
};

/// 构建独立可执行文件
/// @param options 构建选项
/// @return 退出码
int buildCommand(const BuildOptions& options);

} // namespace wingman::runtime::commands
