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
    // 资源加密加载链路尚未实现，默认必须生成可直接加载的未加密资源。
    bool encrypt = false;        // 是否加密
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

    // 内部辅助方法
    std::vector<uint8_t> processScript();
    std::vector<uint8_t> compileToBytecode(const std::string& source);
    std::vector<uint8_t> encryptData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> compressData(const std::vector<uint8_t>& data);
    bool copyStub();
    bool embedResource(const std::vector<uint8_t>& data);
    bool replaceIcon();
    bool setVersionInfo();
    std::vector<uint8_t> generateKey();
    std::vector<uint8_t> calculateHash(const std::vector<uint8_t>& data);
};

} // namespace wingman::runtime
