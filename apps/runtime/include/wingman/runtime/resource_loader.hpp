#pragma once

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <memory>

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
    /// 加载失败时的原因回调（先声明：下面的成员函数签名与默认实参要用它）
    using ErrorCallback = std::function<void(const std::string&)>;

    ResourceLoader();
    ~ResourceLoader();

    /// 检查是否有嵌入的脚本
    bool hasEmbeddedScript() const;

    /// 获取资源信息
    ResourceInfo getResourceInfo() const;

    /// 加载嵌入的脚本
    /// @param password 解密密码；加密资源必须提供，否则加载失败
    /// @return 加载的脚本，如果失败返回 nullopt
    std::optional<LoadedScript> loadScript(const std::string& password = "");

    /// 从一份打包资源字节流加载脚本（不经 PE 容器）
    ///
    /// 与 loadScript() 共用同一条解析/解压/解密/校验实现，差别只在字节来源：
    /// PE 资源读取是 Windows-only（FindResource/LoadResource），把这份实现公开出来，
    /// 非 Windows 上的往返与「口令错必须失败」用例跑的才是生产实现本身，而不是另抄的副本。
    ///
    /// @param resourceData Packer::buildResourceBytes() 产出的完整资源（头部 + 负载）
    /// @param password     解密密码；加密资源必须提供，否则失败
    /// @param errorCallback 可选，失败时收到与 loadScript 一致的原因文本
    static std::optional<LoadedScript> loadScriptFromBytes(const std::vector<uint8_t>& resourceData,
                                                           const std::string& password = "",
                                                           ErrorCallback errorCallback = nullptr);

    /// 获取可执行文件路径
    static std::string getExecutablePath();

    /// 设置错误回调
    void setErrorCallback(ErrorCallback callback);

    /// 检测 Lua 字节码签名
    /// Lua 5.x chunk 以 ESC 'L' 'u' 'a' 开头（第 5 字节为版本号，如 0x54 = 5.4），
    /// LuaJIT 字节码以 ESC 'L' 'J' 开头
    static bool looksLikeLuaBytecode(const std::vector<uint8_t>& data);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    ErrorCallback errorCallback_;
};

} // namespace wingman::runtime
