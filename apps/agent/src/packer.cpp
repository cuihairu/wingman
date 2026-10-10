#include "wingman/runtime/packer.hpp"
#include "wingman/runtime/resource_pack.hpp"
#include "platform/agent_platform.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>  // std::min（显式包含，不依赖传递包含）
#include <fstream>
#include <sstream>
#include <cstring>
#include <filesystem>

#ifdef WINGMAN_HAS_LUA
#include <sol/sol.hpp>
#endif

namespace wingman::runtime {

// ========== 资源类型定义 ==========
constexpr const char* WM_SCRIPT_RESOURCE = "WM_SCRIPT";
constexpr const char* WM_MANIFEST_RESOURCE = "WM_MANIFEST";

namespace {

/// 简化 LZ 压缩（LZ4 风格，非严格兼容）；与 resource_loader.cpp 的解压互为逆操作。
/// 压缩不省字节时原样返回——调用方按「长度是否变短」决定是否置 PACK_FLAG_COMPRESSED。
std::vector<uint8_t> compressPayload(const std::vector<uint8_t>& data) {
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
            compressed.push_back(0x80 | static_cast<uint8_t>(repeatPos));  // 高位表示重复
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

} // namespace

class Packer::Impl {
public:
    PackerOptions options;
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

    auto cleanupPartialOutput = [&]() {
        std::error_code ec;
        std::filesystem::remove(impl_->options.outputPath, ec);
        if (ec) {
            spdlog::warn("Failed to remove partial output '{}': {}", impl_->options.outputPath, ec.message());
        }
    };

    try {
        spdlog::info("=== Wingman Packer ===");
        spdlog::info("Script: {}", impl_->options.scriptPath);
        spdlog::info("Output: {}", impl_->options.outputPath);
        // 口令只参与密钥派生：不进日志、不写进产物（产物里只有 salt/IV 与密钥指纹）
        spdlog::info("Encrypt: {}", impl_->options.encrypt);
        spdlog::info("Compress: {}", impl_->options.compress);

        if (impl_->options.encrypt && impl_->options.password.empty()) {
            // 加密产物没有口令就是永久打不开的字节流，故在动手（复制 stub）之前硬拒。
            result.message = "Encryption requires a password: pass --password (or set WINGMAN_PACK_PASSWORD)";
            spdlog::error("{}", result.message);
            return result;
        }

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
            cleanupPartialOutput();
            return result;
        }

        // 4. 替换图标
        if (!replaceIcon()) {
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
        cleanupPartialOutput();
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
#ifdef WINGMAN_HAS_LUA
    // Use Lua C API to compile source to bytecode
    sol::state lua;
    // Load source without executing
    auto result = lua.load(source, "packed_script");
    if (!result.valid()) {
        sol::error err = result;
        spdlog::error("Failed to compile Lua script: {}", err.what());
        return std::vector<uint8_t>(source.begin(), source.end());
    }

    // Dump bytecode from the compiled function
    std::vector<uint8_t> bytecode;
    sol::protected_function fn = result;

    lua_State* L = lua.lua_state();
    // Push the function onto the stack
    fn.push(L);

    // Dump bytecode
    struct BytecodeWriter {
        std::vector<uint8_t>* output;
        BytecodeWriter(std::vector<uint8_t>* out) : output(out) {}
    };

    auto writer = [](lua_State* /*L*/, const void* data, size_t sz, void* ud) -> int {
        auto* writer = static_cast<BytecodeWriter*>(ud);
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        writer->output->insert(writer->output->end(), bytes, bytes + sz);
        return 0;
    };

    BytecodeWriter bw(&bytecode);
    if (lua_dump(L, writer, &bw, 0) != 0) {
        spdlog::error("Failed to dump Lua bytecode");
        lua_pop(L, 1);
        return std::vector<uint8_t>(source.begin(), source.end());
    }

    lua_pop(L, 1);
    spdlog::info("Compiled to Lua bytecode: {} bytes", bytecode.size());
    return bytecode;
#else
    spdlog::warn("Lua not available, using source code");
    return std::vector<uint8_t>(source.begin(), source.end());
#endif
}

std::vector<uint8_t> Packer::buildResourceBytes(const std::vector<uint8_t>& scriptData,
                                                bool encrypt,
                                                bool compress,
                                                const std::string& password) {
    PACK_HEADER header = {};
    setPackMagic(header);
    header.version = PACK_FORMAT_VERSION;
    header.flags = 0;
    header.originalSize = scriptData.size();  // 始终记录原始大小

    // dataHash 必须基于原始明文——加密/压缩后的字节不参与完整性定义
    const std::vector<uint8_t> hash = sha256Bytes(scriptData);
    std::memcpy(header.dataHash, hash.data(), std::min(hash.size(), sizeof(header.dataHash)));

    std::vector<uint8_t> payload = scriptData;

    // 变换顺序是 compress → encrypt（读侧镜像：decrypt → decompress）。
    // 反过来的话密文里没有字节连串可压，压缩永远不省字节 ——「加密 + 压缩」的组合
    // 会静默退化成「只加密」（v2 之前正是这个顺序，但当时 build() 拒绝 encrypt，
    // 故不存在依赖旧顺序的产物）。压缩明文只泄露「明文可压缩程度」这一弱信号，
    // 且这里是静态一次性压缩、无攻击者可参与的自适应压缩，不构成 CRIME 类面。
    if (compress) {
        // 只有真的压小才置位：packer 在没省字节时原样返回，读侧据标志决定是否解压
        const size_t preCompressSize = payload.size();
        payload = compressPayload(payload);
        if (payload.size() < preCompressSize) {
            header.flags |= PACK_FLAG_COMPRESSED;
        }
    }

    if (encrypt) {
        // 口令 → PBKDF2 → AES-256 密钥；salt/IV 随机生成并写进头部（同口令两次打包密文不同）。
        // derivePackKey 对空口令直接抛错：没有口令的加密产物再也解不开，宁可不产出。
        const PackCryptoParams params = makePackCryptoParams();
        const std::vector<uint8_t> key = derivePackKey(password, params);

        setPackCryptoParams(header, params);
        payload = wingman::crypt::aesGcmEncrypt(key, packCryptoIv(params), payload);
        // keyHash 存的是 sha256(派生密钥)——不是密钥本身，只用于加载前的口令快速校验，
        // 真正的边界是 AES-GCM 的认证标签。
        const std::vector<uint8_t> keyHash = packKeyFingerprint(key);
        std::memcpy(header.keyHash, keyHash.data(), std::min(keyHash.size(), sizeof(header.keyHash)));
        header.flags |= PACK_FLAG_ENCRYPTED;
    }

    header.compressedSize = payload.size();

    std::vector<uint8_t> resourceData(sizeof(PACK_HEADER) + payload.size());
    std::memcpy(resourceData.data(), &header, sizeof(PACK_HEADER));
    std::memcpy(resourceData.data() + sizeof(PACK_HEADER), payload.data(), payload.size());
    return resourceData;
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
    // 完整资源数据（头部 + 负载）由平台无关的同一条代码产出：
    // PE 嵌入只是给这份字节流换个容器，Linux 上测的字节语义与 Windows 完全一致。
    const std::vector<uint8_t> resourceData =
        Packer::buildResourceBytes(data, impl_->options.encrypt, impl_->options.compress, impl_->options.password);
    return platform::updatePeResource(impl_->options.outputPath, resourceData);
}

bool Packer::replaceIcon() {
    return platform::replacePeIcon(impl_->options.outputPath, impl_->options.iconPath);
}

bool Packer::setVersionInfo() {
    return platform::setPeVersionInfo(impl_->options.outputPath, impl_->options.appName, impl_->options.appVersion);
}

} // namespace wingman::runtime
