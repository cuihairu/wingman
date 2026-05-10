#pragma once

#include <string>
#include <vector>
#include <memory>

namespace wingman::runtime {

/// 打包选项
struct PackerOptions {
    std::string scriptPath;      // 主脚本路径
    std::string outputPath;      // 输出 EXE 路径
    std::string iconPath;        // 图标路径（可选）
    std::string stubPath;        // Stub 程序路径（wingman-client.exe）
    bool encrypt = true;         // 是否加密
    bool compress = true;        // 是否压缩
    std::string appName = "Wingman App";  // 应用名称
    std::string appVersion = "1.0.0";     // 应用版本
};

/// 打包结果
struct PackerResult {
    bool success = false;
    std::string message;
    std::string outputPath;
};

/// EXE 打包器
/// 将 Lua 脚本嵌入到 EXE 中，创建独立可执行文件
class Packer {
public:
    Packer(const PackerOptions& options);
    ~Packer();

    /// 执行打包
    PackerResult build();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman::runtime
