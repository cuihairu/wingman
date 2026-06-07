#include "wingman/runtime/resource_loader.hpp"
#include <spdlog/spdlog.h>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <winuser.h>
#include <wincrypt.h>

// Crypto handle wrappers for RAII (matching packer.cpp)
struct CryptoProviderHandle {
    HCRYPTPROV value = 0;
    ~CryptoProviderHandle() {
        if (value) {
            CryptReleaseContext(value, 0);
        }
    }
    HCRYPTPROV* out() { return &value; }
    operator HCRYPTPROV() const { return value; }
};

struct CryptoHashHandle {
    HCRYPTHASH value = 0;
    ~CryptoHashHandle() {
        if (value) {
            CryptDestroyHash(value);
        }
    }
    HCRYPTHASH* out() { return &value; }
    operator HCRYPTHASH() const { return value; }
};

struct CryptoKeyHandle {
    HCRYPTKEY value = 0;
    ~CryptoKeyHandle() {
        if (value) {
            CryptDestroyKey(value);
        }
    }
    HCRYPTKEY* out() { return &value; }
    operator HCRYPTKEY() const { return value; }
};
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace wingman::runtime {

// ========== 资源格式定义 ==========
constexpr const char* RESOURCE_ID = "WM_SCRIPT";  // 资源类型

// 打包头部（与 packer.cpp 中的定义一致）
struct PACK_HEADER {
    uint8_t magic[4];           // "WMSP"
    uint32_t version;           // 版本号
    uint32_t flags;             // 标志位
    uint64_t originalSize;      // 原始大小
    uint64_t compressedSize;    // 压缩后大小
    uint8_t keyHash[32];        // 加密密钥
    uint8_t dataHash[32];       // 数据哈希
    uint8_t reserved[64];       // 保留
};

// ========== 标志位定义（与 packer.cpp 一致） ==========
constexpr uint32_t PACK_FLAG_ENCRYPTED = 0x01;  // 数据已加密
constexpr uint32_t PACK_FLAG_COMPRESSED = 0x02;  // 数据已压缩


// ========== ResourceLoader 实现 ==========

class ResourceLoader::Impl {
public:
    std::string executablePath;
    ResourceInfo resourceInfo;
    ResourceLoader::ErrorCallback errorCallback;

    // 检测嵌入资源
    bool detectResource() {
#ifdef _WIN32
        std::wstring pathW;
        pathW.assign(executablePath.begin(), executablePath.end());

        // 加载可执行文件
        HMODULE hModule = GetModuleHandleW(NULL);
        if (!hModule) {
            return false;
        }

        // 尝试查找资源
        HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(100), RT_RCDATA);
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
        HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(100), RT_RCDATA);
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

    // 简单解压（LZ4 风格的逆操作）
    std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressed) {
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

    // AES 解密（与 packer.cpp 的 aesEncrypt 匹配）
    std::vector<uint8_t> decryptData(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key) {
#ifdef _WIN32
        CryptoProviderHandle hProv;
        CryptoKeyHandle hKey;
        CryptoHashHandle hHash;

        // 获取加密服务提供者
        if (!CryptAcquireContextW(hProv.out(), NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire crypto context for decryption");
        }

        // 创建哈希对象
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, hHash.out())) {
            throw std::runtime_error("Failed to create hash for decryption");
        }

        // 导入密钥
        if (!CryptHashData(hHash, key.data(), static_cast<DWORD>(key.size()), 0)) {
            throw std::runtime_error("Failed to hash key for decryption");
        }

        // 从哈希创建密钥
        struct AES_KEY_BLOB {
            BLOBHEADER header;
            DWORD keySize;
            BYTE keyBytes[32];
        } keyBlob;

        keyBlob.header.bType = PLAINTEXTKEYBLOB;
        keyBlob.header.bVersion = 2;
        keyBlob.header.reserved = 0;
        keyBlob.header.aiKeyAlg = CALG_AES_256;
        keyBlob.keySize = 32;
        std::memcpy(keyBlob.keyBytes, key.data(), std::min(key.size(), size_t(32)));

        if (!CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob),
                           sizeof(AES_KEY_BLOB), 0, 0, hKey.out())) {
            throw std::runtime_error("Failed to import key for decryption");
        }

        // 准备解密缓冲区
        std::vector<uint8_t> decrypted = encrypted;
        DWORD dataLen = static_cast<DWORD>(encrypted.size());

        // 解密（必须与加密使用相同的模式）
        if (!CryptDecrypt(hKey, 0, TRUE, 0, decrypted.data(), &dataLen)) {
            throw std::runtime_error("Failed to decrypt data");
        }

        // 调整到实际解密后的大小（移除 PKCS7 填充）
        decrypted.resize(dataLen);
        return decrypted;
#else
        // 非 Windows 平台抛出错误而不是使用不安全的 XOR
        (void)encrypted;
        (void)key;
        throw std::runtime_error("AES decryption is not supported on this platform");
#endif
    }

    // 验证哈希
    bool verifyHash(const std::vector<uint8_t>& data, const std::vector<uint8_t>& expectedHash) {
#ifdef _WIN32
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        std::vector<uint8_t> hash(32);

        if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return false;
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return false;
        }

        if (!CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }

        DWORD hashLen = 32;
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        return std::memcmp(hash.data(), expectedHash.data(), 32) == 0;
#else
        // 简单验证
        return true;
#endif
    }
};

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
    impl_->errorCallback = errorCallback_;
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
        // 读取资源数据
        std::vector<uint8_t> resourceData = impl_->readResourceData();
        if (resourceData.empty()) {
            if (errorCallback_) {
                errorCallback_("Failed to read resource data");
            }
            return std::nullopt;
        }

        // 解析头部
        if (resourceData.size() < sizeof(PACK_HEADER)) {
            if (errorCallback_) {
                errorCallback_("Resource data too small");
            }
            return std::nullopt;
        }

        PACK_HEADER header;
        std::memcpy(&header, resourceData.data(), sizeof(PACK_HEADER));

        // 验证魔数
        if (std::memcmp(header.magic, "WMSP", 4) != 0) {
            if (errorCallback_) {
                errorCallback_("Invalid resource magic number");
            }
            return std::nullopt;
        }

        spdlog::info("Loading embedded script (v{}, {} bytes)",
                    header.version, header.originalSize);

        // 提取负载数据
        std::vector<uint8_t> payload(
            resourceData.begin() + sizeof(PACK_HEADER),
            resourceData.end()
        );

        // 解压（必须先解压，逆转 packer 的 compress → encrypt 顺序）
        if (header.flags & PACK_FLAG_COMPRESSED) {
            try {
                payload = impl_->decompressData(payload);
                impl_->resourceInfo.compressed = true;
                spdlog::info("Decompressed to {} bytes", payload.size());
            } catch (const std::exception& e) {
                if (errorCallback_) {
                    errorCallback_(std::string("Decompression failed: ") + e.what());
                }
                return std::nullopt;
            }
        }

        // 解密（keyHash 字段存储的是加密密钥，从头部读取用于解密）
        std::vector<uint8_t> encryptionKey(header.keyHash, header.keyHash + 32);
        if (std::any_of(encryptionKey.begin(), encryptionKey.end(), [](uint8_t b) { return b != 0; })) {
            // 有加密，使用存储的加密密钥进行 AES 解密
            // 注意：packer 顺序是 encrypt → compress，所以 loader 顺序必须是 decompress → decrypt
            try {
                payload = impl_->decryptData(payload, encryptionKey);
                impl_->resourceInfo.encrypted = true;
                spdlog::info("Decrypted {} bytes", payload.size());
            } catch (const std::exception& e) {
                if (errorCallback_) {
                    errorCallback_(std::string("Decryption failed: ") + e.what());
                }
                return std::nullopt;
            }
        }

        // 验证哈希（对最终解密后的数据验证）
        std::vector<uint8_t> dataHash(header.dataHash, header.dataHash + 32);
        if (std::any_of(dataHash.begin(), dataHash.end(), [](uint8_t b) { return b != 0; })) {
            if (!impl_->verifyHash(payload, dataHash)) {
                if (errorCallback_) {
                    errorCallback_("Hash verification failed");
                }
                return std::nullopt;
            }
        }

        impl_->resourceInfo.version = header.version;
        impl_->resourceInfo.originalSize = header.originalSize;
        impl_->resourceInfo.compressedSize = header.compressedSize;

        // 返回加载的脚本
        LoadedScript script;
        script.data = payload;
        script.name = "embedded";
        script.isBytecode = false;  // TODO: 检测字节码

        spdlog::info("Script loaded successfully: {} bytes", script.data.size());
        return script;

    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(std::string("Failed to load script: ") + e.what());
        }
        return std::nullopt;
    }
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

std::vector<uint8_t> ResourceLoader::readResourceData() {
    return impl_->readResourceData();
}

bool ResourceLoader::parseResourceHeader(const std::vector<uint8_t>& data, ResourceInfo& info, std::vector<uint8_t>& payload) {
    if (data.size() < sizeof(PACK_HEADER)) {
        return false;
    }

    PACK_HEADER header;
    std::memcpy(&header, data.data(), sizeof(PACK_HEADER));

    if (std::memcmp(header.magic, "WMSP", 4) != 0) {
        return false;
    }

    info.version = header.version;
    info.originalSize = header.originalSize;
    info.compressedSize = header.compressedSize;
    info.exists = true;

    payload.assign(
        data.begin() + sizeof(PACK_HEADER),
        data.end()
    );

    return true;
}

std::vector<uint8_t> ResourceLoader::decryptData(const std::vector<uint8_t>& encrypted, const std::vector<uint8_t>& key) {
    return impl_->decryptData(encrypted, key);
}

std::vector<uint8_t> ResourceLoader::decompressData(const std::vector<uint8_t>& compressed) {
    return impl_->decompressData(compressed);
}

bool ResourceLoader::verifyHash(const std::vector<uint8_t>& data, const std::vector<uint8_t>& expectedHash) {
    return impl_->verifyHash(data, expectedHash);
}

} // namespace wingman::runtime
