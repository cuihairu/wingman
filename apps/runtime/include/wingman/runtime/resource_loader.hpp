#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace wingman::runtime {

/// 资源信息
struct ResourceInfo {
    bool exists = false;
    uint32_t version = 0;
    uint64_t originalSize = 0;
    uint64_t compressedSize = 0;
    bool encrypted = false;
    bool compressed = false;
};

/// 加载的脚本数据
struct LoadedScript {
    std::vector<uint8_t> data;
    std::string name;
    bool isBytecode = false;
};

/// 资源加载器
/// 从 PE 资源中加载嵌入的脚本
class ResourceLoader {
public:
    ResourceLoader();
    ~ResourceLoader();

    /// 检查是否有嵌入的脚本
    bool hasEmbeddedScript() const;

    /// 获取资源信息
    ResourceInfo getResourceInfo() const;

    /// 加载嵌入的脚本
    /// @param password 解密密码（如果需要）
    /// @return 加载的脚本，如果失败返回 nullopt
    std::optional<LoadedScript> loadScript(const std::string& password = "");

    /// 获取可执行文件路径
    static std::string getExecutablePath();

    /// 设置错误回调
    using ErrorCallback = std::function<void(const std::string&)>;
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = std::move(callback); }

private:
    std::string executablePath_;
    ResourceInfo resourceInfo_;
    ErrorCallback errorCallback_;

    // 从 PE 资源读取数据
    std::vector<uint8_t> readResourceData();

    // 解析资源头部
    bool parseResourceHeader(const std::vector<uint8_t>& data, ResourceInfo& info, std::vector<uint8_t>& payload);

    // 解密数据
    std::vector<uint8_t> decryptData(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key);

    // 解压数据
    std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressed);

    // 验证哈希
    bool verifyHash(const std::vector<uint8_t>& data, const std::vector<uint8_t>& expectedHash);
};

} // namespace wingman::runtime
