#pragma once

#include <string>
#include <vector>
#include <filesystem>

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
    PackerOptions options_;

    // 步骤1: 读取并处理脚本
    std::vector<uint8_t> processScript();

    // 步骤2: 可选：编译为字节码
    std::vector<uint8_t> compileToBytecode(const std::string& source);

    // 步骤3: 加密数据
    std::vector<uint8_t> encryptData(const std::vector<uint8_t>& data);

    // 步骤4: 压缩数据
    std::vector<uint8_t> compressData(const std::vector<uint8_t>& data);

    // 步骤5: 复制 stub 程序
    bool copyStub();

    // 步骤6: 嵌入资源
    bool embedResource(const std::vector<uint8_t>& data);

    // 步骤7: 替换图标
    bool replaceIcon();

    // 步骤8: 设置版本信息
    bool setVersionInfo();

    // 生成随机密钥
    std::vector<uint8_t> generateKey();

    // 计算 SHA256 哈希
    std::vector<uint8_t> calculateHash(const std::vector<uint8_t>& data);
};

} // namespace wingman::runtime
