#include "wingman/runtime/resource_loader.hpp"
#include "wingman/runtime/resource_pack.hpp"
#include <spdlog/spdlog.h>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <winuser.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace wingman::runtime {

// ========== ResourceLoader 实现 ==========

namespace {

/// 简化解压（Packer 侧 compressPayload 的逆操作）
std::vector<uint8_t> decompressPayload(const std::vector<uint8_t>& compressed) {
    std::vector<uint8_t> decompressed;
    size_t i = 0;

    while (i < compressed.size()) {
        uint8_t header = compressed[i++];

        if (header & 0x80) {
            // 重复引用
            size_t offset = header & 0x7F;
            size_t count = compressed[i++];

            for (size_t j = 0; j < count; j++) {
                if (decompressed.size() >= offset) {
                    decompressed.push_back(decompressed[decompressed.size() - offset]);
                }
            }
        } else {
            // 原始字节
            size_t count = header;
            for (size_t j = 0; j < count && i < compressed.size(); j++) {
                decompressed.push_back(compressed[i++]);
            }
        }
    }

    spdlog::debug("Decompressed: {} -> {} bytes", compressed.size(), decompressed.size());
    return decompressed;
}

} // namespace

class ResourceLoader::Impl {
public:
    std::string executablePath;
    ResourceInfo resourceInfo;

    // 检测嵌入资源
    bool detectResource() {
#ifdef _WIN32
        // 加载可执行文件
        HMODULE hModule = GetModuleHandleW(NULL);
        if (!hModule) {
            return false;
        }

        // 尝试查找资源
        HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(PACK_PE_RESOURCE_ID), RT_RCDATA);
        if (!hRes) {
            spdlog::debug("No embedded script resource found");
            return false;
        }

        DWORD size = SizeofResource(hModule, hRes);
        if (size < sizeof(PACK_HEADER)) {
            spdlog::warn("Resource too small to be valid");
            return false;
        }

        spdlog::debug("Found embedded resource: {} bytes", size);
        resourceInfo.exists = true;
        return true;
#else
        return false;
#endif
    }

    // 读取资源数据
    std::vector<uint8_t> readResourceData() {
#ifdef _WIN32
        HMODULE hModule = GetModuleHandleW(NULL);
        HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(PACK_PE_RESOURCE_ID), RT_RCDATA);
        if (!hRes) {
            return {};
        }

        DWORD size = SizeofResource(hModule, hRes);
        HGLOBAL hLoaded = LoadResource(hModule, hRes);
        if (!hLoaded) {
            return {};
        }

        void* pData = LockResource(hLoaded);
        if (!pData) {
            return {};
        }

        std::vector<uint8_t> data(static_cast<uint8_t*>(pData),
                                  static_cast<uint8_t*>(pData) + size);
        return data;
#else
        return {};
#endif
    }
};

// ========== 加载核心（PE 与内存字节流共用） ==========

namespace {

/// 打包字节流 → 明文脚本：解析头部、按口令派生密钥、解压/解密、完整性校验。
///
/// 这是整条加载链的唯一实现。loadScript()（读 PE 资源）与 loadScriptFromBytes()
/// （读调用方给的字节）只差字节来源，其余逻辑一字不差——因此 Linux/CI 上跑的
/// 往返与错口令用例，执行的就是生产路径本身。
///
/// @param info 载入过程中就地填充（version/sizes/flags/exists）
/// @throws std::runtime_error 失败原因即消息文本，由调用方转成错误回调
LoadedScript unpackResource(const std::vector<uint8_t>& resourceData,
                            const std::string& password,
                            ResourceInfo& info) {
    if (resourceData.size() < sizeof(PACK_HEADER)) {
        throw std::runtime_error("Resource data too small");
    }

    PACK_HEADER header;
    std::memcpy(&header, resourceData.data(), sizeof(PACK_HEADER));

    if (!hasPackMagic(header)) {
        throw std::runtime_error("Invalid resource magic number");
    }

    if (header.version < PACK_FORMAT_VERSION_LEGACY || header.version > PACK_FORMAT_VERSION) {
        // 明确拒绝未知版本：继续解析只会把「新格式」误读成「数据损坏」
        throw std::runtime_error("Unsupported pack format version " + std::to_string(header.version));
    }

    const bool encrypted = (header.flags & PACK_FLAG_ENCRYPTED) != 0;
    const bool compressed = (header.flags & PACK_FLAG_COMPRESSED) != 0;

    if (encrypted && header.version == PACK_FORMAT_VERSION_LEGACY) {
        // v1 的加密包在构造上不可恢复：密钥是打包时随机生成的一次性密钥，头部只留了
        // sha256(key)。那个年代 Packer::build() 又直接拒绝 encrypt，所以正常产物里
        // 不存在这种包；真遇到就是损坏或伪造，不做任何「猜密钥」的回退。
        throw std::runtime_error(
            "Legacy v1 encrypted pack is unrecoverable (no password-derived key); repack with --encrypt --password");
    }

    if (encrypted && password.empty()) {
        // loadScript() 的 password 形参有默认值 ""，忘了传要在这里明确报出来，
        // 而不是拿空口令去派生一次密钥、最后以「数据损坏」的假象失败。
        throw std::runtime_error("Encrypted resource requires a password");
    }

    spdlog::info("Loading embedded script (v{}, {} bytes)", header.version, header.originalSize);

    std::vector<uint8_t> payload(resourceData.begin() + static_cast<long>(sizeof(PACK_HEADER)),
                                 resourceData.end());
    if (payload.size() != header.compressedSize) {
        throw std::runtime_error("Payload size mismatch: header declares " +
                                 std::to_string(header.compressedSize) + " bytes, resource has " +
                                 std::to_string(payload.size()));
    }

    // packer 的变换顺序是 compress → encrypt，故这里必须 decrypt → decompress
    if (encrypted) {
        PackCryptoParams params;
        if (!readPackCryptoParams(header, params)) {
            throw std::runtime_error("Invalid crypto parameters in pack header");
        }

        const std::vector<uint8_t> key = derivePackKey(password, params);
        if (!packKeyFingerprintMatches(header, key)) {
            // 只是解密前的快速判定（给出一条能区分「口令错」与「数据坏了」的信息）；
            // 真正的认证边界是下面的 GCM tag。
            throw std::runtime_error("Incorrect password");
        }

        payload = wingman::crypt::aesGcmDecrypt(key, packCryptoIv(params), payload);
    }

    if (compressed) {
        payload = decompressPayload(payload);
        spdlog::info("Decompressed to {} bytes", payload.size());
    }

    if (payload.size() != header.originalSize) {
        throw std::runtime_error("Decoded size mismatch: header declares " +
                                 std::to_string(header.originalSize) + " bytes, payload decodes to " +
                                 std::to_string(payload.size()));
    }

    // dataHash 定义在原始明文上（见 resource_pack.hpp），故解压 + 解密之后校验。
    // 非加密包同样走这一步：这条检查在 Linux 上此前是恒真桩，现在是真 SHA-256。
    if (packHasDataHash(header)) {
        const std::vector<uint8_t> expected(header.dataHash, header.dataHash + sizeof(header.dataHash));
        if (sha256Bytes(payload) != expected) {
            throw std::runtime_error("Hash verification failed");
        }
    }

    info.exists = true;
    info.version = header.version;
    info.originalSize = header.originalSize;
    info.compressedSize = header.compressedSize;
    info.encrypted = encrypted;
    info.compressed = compressed;

    LoadedScript script;
    script.data = std::move(payload);
    script.name = "embedded";
    script.isBytecode = ResourceLoader::looksLikeLuaBytecode(script.data);

    spdlog::info("Script loaded successfully: {} bytes", script.data.size());
    return script;
}

} // namespace

// ========== ResourceLoader 公共接口 ==========

ResourceLoader::ResourceLoader()
    : impl_(std::make_unique<Impl>())
{
    impl_->executablePath = getExecutablePath();
    impl_->detectResource();
}

ResourceLoader::~ResourceLoader() = default;

void ResourceLoader::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = std::move(callback);
}

bool ResourceLoader::hasEmbeddedScript() const {
    return impl_->resourceInfo.exists;
}

ResourceInfo ResourceLoader::getResourceInfo() const {
    return impl_->resourceInfo;
}

std::optional<LoadedScript> ResourceLoader::loadScript(const std::string& password) {
    if (!hasEmbeddedScript()) {
        if (errorCallback_) {
            errorCallback_("No embedded script found");
        }
        return std::nullopt;
    }

    try {
        // 读取资源数据（PE 容器 → 字节流，之后与 loadScriptFromBytes 同路）
        std::vector<uint8_t> resourceData = impl_->readResourceData();
        if (resourceData.empty()) {
            if (errorCallback_) {
                errorCallback_("Failed to read resource data");
            }
            return std::nullopt;
        }

        return unpackResource(resourceData, password, impl_->resourceInfo);
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(std::string("Failed to load script: ") + e.what());
        }
        return std::nullopt;
    }
}

std::optional<LoadedScript> ResourceLoader::loadScriptFromBytes(const std::vector<uint8_t>& resourceData,
                                                                const std::string& password,
                                                                ErrorCallback errorCallback) {
    try {
        ResourceInfo info;
        return unpackResource(resourceData, password, info);
    } catch (const std::exception& e) {
        if (errorCallback) {
            errorCallback(e.what());
        }
        return std::nullopt;
    }
}

bool ResourceLoader::looksLikeLuaBytecode(const std::vector<uint8_t>& data) {
    // Lua 5.x 官方 chunk 签名：ESC 'L' 'u' 'a'，随后是版本字节（如 0x54 = 5.4）
    static constexpr uint8_t kLuaSignature[] = {0x1B, 'L', 'u', 'a'};
    // LuaJIT 字节码签名：ESC 'L' 'J'
    static constexpr uint8_t kLuaJitSignature[] = {0x1B, 'L', 'J'};

    if (data.size() < sizeof(kLuaSignature)) {
        return false;
    }
    if (std::memcmp(data.data(), kLuaSignature, sizeof(kLuaSignature)) == 0) {
        return true;
    }
    if (data.size() < sizeof(kLuaJitSignature)) {
        return false;
    }
    return std::memcmp(data.data(), kLuaJitSignature, sizeof(kLuaJitSignature)) == 0;
}

std::string ResourceLoader::getExecutablePath() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    // 转换为窄字符
    int size = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], size, nullptr, nullptr);
        return result;
    }
    return "";
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        return std::string(path, count);
    }
    return "";
#endif
}

} // namespace wingman::runtime
