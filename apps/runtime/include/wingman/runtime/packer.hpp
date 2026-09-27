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
    // 默认仍生成未加密资源：加密需要口令，而口令不可能在运行时凭空出现，
    // 因此「加密打包」必须是显式选择（encrypt=true 且 password 非空）。
    bool encrypt = false;        // 是否加密（true 时 password 必须非空，否则 build 失败）
    bool compress = true;        // 是否压缩
    std::string password;        // 加密口令（仅 encrypt=true 时使用；不落盘、不入产物）
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

    /// 生成打包资源字节流（PACK_HEADER + payload），不落盘、不依赖 PE。
    ///
    /// build() 的 PE 嵌入只是把这份字节流塞进 RCDATA 资源；单列出来是为了
    /// ① 让「打包→加载」往返在非 Windows 上可测可跑（PE 写入本身是 Windows-only），
    /// ② 让写侧与读侧共用同一份字节语义（格式定义见 resource_pack.hpp）。
    /// 明文（不加密）产物同样写当前版本，见 resource_pack.hpp 的版本语义说明。
    ///
    /// @param scriptData 原始脚本字节（dataHash 基于它；负载变换顺序 compress → encrypt）
    /// @param encrypt    是否加密；为 true 时 password 必须非空
    /// @param password   加密口令（经 PBKDF2 派生密钥，不写入产物）
    /// @throws std::runtime_error 加密口令为空、或加密参数/密钥派生失败
    static std::vector<uint8_t> buildResourceBytes(const std::vector<uint8_t>& scriptData,
                                                   bool encrypt,
                                                   bool compress,
                                                   const std::string& password);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    // 内部辅助方法
    std::vector<uint8_t> processScript();
    std::vector<uint8_t> compileToBytecode(const std::string& source);
    bool copyStub();
    bool embedResource(const std::vector<uint8_t>& data);
    bool replaceIcon();
    bool setVersionInfo();
};

} // namespace wingman::runtime
