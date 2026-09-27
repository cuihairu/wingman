#pragma once

// ========== 打包资源格式（PACK_HEADER）与口令派生 ==========
//
// 本头是 packer（写侧）与 resource_loader（读侧）对同一格式的**唯一**权威定义。
// 此前两侧各自复制了一份 struct 定义，并各自实现了一份 `_WIN32` 下的 CryptAPI
// 加解密；非 Windows 分支只剩「异或假哈希 + 一律拒绝加密」。收敛成单一定义 +
// 单一可移植实现（wingman::crypt，OpenSSL EVP），Linux/macOS 才能跑通同一条
// 打包→加载链路，Windows 也不再有两套密码学代码。
//
// 版本语义（读侧按此判定）：
//   v1（历史）：加密密钥由 packer 现场随机生成、只把 sha256(key) 写进 keyHash，
//              密钥本身即产生即丢弃——加密包在构造上不可恢复；而当时 `Packer::build()`
//              又直接拒绝 encrypt，因此**不存在合法的 v1 加密包**。v1 的未加密
//              （含压缩）包继续可读。
//   v2（当前）：口令派生密钥（PBKDF2-HMAC-SHA256），salt / IV / 迭代次数写进
//              reserved[]，载荷为 AES-256-GCM 认证加密结果（密文 || tag）。
//              变换顺序为 compress → encrypt（读侧镜像 decrypt → decompress）：
//              先加密会让密文里没有字节连串可压，「加密 + 压缩」将静默退化成「只加密」。
//              未加密包同样写 v2：v1 读侧不解释 reserved[]、也不校验版本号，
//              所以新写的未加密包对旧读侧仍然可加载（向后兼容）。
//
// 完整性分层（为什么要三层各司其职）：
//   * AES-256-GCM tag：证明密文未被篡改、且持有正确口令（口令错 → tag 校验失败）。
//   * keyHash = sha256(derivedKey)：解密**前**的口令指纹，只为把「口令错误」与
//     「数据损坏」区分成两条可读错误信息，不承担独立的安全边界。
//   * dataHash = sha256(原始明文)：解密后校验。头部被改写（改 flags / originalSize /
//     换 reserved 里的 salt 等）虽然不会让 GCM 失败（头部不在认证范围内），但攻击者
//     没有密钥就无法构造出能解密成任意内容的密文，明文不变而头部变了 → dataHash
//     必然不匹配 → 拒绝。头部篡改由此兜住。

#include "wingman/crypt.hpp"

#include <algorithm>  // std::any_of（显式包含，不依赖传递包含）
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace wingman::runtime {

// ========== 格式常量 ==========

inline constexpr uint32_t PACK_FORMAT_VERSION_LEGACY = 1;  ///< 历史版本（随机密钥，加密包不可恢复）
inline constexpr uint32_t PACK_FORMAT_VERSION = 2;          ///< 当前写侧使用的版本

inline constexpr uint32_t PACK_FLAG_ENCRYPTED = 0x01;   ///< 数据已加密
inline constexpr uint32_t PACK_FLAG_COMPRESSED = 0x02;  ///< 数据已压缩

/// 打包头部（160 字节，两侧按字节 memcpy 解释，禁止加入非 POD 成员）
struct PACK_HEADER {
    uint8_t magic[4];         // "WMSP" (WingMan Script Pack)
    uint32_t version;         // 版本号
    uint32_t flags;           // 标志位
    uint64_t originalSize;    // 原始大小
    uint64_t compressedSize;  // 压缩后大小
    uint8_t keyHash[32];      // 密钥哈希（SHA-256 of derived key，用于口令校验，不是密钥本身）
    uint8_t dataHash[32];     // 数据哈希（SHA-256 of original data）
    uint8_t reserved[64];     // 保留：v2 起用于加密参数（见下方布局）
};

static_assert(sizeof(PACK_HEADER) == 160,
              "PACK_HEADER 是 packer 与 loader 共享的落盘布局，尺寸变化即格式破坏性变更");

// ========== v2 加密参数在 reserved[64] 中的布局 ==========
//
// [0,16)   salt（PBKDF2 salt，随机）
// [16,28)  IV（AES-GCM 96-bit IV，随机）
// [28,32)  PBKDF2 迭代次数（u32 小端）
// [32,64)  保留，写侧必须清零（未来新字段从零追加，旧读侧忽略即向前兼容）

inline constexpr size_t PACK_CRYPTO_SALT_LEN = 16;
inline constexpr size_t PACK_CRYPTO_IV_LEN = 12;  // GCM 推荐 96-bit
inline constexpr size_t PACK_CRYPTO_ITERATIONS_LEN = 4;
inline constexpr size_t PACK_CRYPTO_SALT_OFFSET = 0;
inline constexpr size_t PACK_CRYPTO_IV_OFFSET = PACK_CRYPTO_SALT_OFFSET + PACK_CRYPTO_SALT_LEN;
inline constexpr size_t PACK_CRYPTO_ITERATIONS_OFFSET = PACK_CRYPTO_IV_OFFSET + PACK_CRYPTO_IV_LEN;
inline constexpr size_t PACK_CRYPTO_USED_BYTES = PACK_CRYPTO_ITERATIONS_OFFSET + PACK_CRYPTO_ITERATIONS_LEN;

static_assert(PACK_CRYPTO_USED_BYTES <= sizeof(PACK_HEADER::reserved),
              "加密参数放不下 reserved[]，需要的是格式版本升级而不是继续塞字段");

// ========== 容器约定（仅 Windows PE 用得到；非 Windows 走内存字节流） ==========
//
// 打包资源在 PE 里的定位：类型 RT_RCDATA、名字 100。写侧（Packer::Impl::updateResource）
// 与读侧（ResourceLoader::Impl 的 FindResourceA）必须取同一个值——两侧各写一遍字面量
// 正是本头要消灭的那类分叉。

inline constexpr int PACK_PE_RESOURCE_ID = 100;

inline constexpr size_t PACK_KEY_LEN = 32;                              // AES-256
inline constexpr uint32_t PACK_DEFAULT_KDF_ITERATIONS = 100000;         ///< 与 wingman::crypt 默认一致
inline constexpr uint32_t PACK_MAX_KDF_ITERATIONS = 5000000;            ///< 读侧上限：拒绝病态迭代次数

/// 头部里携带的密钥派生参数
struct PackCryptoParams {
    uint8_t salt[PACK_CRYPTO_SALT_LEN] = {};
    uint8_t iv[PACK_CRYPTO_IV_LEN] = {};
    uint32_t iterations = PACK_DEFAULT_KDF_ITERATIONS;
};

inline constexpr const char* PACK_MAGIC = "WMSP";

inline void setPackMagic(PACK_HEADER& header) {
    std::memcpy(header.magic, PACK_MAGIC, 4);
}

inline bool hasPackMagic(const PACK_HEADER& header) {
    return std::memcmp(header.magic, PACK_MAGIC, 4) == 0;
}

inline void writeLe32(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

inline uint32_t readLe32(const uint8_t* in) {
    return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) | (static_cast<uint32_t>(in[3]) << 24);
}

/// 把加密参数写进头部 reserved 段（未使用的尾字节清零）
inline void setPackCryptoParams(PACK_HEADER& header, const PackCryptoParams& params) {
    std::memset(header.reserved, 0, sizeof(header.reserved));
    std::memcpy(header.reserved + PACK_CRYPTO_SALT_OFFSET, params.salt, PACK_CRYPTO_SALT_LEN);
    std::memcpy(header.reserved + PACK_CRYPTO_IV_OFFSET, params.iv, PACK_CRYPTO_IV_LEN);
    writeLe32(header.reserved + PACK_CRYPTO_ITERATIONS_OFFSET, params.iterations);
}

/// 从头部 reserved 段读出加密参数；迭代次数为 0 或超出读侧上限即 false
/// （0 会让 PBKDF2 直接失败，超大值等于让加载方替打包方烧 CPU——都不接受）
inline bool readPackCryptoParams(const PACK_HEADER& header, PackCryptoParams& params) {
    const uint32_t iterations = readLe32(header.reserved + PACK_CRYPTO_ITERATIONS_OFFSET);
    if (iterations == 0 || iterations > PACK_MAX_KDF_ITERATIONS) {
        return false;
    }
    std::memcpy(params.salt, header.reserved + PACK_CRYPTO_SALT_OFFSET, PACK_CRYPTO_SALT_LEN);
    std::memcpy(params.iv, header.reserved + PACK_CRYPTO_IV_OFFSET, PACK_CRYPTO_IV_LEN);
    params.iterations = iterations;
    return true;
}

/// 头部里的 IV 转成 aesGcm* 需要的 vector（两侧同一个转换，避免各自手抄偏移）
inline std::vector<uint8_t> packCryptoIv(const PackCryptoParams& params) {
    return std::vector<uint8_t>(params.iv, params.iv + PACK_CRYPTO_IV_LEN);
}

/// 为一次打包生成新的 salt + IV（同一口令两次打包必须产出不同密文）
inline PackCryptoParams makePackCryptoParams() {
    PackCryptoParams params;
    params.iterations = PACK_DEFAULT_KDF_ITERATIONS;

    const auto salt = wingman::crypt::randomBytes(PACK_CRYPTO_SALT_LEN);
    const auto iv = wingman::crypt::randomBytes(PACK_CRYPTO_IV_LEN);
    if (salt.size() != PACK_CRYPTO_SALT_LEN || iv.size() != PACK_CRYPTO_IV_LEN) {
        throw std::runtime_error("Failed to generate pack salt/IV (RAND_bytes unavailable)");
    }
    std::memcpy(params.salt, salt.data(), PACK_CRYPTO_SALT_LEN);
    std::memcpy(params.iv, iv.data(), PACK_CRYPTO_IV_LEN);
    return params;
}

/// 原始字节的 SHA-256（32 字节）。两侧同源，Linux 侧不再有假哈希。
inline std::vector<uint8_t> sha256Bytes(const std::vector<uint8_t>& data) {
    const std::string hex = wingman::crypt::sha256(
        std::string(reinterpret_cast<const char*>(data.data()), data.size()));
    auto bytes = wingman::crypt::hexToBytes(hex);
    if (bytes.size() != 32) {
        throw std::runtime_error("SHA-256 failed");
    }
    return bytes;
}

/// 口令 + 头部参数 → 32 字节 AES-256 密钥（PBKDF2-HMAC-SHA256）
///
/// 空口令一律拒绝：读侧 `loadScript()` 的 password 有默认值 ""，若不在此拦住，
/// 「忘了传口令」会退化成一次合法的、以空口令派生密钥的解密尝试。
inline std::vector<uint8_t> derivePackKey(const std::string& password, const PackCryptoParams& params) {
    if (password.empty()) {
        throw std::runtime_error("Encryption requires a non-empty password");
    }
    if (params.iterations == 0 || params.iterations > PACK_MAX_KDF_ITERATIONS) {
        throw std::runtime_error("Invalid PBKDF2 iteration count in pack header");
    }

    // wingman::crypt::deriveKey 的 salt 形参按 hex 解释（与 crypto.encryptAES 的公开格式一致），
    // 头部存的是原始字节，故此处 hex 编解码一轮；迭代次数由头部给定而非用默认值。
    const std::vector<uint8_t> salt(params.salt, params.salt + PACK_CRYPTO_SALT_LEN);
    const std::string keyHex = wingman::crypt::deriveKey(password, wingman::crypt::bytesToHex(salt),
                                                         static_cast<int>(params.iterations), PACK_KEY_LEN);
    auto key = wingman::crypt::hexToBytes(keyHex);
    if (key.size() != PACK_KEY_LEN) {
        throw std::runtime_error("Key derivation failed");
    }
    return key;
}

/// 密钥指纹（写进 keyHash，供加载前判定口令是否正确）
inline std::vector<uint8_t> packKeyFingerprint(const std::vector<uint8_t>& key) {
    return sha256Bytes(key);
}

inline bool packKeyFingerprintMatches(const PACK_HEADER& header, const std::vector<uint8_t>& key) {
    const std::vector<uint8_t> expected(header.keyHash, header.keyHash + 32);
    const bool hasFingerprint =
        std::any_of(expected.begin(), expected.end(), [](uint8_t b) { return b != 0; });
    if (!hasFingerprint) {
        return true;  // 无指纹的包（历史/未加密）不做判定，交给 GCM tag
    }
    return packKeyFingerprint(key) == expected;
}

/// 头部 dataHash 是否非零（非零才需要校验）
inline bool packHasDataHash(const PACK_HEADER& header) {
    for (size_t i = 0; i < sizeof(header.dataHash); ++i) {
        if (header.dataHash[i] != 0) {
            return true;
        }
    }
    return false;
}

} // namespace wingman::runtime
