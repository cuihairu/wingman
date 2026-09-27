#include <gtest/gtest.h>

#include "wingman/crypt.hpp"
#include "wingman/runtime/packer.hpp"
#include "wingman/runtime/resource_loader.hpp"
#include "wingman/runtime/resource_pack.hpp"

#include <algorithm>
#include <cstddef>  // offsetof（按字段偏移构造被篡改的头部）
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace wingman::runtime {
namespace {

using wingman::crypt::hexToBytes;

std::vector<uint8_t> bytesOf(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string textOf(const std::vector<uint8_t>& bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

bool isAllZero(const uint8_t* data, size_t len) {
    return std::all_of(data, data + len, [](uint8_t b) { return b == 0; });
}

/// 头部各字段在资源字节流里的位置（reserved 段起点 = 头部末尾往前 64 字节）
size_t reservedOffset() {
    return sizeof(PACK_HEADER) - sizeof(PACK_HEADER::reserved);
}

/// 取加载失败的原因文本（loadScriptFromBytes 的错误回调）
struct ErrorSink {
    std::string message;
    ResourceLoader::ErrorCallback callback() {
        return [this](const std::string& m) { message = m; };
    }
};

/// 手工拼一份 PACK_HEADER + 负载，用于构造 packer 不会产出的输入
/// （历史版本、未知版本、被改写的头部字段）
std::vector<uint8_t> makeRawPack(uint32_t version,
                                 uint32_t flags,
                                 const std::vector<uint8_t>& payload,
                                 const std::vector<uint8_t>& dataHash = {},
                                 const PackCryptoParams* crypto = nullptr) {
    PACK_HEADER header = {};
    setPackMagic(header);
    header.version = version;
    header.flags = flags;
    header.originalSize = payload.size();
    header.compressedSize = payload.size();
    if (!dataHash.empty()) {
        std::memcpy(header.dataHash, dataHash.data(), std::min(dataHash.size(), sizeof(header.dataHash)));
    }
    if (crypto) {
        setPackCryptoParams(header, *crypto);
    }

    std::vector<uint8_t> bytes(sizeof(PACK_HEADER) + payload.size());
    std::memcpy(bytes.data(), &header, sizeof(PACK_HEADER));
    std::memcpy(bytes.data() + sizeof(PACK_HEADER), payload.data(), payload.size());
    return bytes;
}

PACK_HEADER readHeaderOf(const std::vector<uint8_t>& resource) {
    PACK_HEADER header = {};
    std::memcpy(&header, resource.data(), sizeof(PACK_HEADER));
    return header;
}

/// 负载切片（跳过头部）
std::vector<uint8_t> payloadOf(const std::vector<uint8_t>& resource) {
    return std::vector<uint8_t>(resource.begin() + static_cast<long>(sizeof(PACK_HEADER)), resource.end());
}

/// 压缩器的匹配模型是「同一字节连出现 ≥4 次」（见 packer.cpp compressPayload 里
/// data[i - j] 这个不随 count 前进的常量左下标），故只有含长重复字节段的数据才压得动。
std::string textWithByteRuns() {
    std::string text;
    text.reserve(60000);
    for (int i = 0; i < 2000; ++i) {
        text += "line " + std::to_string(i) + ": " + std::string(24, ' ') + "\n";
    }
    return text;
}

const std::string kScript = "print('hello wingman')\n";
const std::string kPassword = "correct horse battery staple";

// ========== 格式布局 ==========

TEST(ResourcePackFormatTest, HeaderLayoutIsLocked) {
    // 尺寸是写读两侧共享的落盘契约：加字段/改对齐都必须是有意的版本升级
    EXPECT_EQ(sizeof(PACK_HEADER), 160u);
    EXPECT_EQ(PACK_CRYPTO_USED_BYTES, 32u);
    EXPECT_LE(PACK_CRYPTO_USED_BYTES, sizeof(PACK_HEADER::reserved));
}

TEST(ResourcePackFormatTest, MagicIsRecognised) {
    PACK_HEADER header = {};
    EXPECT_FALSE(hasPackMagic(header));
    setPackMagic(header);
    EXPECT_TRUE(hasPackMagic(header));
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(header.magic), 4), "WMSP");
}

TEST(ResourcePackFormatTest, CryptoParamsRoundTripThroughReserved) {
    PackCryptoParams written = {};
    for (size_t i = 0; i < PACK_CRYPTO_SALT_LEN; ++i) {
        written.salt[i] = static_cast<uint8_t>(i + 1);
    }
    for (size_t i = 0; i < PACK_CRYPTO_IV_LEN; ++i) {
        written.iv[i] = static_cast<uint8_t>(0xF0 + i);
    }
    written.iterations = 123456;

    PACK_HEADER header = {};
    setPackMagic(header);
    setPackCryptoParams(header, written);

    // 迭代次数按小端 4 字节存放在 salt/IV 之后
    EXPECT_EQ(readLe32(header.reserved + PACK_CRYPTO_ITERATIONS_OFFSET), 123456u);
    // 未使用的尾字节必须清零（将来的新字段要从零追加，旧读侧才能忽略）
    EXPECT_TRUE(isAllZero(header.reserved + PACK_CRYPTO_USED_BYTES,
                          sizeof(header.reserved) - PACK_CRYPTO_USED_BYTES));

    PackCryptoParams readBack = {};
    ASSERT_TRUE(readPackCryptoParams(header, readBack));
    EXPECT_EQ(std::memcmp(readBack.salt, written.salt, PACK_CRYPTO_SALT_LEN), 0);
    EXPECT_EQ(std::memcmp(readBack.iv, written.iv, PACK_CRYPTO_IV_LEN), 0);
    EXPECT_EQ(readBack.iterations, written.iterations);
    EXPECT_EQ(packCryptoIv(readBack), std::vector<uint8_t>(written.iv, written.iv + PACK_CRYPTO_IV_LEN));
}

TEST(ResourcePackFormatTest, ReadRejectsBogusIterationCounts) {
    PACK_HEADER header = {};
    setPackMagic(header);
    PackCryptoParams params;

    // 0 会让 PBKDF2 直接失败
    writeLe32(header.reserved + PACK_CRYPTO_ITERATIONS_OFFSET, 0);
    EXPECT_FALSE(readPackCryptoParams(header, params));

    // 超上界等于让加载方替打包方烧 CPU
    writeLe32(header.reserved + PACK_CRYPTO_ITERATIONS_OFFSET, PACK_MAX_KDF_ITERATIONS + 1);
    EXPECT_FALSE(readPackCryptoParams(header, params));

    // 边界值本身可接受
    writeLe32(header.reserved + PACK_CRYPTO_ITERATIONS_OFFSET, PACK_MAX_KDF_ITERATIONS);
    ASSERT_TRUE(readPackCryptoParams(header, params));
    EXPECT_EQ(params.iterations, PACK_MAX_KDF_ITERATIONS);
}

TEST(ResourcePackFormatTest, Le32IsLittleEndian) {
    uint8_t bytes[4] = {};
    writeLe32(bytes, 0x12345678u);
    EXPECT_EQ(bytes[0], 0x78);
    EXPECT_EQ(bytes[1], 0x56);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);
    EXPECT_EQ(readLe32(bytes), 0x12345678u);
}

// ========== 密钥派生 ==========

TEST(ResourcePackKeyTest, Sha256BytesMatchesKnownVector) {
    // 与 crypt::sha256 的 hex 口径对齐：这里要的是原始 32 字节
    const auto digest = sha256Bytes(bytesOf("abc"));
    ASSERT_EQ(digest.size(), 32u);
    EXPECT_EQ(digest, hexToBytes("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST(ResourcePackKeyTest, DerivedKeyIsDeterministicForSamePasswordAndSalt) {
    const PackCryptoParams params = makePackCryptoParams();
    const auto a = derivePackKey(kPassword, params);
    const auto b = derivePackKey(kPassword, params);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.size(), PACK_KEY_LEN);
}

TEST(ResourcePackKeyTest, DerivedKeyChangesWithPasswordSaltOrIterations) {
    const PackCryptoParams params = makePackCryptoParams();
    EXPECT_NE(derivePackKey(kPassword, params), derivePackKey("another password", params));

    PackCryptoParams otherSalt = params;
    otherSalt.salt[0] ^= 0x01;
    EXPECT_NE(derivePackKey(kPassword, params), derivePackKey(kPassword, otherSalt));

    PackCryptoParams fewer = params;
    fewer.iterations = 1000;
    EXPECT_NE(derivePackKey(kPassword, params), derivePackKey(kPassword, fewer));
}

TEST(ResourcePackKeyTest, EmptyPasswordIsRejected) {
    // 读侧 loadScript() 的口令形参默认是 ""：不在派生这层拦住，
    // 「忘了传口令」就会退化成一次以空口令派生密钥的正常解密尝试
    const PackCryptoParams params = makePackCryptoParams();
    EXPECT_THROW(derivePackKey("", params), std::runtime_error);
}

TEST(ResourcePackKeyTest, GeneratedParamsAreFreshAndSane) {
    const auto a = makePackCryptoParams();
    const auto b = makePackCryptoParams();
    EXPECT_EQ(a.iterations, PACK_DEFAULT_KDF_ITERATIONS);
    // 同口令两次打包必须产出不同的 salt/IV
    EXPECT_NE(std::memcmp(a.salt, b.salt, PACK_CRYPTO_SALT_LEN), 0);
    EXPECT_NE(std::memcmp(a.iv, b.iv, PACK_CRYPTO_IV_LEN), 0);
}

TEST(ResourcePackKeyTest, FingerprintMatchesOnlyTheRightKey) {
    const PackCryptoParams params = makePackCryptoParams();
    const auto key = derivePackKey(kPassword, params);

    PACK_HEADER header = {};
    setPackMagic(header);
    const auto fingerprint = packKeyFingerprint(key);
    std::memcpy(header.keyHash, fingerprint.data(), fingerprint.size());

    EXPECT_TRUE(packKeyFingerprintMatches(header, key));
    EXPECT_FALSE(packKeyFingerprintMatches(header, derivePackKey("wrong", params)));

    // 全零指纹（未加密/历史包）不做判定，交给 GCM 标签
    PACK_HEADER noFingerprint = {};
    setPackMagic(noFingerprint);
    EXPECT_TRUE(packKeyFingerprintMatches(noFingerprint, key));
}

// ========== 写侧：buildResourceBytes ==========

TEST(ResourcePackBuildTest, PlainPackCarriesNoV2OnlyState) {
    const auto resource = Packer::buildResourceBytes(bytesOf(kScript), false, true, "");
    const auto header = readHeaderOf(resource);

    EXPECT_TRUE(hasPackMagic(header));
    EXPECT_EQ(header.version, PACK_FORMAT_VERSION);
    EXPECT_EQ(header.flags & PACK_FLAG_ENCRYPTED, 0u);
    // 未加密包不写任何加密参数：reserved 全零 + 无密钥指纹。
    // 旧读侧（不认得 v2、不解释 reserved）因此仍能加载新写的未加密包 —— 向后兼容的根据。
    EXPECT_TRUE(isAllZero(header.keyHash, sizeof(header.keyHash)))
        << "未加密包不得留下密钥指纹";
    EXPECT_TRUE(isAllZero(header.reserved, sizeof(header.reserved)));
    EXPECT_EQ(header.originalSize, kScript.size());
    EXPECT_EQ(header.compressedSize, payloadOf(resource).size());
    // dataHash 定义在原始明文上（不是压缩/加密后的字节）
    EXPECT_EQ(std::vector<uint8_t>(header.dataHash, header.dataHash + sizeof(header.dataHash)),
              sha256Bytes(bytesOf(kScript)));
}

TEST(ResourcePackBuildTest, EncryptedPackPublishesParamsButNeverTheKey) {
    const auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    const auto header = readHeaderOf(resource);

    EXPECT_EQ(header.version, PACK_FORMAT_VERSION);
    EXPECT_NE(header.flags & PACK_FLAG_ENCRYPTED, 0u);

    PackCryptoParams params;
    ASSERT_TRUE(readPackCryptoParams(header, params));
    EXPECT_EQ(params.iterations, PACK_DEFAULT_KDF_ITERATIONS);

    // keyHash 是 sha256(key)：既不是密钥本身，也不是口令
    const auto key = derivePackKey(kPassword, params);
    const std::vector<uint8_t> storedKeyHash(header.keyHash, header.keyHash + sizeof(header.keyHash));
    EXPECT_EQ(storedKeyHash, packKeyFingerprint(key));
    EXPECT_NE(storedKeyHash, key);

    // 明文与口令都不许出现在产物里
    const std::string asText = textOf(resource);
    EXPECT_EQ(asText.find(kScript), std::string::npos);
    EXPECT_EQ(asText.find(kPassword), std::string::npos);
}

TEST(ResourcePackBuildTest, EncryptionRequiresAPassword) {
    EXPECT_THROW(Packer::buildResourceBytes(bytesOf(kScript), true, true, ""), std::runtime_error);
}

TEST(ResourcePackBuildTest, SameInputTwiceProducesDifferentBytes) {
    const auto a = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    const auto b = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    EXPECT_NE(a, b) << "随机 salt/IV 失效的话，同口令同明文会产出可辨识的固定密文";
}

TEST(ResourcePackBuildTest, CompressionFlagOnlySetWhenItPays) {
    // 不可压缩的数据：置了 COMPRESSED 会让读侧去解一段没压过的字节
    const std::vector<uint8_t> incompressible = wingman::crypt::randomBytes(4096);
    const auto header = readHeaderOf(Packer::buildResourceBytes(incompressible, false, true, ""));
    EXPECT_EQ(header.flags & PACK_FLAG_COMPRESSED, 0u);
    EXPECT_EQ(header.compressedSize, incompressible.size());

    // 可压缩数据则应真的压小并置位
    const std::string repetitive(4096, 'A');
    const auto packedHeader = readHeaderOf(Packer::buildResourceBytes(bytesOf(repetitive), false, true, ""));
    EXPECT_NE(packedHeader.flags & PACK_FLAG_COMPRESSED, 0u);
    EXPECT_LT(packedHeader.compressedSize, repetitive.size());
}

// ========== 往返：打包 → 加载 ==========

class ResourcePackRoundTripTest : public ::testing::TestWithParam<bool> {};

TEST_P(ResourcePackRoundTripTest, PlainAndEncryptedBothRestoreExactBytes) {
    const bool encrypt = GetParam();
    ErrorSink errors;
    const auto resource =
        Packer::buildResourceBytes(bytesOf(kScript), encrypt, true, encrypt ? kPassword : "");

    const auto loaded =
        ResourceLoader::loadScriptFromBytes(resource, encrypt ? kPassword : "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_TRUE(errors.message.empty());
    EXPECT_EQ(textOf(loaded->data), kScript);
    EXPECT_EQ(loaded->name, "embedded");
    EXPECT_FALSE(loaded->isBytecode);
}

TEST_P(ResourcePackRoundTripTest, LargeIncompressiblePayloadRoundTrips) {
    const bool encrypt = GetParam();
    const std::vector<uint8_t> payload = wingman::crypt::randomBytes(20000);
    ErrorSink errors;

    const auto resource =
        Packer::buildResourceBytes(payload, encrypt, true, encrypt ? kPassword : "");
    // 压不动 → 不带 COMPRESSED 标志，但读取路径仍要一致
    EXPECT_EQ(readHeaderOf(resource).flags & PACK_FLAG_COMPRESSED, 0u);

    const auto loaded =
        ResourceLoader::loadScriptFromBytes(resource, encrypt ? kPassword : "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_EQ(loaded->data, payload);
}

TEST_P(ResourcePackRoundTripTest, LargeCompressiblePayloadRoundTrips) {
    const bool encrypt = GetParam();
    const std::string big = textWithByteRuns();
    ErrorSink errors;

    const auto resource =
        Packer::buildResourceBytes(bytesOf(big), encrypt, true, encrypt ? kPassword : "");
    const auto header = readHeaderOf(resource);
    // 两个标志同时置位：变换顺序是 compress → encrypt，故加密包也真能压小
    ASSERT_NE(header.flags & PACK_FLAG_COMPRESSED, 0u);
    ASSERT_EQ(header.flags & PACK_FLAG_ENCRYPTED, encrypt ? PACK_FLAG_ENCRYPTED : 0u);
    ASSERT_LT(header.compressedSize, big.size());

    const auto loaded =
        ResourceLoader::loadScriptFromBytes(resource, encrypt ? kPassword : "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_EQ(loaded->data, bytesOf(big));
    EXPECT_EQ(header.originalSize, big.size());
}

TEST_P(ResourcePackRoundTripTest, PayloadWithoutByteRunsStaysUncompressedAndStillRoundTrips) {
    // 锁住一个容易误判的事实：普通脚本源码（无 ≥4 字节的同一字节连串）压不动，
    // 于是 COMPRESSED 标志不置位、负载原样存放——「压缩失败」不是加载失败的原因。
    const bool encrypt = GetParam();
    std::string source;
    for (int i = 0; i < 2000; ++i) {
        source += "local v" + std::to_string(i) + " = compute(i) -- repeat repeat repeat\n";
    }
    ErrorSink errors;

    const auto resource =
        Packer::buildResourceBytes(bytesOf(source), encrypt, true, encrypt ? kPassword : "");
    const auto header = readHeaderOf(resource);
    EXPECT_EQ(header.flags & PACK_FLAG_COMPRESSED, 0u);
    EXPECT_EQ(header.compressedSize, payloadOf(resource).size());

    const auto loaded =
        ResourceLoader::loadScriptFromBytes(resource, encrypt ? kPassword : "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_EQ(loaded->data, bytesOf(source));
}

TEST_P(ResourcePackRoundTripTest, EmptyPayloadRoundTrips) {
    // 空脚本不是合法产物（build() 拒绝），但格式本身要能表达：空明文 ≠ 加载失败
    const bool encrypt = GetParam();
    ErrorSink errors;
    const auto resource = Packer::buildResourceBytes({}, encrypt, true, encrypt ? kPassword : "");

    const auto loaded =
        ResourceLoader::loadScriptFromBytes(resource, encrypt ? kPassword : "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_TRUE(loaded->data.empty());
    EXPECT_FALSE(loaded->isBytecode);
}

TEST_P(ResourcePackRoundTripTest, BytecodeSignatureIsDetectedAfterDecoding) {
    const bool encrypt = GetParam();
    const std::vector<uint8_t> bytecode = {0x1B, 'L', 'u', 'a', 0x54, 0x00, 0x19, 0x93, 0x01, 0x02};
    ErrorSink errors;

    const auto resource =
        Packer::buildResourceBytes(bytecode, encrypt, true, encrypt ? kPassword : "");
    const auto loaded =
        ResourceLoader::loadScriptFromBytes(resource, encrypt ? kPassword : "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_TRUE(loaded->isBytecode);
    EXPECT_EQ(loaded->data, bytecode);
}

INSTANTIATE_TEST_SUITE_P(Modes, ResourcePackRoundTripTest, ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                             return info.param ? "Encrypted" : "Plain";
                         });

// ========== 负路径：口令与篡改 ==========

TEST(ResourcePackFailureTest, WrongPasswordIsRejected) {
    ErrorSink errors;
    const auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, "totally-wrong", errors.callback());
    EXPECT_FALSE(loaded.has_value());
    // 口令指纹在解密前就拒（GCM 标签随后也会拒；这里要的是那条能区分「口令错」的信息）
    EXPECT_NE(errors.message.find("Incorrect password"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, MissingPasswordIsRejected) {
    ErrorSink errors;
    const auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, "", errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("requires a password"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, TamperedCiphertextFailsAuthentication) {
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    resource.back() ^= 0xFF;  // GCM 标签随负载存放在末尾

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("authentication failed"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, TamperedSaltReadsAsWrongPassword) {
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    resource[reservedOffset() + PACK_CRYPTO_SALT_OFFSET] ^= 0xFF;

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    // salt 变了 → 派生出另一把密钥 → 指纹先拒。省掉一次无用 AES 运算，
    // 更重要的是别把「头部被改」报成「数据损坏」让人去查磁盘。
    EXPECT_NE(errors.message.find("Incorrect password"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, TamperedIvFailsAuthentication) {
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    resource[reservedOffset() + PACK_CRYPTO_IV_OFFSET] ^= 0xFF;

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    // IV 不参与派生，密钥指纹仍然匹配 → 由 GCM 标签兜住（认证加密连 IV 一起认证）
    EXPECT_NE(errors.message.find("authentication failed"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, TamperedIterationsRejected) {
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    // 迭代次数（reserved[28,32)）改成越界值：头部没坏，但拒绝按它派生
    writeLe32(resource.data() + reservedOffset() + PACK_CRYPTO_ITERATIONS_OFFSET,
              PACK_MAX_KDF_ITERATIONS + 1);

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("crypto parameters"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, TruncatedPayloadRejected) {
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);
    resource.pop_back();

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("Payload size mismatch"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, PlainPackTamperingCaughtByDataHash) {
    // 未加密包没有 GCM 标签可依赖，完整性全靠 dataHash ——
    // 这条在 Linux 上是真检查：旧实现的校验在非 Windows 分支恒返回 true
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), false, false, "");
    resource[sizeof(PACK_HEADER)] ^= 0xFF;

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, "", errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("Hash verification failed"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, HeaderClaimingCompressionIsRejected) {
    // 头部不在 GCM 认证范围内，v1 未加密包也没有标签可依赖：
    // 改写 flags 谎称压缩，最终由「解出来的字节对不上 originalSize/dataHash」兜住。
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), false, false, "");
    resource[offsetof(PACK_HEADER, flags)] ^= PACK_FLAG_COMPRESSED;  // 未加密包的 flags 本是 0

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, "", errors.callback());
    ASSERT_FALSE(loaded.has_value());
    EXPECT_TRUE(errors.message.find("Decoded size mismatch") != std::string::npos ||
                errors.message.find("Hash verification failed") != std::string::npos)
        << errors.message;
}

TEST(ResourcePackFailureTest, HeaderHidingCompressionIsRejected) {
    // 头部不在 GCM 认证范围内：把加密包的 COMPRESSED 位清掉，解密会「成功」，
    // 但拿到的是压缩字节 —— 由 originalSize / dataHash 两层拦下。
    ErrorSink errors;
    const std::string big = textWithByteRuns();
    auto resource = Packer::buildResourceBytes(bytesOf(big), true, true, kPassword);
    ASSERT_NE(readHeaderOf(resource).flags & PACK_FLAG_COMPRESSED, 0u);
    resource[offsetof(PACK_HEADER, flags)] &= ~PACK_FLAG_COMPRESSED;

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    ASSERT_FALSE(loaded.has_value());
    EXPECT_TRUE(errors.message.find("Decoded size mismatch") != std::string::npos ||
                errors.message.find("Hash verification failed") != std::string::npos)
        << errors.message;
}

TEST(ResourcePackFailureTest, ForgedKeyFingerprintStillFailsGcm) {
    // 口令指纹只是解密前的快速判定，不是安全边界：改写 keyHash 让它等于「攻击者那把密钥」
    // 的指纹，配合该密钥派生出的口令即可通过预检——但密文仍解不开（GCM 标签是权威）。
    const std::string decoyPassword = "attacker-chosen-password";
    ErrorSink errors;
    auto resource = Packer::buildResourceBytes(bytesOf(kScript), true, true, kPassword);

    PackCryptoParams params;
    ASSERT_TRUE(readPackCryptoParams(readHeaderOf(resource), params));
    const auto decoyFingerprint = packKeyFingerprint(derivePackKey(decoyPassword, params));
    std::copy(decoyFingerprint.begin(), decoyFingerprint.end(),
              resource.begin() + offsetof(PACK_HEADER, keyHash));

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, decoyPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_EQ(errors.message.find("Incorrect password"), std::string::npos)
        << "预检已被自洽指纹绕过，失败原因应是认证失败：" << errors.message;
    EXPECT_NE(errors.message.find("authentication failed"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, BadMagicAndShortInputRejected) {
    ErrorSink errors;
    std::vector<uint8_t> garbage(200, 0x5A);
    EXPECT_FALSE(ResourceLoader::loadScriptFromBytes(garbage, "", errors.callback()).has_value());
    EXPECT_NE(errors.message.find("magic"), std::string::npos) << errors.message;

    errors.message.clear();
    const std::vector<uint8_t> tooSmall(16, 0);
    EXPECT_FALSE(ResourceLoader::loadScriptFromBytes(tooSmall, "", errors.callback()).has_value());
    EXPECT_NE(errors.message.find("too small"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, LegacyV1EncryptedPackIsRejectedOutright) {
    // v1 的随机密钥只留下 sha256(key)，构造上不可恢复；当年 build() 也拒绝 encrypt，
    // 所以合法产物里不存在这种包 —— 遇到就是损坏或伪造，不做任何「猜密钥」的回退。
    ErrorSink errors;
    const auto resource = makeRawPack(PACK_FORMAT_VERSION_LEGACY, PACK_FLAG_ENCRYPTED, bytesOf(kScript),
                                      sha256Bytes(bytesOf(kScript)));

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, kPassword, errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("Legacy v1"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, UnknownVersionRejected) {
    ErrorSink errors;
    const auto resource = makeRawPack(PACK_FORMAT_VERSION + 1, 0, bytesOf(kScript),
                                      sha256Bytes(bytesOf(kScript)));

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, "", errors.callback());
    EXPECT_FALSE(loaded.has_value());
    EXPECT_NE(errors.message.find("Unsupported pack format version"), std::string::npos) << errors.message;
}

TEST(ResourcePackFailureTest, V1PlainPackStillLoads) {
    // 向后兼容的另一半：历史 v1 未加密包（无加密参数、无指纹）现在仍要能加载
    ErrorSink errors;
    const auto resource = makeRawPack(PACK_FORMAT_VERSION_LEGACY, 0, bytesOf(kScript),
                                      sha256Bytes(bytesOf(kScript)));

    const auto loaded = ResourceLoader::loadScriptFromBytes(resource, "", errors.callback());
    ASSERT_TRUE(loaded.has_value()) << errors.message;
    EXPECT_EQ(textOf(loaded->data), kScript);
}

}  // namespace
}  // namespace wingman::runtime
