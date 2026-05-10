#include "wingman/runtime/packer.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <imagehlp.h>
#include <shlobj.h>
#pragma comment(lib, "imagehlib.lib")
#endif

namespace wingman::runtime {

// ========== 资源类型定义 ==========
constexpr const char* WM_SCRIPT_RESOURCE = "WM_SCRIPT";
constexpr const char* WM_MANIFEST_RESOURCE = "WM_MANIFEST";

// ========== 加密头部 (32 字节) ==========
struct PACK_HEADER {
    uint8_t magic[4];           // "WMSP" (WingMan Script Pack)
    uint32_t version;           // 版本号
    uint32_t flags;             // 标志位
    uint64_t originalSize;      // 原始大小
    uint64_t compressedSize;    // 压缩后大小
    uint8_t keyHash[32];        // 密钥哈希
    uint8_t dataHash[32];       // 数据哈希
    uint8_t reserved[64];       // 保留
};

class Packer::Impl {
public:
    PackerOptions options;

    // AES-256 加密
    std::vector<uint8_t> aesEncrypt(const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key) {
#ifdef _WIN32
        std::vector<uint8_t> ciphertext;

        HCRYPTPROV hProv = 0;
        HCRYPTKEY hKey = 0;
        HCRYPTHASH hHash = 0;

        // 获取加密服务提供者
        if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire crypto context");
        }

        // 创建哈希对象
        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to create hash");
        }

        // 导入密钥
        if (!CryptHashData(hHash, key.data(), static_cast<DWORD>(key.size()), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to hash key");
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
        std::memcpy(keyBlob.keyBytes, key.data(), 32);

        if (!CryptImportKey(hProv, reinterpret_cast<BYTE*>(&keyBlob),
                           sizeof(AES_KEY_BLOB), 0, 0, &hKey)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to import key");
        }

        // AES 块大小
        constexpr size_t AES_BLOCK_SIZE = 16;
        size_t paddedSize = plaintext.size() + (AES_BLOCK_SIZE - (plaintext.size() % AES_BLOCK_SIZE));
        std::vector<uint8_t> padded(paddedSize, 0);
        std::memcpy(padded.data(), plaintext.data(), plaintext.size());

        // PKCS7 填充
        uint8_t paddingValue = AES_BLOCK_SIZE - (plaintext.size() % AES_BLOCK_SIZE);
        for (size_t i = plaintext.size(); i < paddedSize; i++) {
            padded[i] = paddingValue;
        }

        ciphertext.resize(paddedSize);

        // 加密（使用 CBC 模式，这里简化为 ECB）
        DWORD dataLen = static_cast<DWORD>(paddedSize);
        if (!CryptEncrypt(hKey, 0, TRUE, 0, ciphertext.data(), &dataLen, static_cast<DWORD>(paddedSize))) {
            CryptDestroyKey(hKey);
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to encrypt data");
        }

        CryptDestroyKey(hKey);
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        return ciphertext;
#else
        // 非 Windows 平台：简单 XOR（临时方案）
        std::vector<uint8_t> ciphertext = plaintext;
        for (size_t i = 0; i < ciphertext.size(); i++) {
            ciphertext[i] ^= key[i % key.size()];
        }
        return ciphertext;
#endif
    }

    // SHA256 哈希
    std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
#ifdef _WIN32
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        std::vector<uint8_t> hash(32);

        if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            throw std::runtime_error("Failed to acquire crypto context");
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to create hash");
        }

        if (!CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to hash data");
        }

        DWORD hashLen = 32;
        if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Failed to get hash value");
        }

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        return hash;
#else
        // 非 Windows 平台：简单哈希
        std::vector<uint8_t> hash(32, 0);
        for (size_t i = 0; i < data.size(); i++) {
            hash[i % 32] ^= data[i];
        }
        return hash;
#endif
    }

    // 简单压缩（LZ4 风格，简化版）
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        // 简单实现：使用重复序列压缩
        std::vector<uint8_t> compressed;
        size_t i = 0;

        while (i < data.size()) {
            // 查找重复序列
            size_t maxRepeat = 0;
            size_t repeatPos = 0;

            for (size_t j = 1; j < 128 && j <= i; j++) {
                size_t count = 0;
                while (i + count < data.size() && count < 127 &&
                       data[i - j] == data[i + count]) {
                    count++;
                }
                if (count > maxRepeat) {
                    maxRepeat = count;
                    repeatPos = j;
                }
            }

            if (maxRepeat >= 4) {
                // 写入重复引用
                compressed.push_back(0x80 | repeatPos);  // 高位表示重复
                compressed.push_back(static_cast<uint8_t>(maxRepeat));
                i += maxRepeat;
            } else {
                // 写入原始字节（最多 127 字节）
                size_t literalCount = std::min(size_t(127), data.size() - i);
                compressed.push_back(static_cast<uint8_t>(literalCount));
                compressed.insert(compressed.end(), data.begin() + i, data.begin() + i + literalCount);
                i += literalCount;
            }
        }

        spdlog::debug("Compression: {} -> {} bytes", data.size(), compressed.size());
        return compressed.size() < data.size() ? compressed : data;
    }

    // 更新 PE 资源
    bool updateResource(const std::vector<uint8_t>& data) {
#ifdef _WIN32
        std::wstring outputPathW;
        outputPathW.assign(options.outputPath.begin(), options.outputPath.end());

        // 开始资源更新
        HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
        if (!hUpdate) {
            spdlog::error("Failed to begin resource update: {}", GetLastError());
            return false;
        }

        // 构建完整资源数据（头部 + 数据）
        std::vector<uint8_t> resourceData;
        PACK_HEADER header = {};
        std::memcpy(header.magic, "WMSP", 4);
        header.version = 1;
        header.flags = 0;
        header.originalSize = options.encrypt ? data.size() : 0;  // 原始大小（加密前）

        std::vector<uint8_t> payload = data;
        if (options.encrypt) {
            // 生成密钥
            std::vector<uint8_t> key = generateKeyImpl();
            payload = aesEncrypt(data, key);
            std::memcpy(header.keyHash, key.data(), std::min(key.size(), size_t(32)));
        }

        if (options.compress) {
            payload = compress(payload);
        }

        header.compressedSize = payload.size();
        std::vector<uint8_t> hash = sha256(payload);
        std::memcpy(header.dataHash, hash.data(), std::min(hash.size(), size_t(32)));

        // 将头部和数据合并
        resourceData.resize(sizeof(PACK_HEADER) + payload.size());
        std::memcpy(resourceData.data(), &header, sizeof(PACK_HEADER));
        std::memcpy(resourceData.data() + sizeof(PACK_HEADER), payload.data(), payload.size());

        // 添加资源
        BOOL result = UpdateResourceA(
            hUpdate,
            RT_RCDATA,                    // 资源类型
            MAKEINTRESOURCEA(100),        // 资源 ID
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
            resourceData.data(),
            static_cast<DWORD>(resourceData.size())
        );

        if (!result) {
            spdlog::error("Failed to update resource: {}", GetLastError());
            EndUpdateResource(hUpdate, TRUE);
            return false;
        }

        // 结束资源更新
        if (!EndUpdateResource(hUpdate, FALSE)) {
            spdlog::error("Failed to end resource update: {}", GetLastError());
            return false;
        }

        spdlog::info("Resource embedded successfully: {} bytes", resourceData.size());
        return true;
#else
        spdlog::error("Resource update not supported on this platform");
        return false;
#endif
    }

    // 替换图标
    bool replaceIcon() {
        if (options.iconPath.empty()) {
            return true;  // 没有指定图标，跳过
        }

#ifdef _WIN32
        std::wstring iconPathW, outputPathW;
        iconPathW.assign(options.iconPath.begin(), options.iconPath.end());
        outputPathW.assign(options.outputPath.begin(), options.outputPath.end());

        // 简单实现：复制图标到资源
        HANDLE hUpdate = BeginUpdateResourceW(outputPathW.c_str(), FALSE);
        if (!hUpdate) {
            spdlog::warn("Failed to begin resource update for icon: {}", GetLastError());
            return false;
        }

        // 读取图标文件
        std::ifstream iconFile(options.iconPath, std::ios::binary);
        if (!iconFile) {
            spdlog::warn("Failed to open icon file: {}", options.iconPath);
            EndUpdateResource(hUpdate, TRUE);
            return false;
        }

        std::vector<uint8_t> iconData((std::istreambuf_iterator<char>(iconFile)),
                                       std::istreambuf_iterator<char>());
        iconFile.close();

        // 替换主图标 (ID = 1)
        BOOL result = UpdateResourceA(
            hUpdate,
            RT_ICON,
            MAKEINTRESOURCEA(1),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
            iconData.data(),
            static_cast<DWORD>(iconData.size())
        );

        if (!result) {
            spdlog::warn("Failed to update icon resource: {}", GetLastError());
            EndUpdateResource(hUpdate, TRUE);
            return false;
        }

        EndUpdateResource(hUpdate, FALSE);
        spdlog::info("Icon replaced successfully");
        return true;
#else
        spdlog::warn("Icon replacement not supported on this platform");
        return true;
#endif
    }

    std::vector<uint8_t> generateKeyImpl() {
        std::vector<uint8_t> key(32);
#ifdef _WIN32
        HCRYPTPROV hProv = 0;
        if (CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            CryptGenRandom(hProv, 32, key.data());
            CryptReleaseContext(hProv, 0);
        }
#else
        for (size_t i = 0; i < 32; i++) {
            key[i] = static_cast<uint8_t>(rand() % 256);
        }
#endif
        return key;
    }
};

// ========== Packer 实现 ==========

Packer::Packer(const PackerOptions& options)
    : impl_(std::make_unique<Impl>())
{
    impl_->options = options;
}

Packer::~Packer() = default;

PackerResult Packer::build() {
    PackerResult result;
    result.outputPath = impl_->options.outputPath;

    try {
        spdlog::info("=== Wingman Packer ===");
        spdlog::info("Script: {}", impl_->options.scriptPath);
        spdlog::info("Output: {}", impl_->options.outputPath);
        spdlog::info("Encrypt: {}", impl_->options.encrypt);
        spdlog::info("Compress: {}", impl_->options.compress);

        // 1. 读取并处理脚本
        std::vector<uint8_t> scriptData = processScript();
        if (scriptData.empty()) {
            result.message = "Failed to read script file";
            return result;
        }

        // 2. 复制 stub 程序
        if (!copyStub()) {
            result.message = "Failed to copy stub executable";
            return result;
        }

        // 3. 嵌入资源
        if (!embedResource(scriptData)) {
            result.message = "Failed to embed script resource";
            return result;
        }

        // 4. 替换图标
        if (!impl_->replaceIcon()) {
            spdlog::warn("Icon replacement failed, continuing...");
        }

        result.success = true;
        result.message = "Build completed successfully";

        spdlog::info("=== Build Complete ===");
        spdlog::info("Output: {}", impl_->options.outputPath);
        spdlog::info("Size: {} KB", std::filesystem::file_size(impl_->options.outputPath) / 1024);

    } catch (const std::exception& e) {
        result.message = std::string("Build failed: ") + e.what();
        spdlog::error("{}", result.message);
    }

    return result;
}

std::vector<uint8_t> Packer::processScript() {
    std::ifstream file(impl_->options.scriptPath, std::ios::binary);
    if (!file) {
        spdlog::error("Failed to open script: {}", impl_->options.scriptPath);
        return {};
    }

    std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    file.close();

    spdlog::info("Script loaded: {} bytes", content.size());
    return content;
}

std::vector<uint8_t> Packer::compileToBytecode(const std::string& source) {
    // TODO: 实现 Lua 字节码编译
    // 可以使用 luac 或 LuaLCompileBuffer
    spdlog::warn("Bytecode compilation not yet implemented, using source code");
    return std::vector<uint8_t>(source.begin(), source.end());
}

std::vector<uint8_t> Packer::encryptData(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> key = generateKey();
    return impl_->aesEncrypt(data, key);
}

std::vector<uint8_t> Packer::compressData(const std::vector<uint8_t>& data) {
    return impl_->compress(data);
}

bool Packer::copyStub() {
    std::error_code ec;

    // 如果输出文件已存在，先删除
    if (std::filesystem::exists(impl_->options.outputPath)) {
        std::filesystem::remove(impl_->options.outputPath, ec);
        if (ec) {
            spdlog::error("Failed to remove existing output file: {}", ec.message());
            return false;
        }
    }

    // 复制 stub 程序
    std::filesystem::copy_file(impl_->options.stubPath, impl_->options.outputPath, ec);
    if (ec) {
        spdlog::error("Failed to copy stub: {} -> {}", impl_->options.stubPath, impl_->options.outputPath);
        return false;
    }

    spdlog::info("Stub copied: {} bytes", std::filesystem::file_size(impl_->options.outputPath));
    return true;
}

bool Packer::embedResource(const std::vector<uint8_t>& data) {
    return impl_->updateResource(data);
}

bool Packer::replaceIcon() {
    return impl_->replaceIcon();
}

bool Packer::setVersionInfo() {
    // TODO: 实现 PE 版本信息设置
    spdlog::warn("Version info setting not yet implemented");
    return true;
}

std::vector<uint8_t> Packer::generateKey() {
    return impl_->generateKeyImpl();
}

std::vector<uint8_t> Packer::calculateHash(const std::vector<uint8_t>& data) {
    return impl_->sha256(data);
}

} // namespace wingman::runtime
