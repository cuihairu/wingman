#include "wingman/crypt.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/err.h>
#include <openssl/aes.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <iomanip>
#include <memory>
#include <random>
#include <stdexcept>
#include <cstring>

namespace wingman::crypt {

namespace {
    // Base64 encoding table
    static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // Base64 decode table
    static const int8_t base64DecodeTable[256] = {
        /* 0-15   */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* 16-31  */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* 32-47  */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        /* 48-57  */ 52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        /* 64-79  */ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        /* 80-95  */ 15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        /* 96-111 */ -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        /* 112-127*/ 41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        /* 128+   */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
}

// ========== Utility Functions ==========

std::string base64Encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        // 剩余字节数（1、2 或 3）
        const size_t remaining = data.size() - i;
        const size_t chunk = remaining < 3 ? remaining : 3;

        // 将字节打包到高位，不足 3 字节时低位补零
        uint32_t value = 0;
        for (size_t j = 0; j < chunk; ++j) {
            value = (value << 8) | static_cast<uint8_t>(data[i + j]);
        }
        value <<= (3 - chunk) * 8;

        for (size_t j = 0; j < 4; ++j) {
            size_t index = (value >> (6 * (3 - j))) & 0x3F;
            // 每个有效字节贡献 4 个 base64 字符中的 8/6 个：
            // 1 字节 -> 2 个实字符 + 2 个 '='；2 字节 -> 3 个实字符 + 1 个 '='
            if (j < (chunk * 8 + 5) / 6) {
                result += base64Chars[index];
            } else {
                result += '=';
            }
        }
    }

    return result;
}

std::vector<uint8_t> base64Decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    int value = 0;
    int bits = 0;

    for (unsigned char c : encoded) {
        if (c == '=') break;
        int8_t index = base64DecodeTable[c];
        if (index < 0) continue;

        // 掩码限制在 24 位窗口内，避免长输入累积溢出
        value = ((value << 6) | index) & 0xFFFFFF;
        bits += 6;

        if (bits >= 8) {
            result.push_back(static_cast<uint8_t>((value >> (bits - 8)) & 0xFF));
            bits -= 8;
        }
    }

    return result;
}

std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::vector<uint8_t> randomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    if (RAND_bytes(bytes.data(), static_cast<int>(length)) != 1) {
        spdlog::error("[Crypto] Failed to generate random bytes: {}", ERR_error_string(ERR_get_error(), nullptr));
        return {};
    }
    return bytes;
}

std::string sha256(const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    return bytesToHex(std::vector<uint8_t>(hash, hash + hashLen));
}

std::string sha512(const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha512(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    return bytesToHex(std::vector<uint8_t>(hash, hash + hashLen));
}

std::string generateSalt(size_t length) {
    auto bytes = randomBytes(length);
    if (bytes.empty()) {
        return "";
    }
    return bytesToHex(bytes);
}

// ========== Key Derivation ==========

std::string deriveKey(const std::string& password, const std::string& salt, int iterations, size_t keyLen) {
    auto saltBytes = hexToBytes(salt);
    if (saltBytes.empty()) {
        spdlog::error("[Crypto] Invalid salt");
        return "";
    }

    std::vector<uint8_t> key(keyLen);

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);

    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_octet_string("pass", const_cast<char*>(password.c_str()), password.size());
    params[1] = OSSL_PARAM_construct_octet_string("salt", saltBytes.data(), saltBytes.size());
    uint32_t iterationsValue = static_cast<uint32_t>(iterations);
    params[2] = OSSL_PARAM_construct_uint32("iter", &iterationsValue);
    params[3] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
    params[4] = OSSL_PARAM_construct_end();

    if (EVP_KDF_derive(kctx, key.data(), keyLen, params) <= 0) {
        EVP_KDF_CTX_free(kctx);
        EVP_KDF_free(kdf);
        spdlog::error("[Crypto] Key derivation failed");
        return "";
    }

    EVP_KDF_CTX_free(kctx);
    EVP_KDF_free(kdf);

    return bytesToHex(key);
}

// ========== AES-256-GCM Encryption ==========

std::string encryptAES(const std::string& plaintext, const std::string& password, const std::string& salt) {
    const size_t keyLength = 32;      // 256 bits
    const size_t saltLength = 16;     // 128 bits
    const size_t ivLength = 12;       // 96 bits (recommended for GCM)
    const size_t tagLength = 16;      // 128 bits auth tag
    const int pbkdf2Iterations = 100000;

    try {
        // Generate or use provided salt
        std::vector<uint8_t> saltBytes;
        if (salt.empty()) {
            saltBytes = randomBytes(saltLength);
            if (saltBytes.empty()) {
                return "";
            }
        } else {
            saltBytes = hexToBytes(salt);
            if (saltBytes.size() != saltLength) {
                spdlog::error("[Crypto] Invalid salt length (expected {} bytes)", saltLength);
                return "";
            }
        }

        // Derive key using PBKDF2
        std::vector<uint8_t> key(keyLength);
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
        EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);

        OSSL_PARAM params[5];
        params[0] = OSSL_PARAM_construct_octet_string("pass", const_cast<char*>(password.c_str()), password.size());
        params[1] = OSSL_PARAM_construct_octet_string("salt", saltBytes.data(), saltBytes.size());
        uint32_t iterationsValue = static_cast<uint32_t>(pbkdf2Iterations);
        params[2] = OSSL_PARAM_construct_uint32("iter", &iterationsValue);
        params[3] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
        params[4] = OSSL_PARAM_construct_end();

        if (EVP_KDF_derive(kctx, key.data(), keyLength, params) <= 0) {
            EVP_KDF_CTX_free(kctx);
            EVP_KDF_free(kdf);
            spdlog::error("[Crypto] Key derivation failed");
            return "";
        }

        EVP_KDF_CTX_free(kctx);
        EVP_KDF_free(kdf);

        // Generate random IV
        auto iv = randomBytes(ivLength);
        if (iv.empty()) {
            return "";
        }

        // Encrypt using AES-256-GCM
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            spdlog::error("[Crypto] Failed to create cipher context");
            return "";
        }

        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to initialize cipher");
            return "";
        }

        // Set IV length
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(ivLength), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to set IV length");
            return "";
        }

        // Set key and IV
        if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to set key and IV");
            return "";
        }

        // Encrypt plaintext
        std::vector<uint8_t> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
        int len;
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, reinterpret_cast<const uint8_t*>(plaintext.data()), static_cast<int>(plaintext.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Encryption failed");
            return "";
        }

        int ciphertextLen = len;

        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Encryption finalization failed");
            return "";
        }
        ciphertextLen += len;
        ciphertext.resize(ciphertextLen);

        // Get auth tag
        std::vector<uint8_t> tag(tagLength);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tagLength), tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to get auth tag");
            return "";
        }

        EVP_CIPHER_CTX_free(ctx);

        // Format: salt(16) + iv(12) + ciphertext + tag(16)
        std::vector<uint8_t> result;
        result.reserve(saltLength + ivLength + ciphertext.size() + tagLength);
        result.insert(result.end(), saltBytes.begin(), saltBytes.end());
        result.insert(result.end(), iv.begin(), iv.end());
        result.insert(result.end(), ciphertext.begin(), ciphertext.end());
        result.insert(result.end(), tag.begin(), tag.end());

        return base64Encode(result);

    } catch (const std::exception& e) {
        spdlog::error("[Crypto] Encryption exception: {}", e.what());
        return "";
    }
}

std::string decryptAES(const std::string& ciphertext, const std::string& password) {
    const size_t keyLength = 32;
    const size_t saltLength = 16;
    const size_t ivLength = 12;
    const size_t tagLength = 16;
    const int pbkdf2Iterations = 100000;

    try {
        // Decode base64
        auto data = base64Decode(ciphertext);

        // Minimum size check
        if (data.size() < saltLength + ivLength + tagLength) {
            spdlog::error("[Crypto] Invalid ciphertext (too short)");
            return "";
        }

        // Extract components
        std::vector<uint8_t> salt(data.begin(), data.begin() + saltLength);
        std::vector<uint8_t> iv(data.begin() + saltLength, data.begin() + saltLength + ivLength);
        std::vector<uint8_t> encrypted(data.begin() + saltLength + ivLength, data.end() - tagLength);
        std::vector<uint8_t> tag(data.end() - tagLength, data.end());

        // Derive key using PBKDF2
        std::vector<uint8_t> key(keyLength);
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
        EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);

        OSSL_PARAM params[5];
        params[0] = OSSL_PARAM_construct_octet_string("pass", const_cast<char*>(password.c_str()), password.size());
        params[1] = OSSL_PARAM_construct_octet_string("salt", salt.data(), salt.size());
        uint32_t iterationsValue = static_cast<uint32_t>(pbkdf2Iterations);
        params[2] = OSSL_PARAM_construct_uint32("iter", &iterationsValue);
        params[3] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
        params[4] = OSSL_PARAM_construct_end();

        if (EVP_KDF_derive(kctx, key.data(), keyLength, params) <= 0) {
            EVP_KDF_CTX_free(kctx);
            EVP_KDF_free(kdf);
            spdlog::error("[Crypto] Key derivation failed");
            return "";
        }

        EVP_KDF_CTX_free(kctx);
        EVP_KDF_free(kdf);

        // Decrypt using AES-256-GCM
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            spdlog::error("[Crypto] Failed to create cipher context");
            return "";
        }

        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to initialize cipher");
            return "";
        }

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(ivLength), nullptr) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to set IV length");
            return "";
        }

        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to set key and IV");
            return "";
        }

        // Decrypt
        std::vector<uint8_t> plaintext(encrypted.size() + AES_BLOCK_SIZE);
        int len;
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, encrypted.data(), static_cast<int>(encrypted.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Decryption failed");
            return "";
        }

        int plaintextLen = len;

        // Set expected tag before finalization
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tagLength), tag.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            spdlog::error("[Crypto] Failed to set auth tag");
            return "";
        }

        // Finalize (this verifies the auth tag)
        int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
        EVP_CIPHER_CTX_free(ctx);

        if (ret <= 0) {
            spdlog::error("[Crypto] Authentication failed - data may be tampered or wrong password");
            return "";
        }

        plaintextLen += len;
        plaintext.resize(plaintextLen);

        return std::string(plaintext.begin(), plaintext.end());

    } catch (const std::exception& e) {
        spdlog::error("[Crypto] Decryption exception: {}", e.what());
        return "";
    }
}

// ========== AES-256-GCM (raw bytes, caller-supplied key) ==========

namespace {

constexpr size_t kGcmKeyLength = 32;      // AES-256
constexpr size_t kGcmTagLength = 16;      // 128-bit auth tag
constexpr size_t kGcmMaxIvLength = 16;    // 12 是推荐值；这里只拒绝明显不合理的长度

/// 统一 OpenSSL 返回值检查——本组函数以异常表达失败（调用方 runtime 打包/加载链
/// 整体在 try/catch 里，且原始字节载荷无法用「空串 = 失败」表达：空明文是合法输入）
inline void requireOpenSSL(int rc, const char* what) {
    if (rc == 1) {
        return;
    }
    const unsigned long err = ERR_get_error();
    if (err != 0) {
        throw std::runtime_error(std::string(what) + ": " + ERR_error_string(err, nullptr));
    }
    throw std::runtime_error(std::string(what) + ": OpenSSL rejected the request");
}

void checkGcmKeyAndIv(const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv) {
    if (key.size() != kGcmKeyLength) {
        throw std::invalid_argument("AES-GCM key must be 32 bytes");
    }
    if (iv.empty() || iv.size() > kGcmMaxIvLength) {
        throw std::invalid_argument("AES-GCM iv must be 1..16 bytes (12 recommended)");
    }
}

struct CipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const { EVP_CIPHER_CTX_free(ctx); }
};

} // namespace

std::vector<uint8_t> aesGcmEncrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& iv,
                                   const std::vector<uint8_t>& plaintext) {
    checkGcmKeyAndIv(key, iv);

    std::unique_ptr<EVP_CIPHER_CTX, CipherCtxDeleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    requireOpenSSL(EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr),
                   "Failed to initialize cipher");
    requireOpenSSL(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr),
                   "Failed to set IV length");
    requireOpenSSL(EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()),
                   "Failed to set key and IV");

    std::vector<uint8_t> out(plaintext.size() + AES_BLOCK_SIZE);
    int len = 0;
    if (!plaintext.empty()) {
        // 空 plaintext 时 .data() 可能为 nullptr，OpenSSL 不接受 null 指针参数（与 size 是否为 0 无关）
        requireOpenSSL(EVP_EncryptUpdate(ctx.get(), out.data(), &len, plaintext.data(),
                                         static_cast<int>(plaintext.size())),
                       "Encryption failed");
    }
    int written = len;
    requireOpenSSL(EVP_EncryptFinal_ex(ctx.get(), out.data() + written, &len),
                   "Encryption finalization failed");
    written += len;

    // 给 tag 留出位置后原地写入（EVP_CIPHER_CTX_ctrl 按指针写入，不改长度）
    out.resize(static_cast<size_t>(written) + kGcmTagLength);
    requireOpenSSL(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kGcmTagLength),
                                       out.data() + written),
                   "Failed to get auth tag");

    return out;
}

std::vector<uint8_t> aesGcmDecrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& iv,
                                   const std::vector<uint8_t>& ciphertextWithTag) {
    checkGcmKeyAndIv(key, iv);
    if (ciphertextWithTag.size() < kGcmTagLength) {
        throw std::invalid_argument("AES-GCM input is shorter than the auth tag");
    }

    const size_t cipherLength = ciphertextWithTag.size() - kGcmTagLength;
    std::vector<uint8_t> tag(ciphertextWithTag.begin() + static_cast<long>(cipherLength),
                             ciphertextWithTag.end());

    std::unique_ptr<EVP_CIPHER_CTX, CipherCtxDeleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }

    requireOpenSSL(EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr),
                   "Failed to initialize cipher");
    requireOpenSSL(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr),
                   "Failed to set IV length");
    requireOpenSSL(EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), iv.data()),
                   "Failed to set key and IV");

    std::vector<uint8_t> out(cipherLength + AES_BLOCK_SIZE);
    int len = 0;
    if (cipherLength > 0) {
        requireOpenSSL(EVP_DecryptUpdate(ctx.get(), out.data(), &len, ciphertextWithTag.data(),
                                         static_cast<int>(cipherLength)),
                       "Decryption failed");
    }
    int written = len;

    // SET_TAG 的接口签名要求非 const 指针，故上面先把 tag 拷进 vector
    requireOpenSSL(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kGcmTagLength), tag.data()),
                   "Failed to set auth tag");

    // 认证失败体现在 Final 的返回值上：口令错误或数据被篡改都走这里
    if (EVP_DecryptFinal_ex(ctx.get(), out.data() + written, &len) != 1) {
        throw std::runtime_error("AES-GCM authentication failed (wrong key or tampered data)");
    }
    written += len;

    out.resize(static_cast<size_t>(written));
    return out;
}

} // namespace wingman::crypt
