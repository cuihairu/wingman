#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace wingman::crypt {

/// ========== AES-256-GCM Encryption ==========
///
/// Proper encryption using AES-256-GCM (authenticated encryption)
/// - Uses AES-256 with Galois/Counter Mode for authentication
/// - PBKDF2 with SHA-256 for key derivation (100,000 iterations)
/// - Random 96-bit IV for each encryption
/// - Returns base64-encoded ciphertext with IV and auth tag
///

/// Encrypt plaintext using AES-256-GCM
///
/// @param plaintext The data to encrypt
/// @param password The password (will be derived using PBKDF2)
/// @param salt Optional salt (hex string). If empty, generates random salt.
/// @return Base64-encoded string (salt + IV + ciphertext + auth tag), empty on error
std::string encryptAES(const std::string& plaintext, const std::string& password, const std::string& salt = "");

/// Decrypt AES-256-GCM encrypted data
///
/// @param ciphertext Base64-encoded string (salt + IV + ciphertext + auth tag)
/// @param password The password used for encryption
/// @return Decrypted plaintext, empty on error
std::string decryptAES(const std::string& ciphertext, const std::string& password);

/// ========== AES-256-GCM (raw bytes, caller-supplied key) ==========
///
/// 与 encryptAES/decryptAES 的分工：那两个是「口令进、base64 串出」的自描述格式
/// （salt + IV 内嵌在载荷里，PBKDF2 迭代次数固定）。下面这对只吃/吐原始字节，
/// 密钥与 IV 由调用方给出——供需要自定义密钥派生与二进制头部布局的格式使用
/// （如 runtime 打包资源 PACK_HEADER，见 apps/runtime/include/wingman/runtime/resource_pack.hpp）。
///

/// Encrypt with AES-256-GCM using a caller-provided key
///
/// @param key 32-byte key (AES-256)
/// @param iv Initialization vector, 1..16 bytes (12 recommended by GCM)
/// @param plaintext Data to encrypt (may be empty)
/// @return ciphertext || 16-byte auth tag
/// @throws std::invalid_argument on wrong key/iv length
/// @throws std::runtime_error when an OpenSSL call fails
///
/// 长度上限：明文按 int 传给 OpenSSL EVP_*Update，故单次调用需 < 2 GiB。
/// 本函数的调用方是 PE 资源里的脚本载荷（DWORD 尺寸字段、实际量级为 KB），
/// 不做运行时守卫——真要越界，packer 在读取脚本文件时就会先失败。
std::vector<uint8_t> aesGcmEncrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& iv,
                                   const std::vector<uint8_t>& plaintext);

/// Decrypt AES-256-GCM data and verify the auth tag
///
/// @param key 32-byte key used for encryption
/// @param iv Initialization vector used for encryption
/// @param ciphertextWithTag ciphertext || 16-byte auth tag
/// @return plaintext (may be empty)
/// @throws std::invalid_argument on wrong key/iv/input length
/// @throws std::runtime_error when authentication fails (wrong key or tampered
///         data) or an OpenSSL call fails
std::vector<uint8_t> aesGcmDecrypt(const std::vector<uint8_t>& key,
                                   const std::vector<uint8_t>& iv,
                                   const std::vector<uint8_t>& ciphertextWithTag);

/// ========== Key Derivation ==========

/// Derive a key from password using PBKDF2
///
/// @param password The password
/// @param salt The salt (hex string)
/// @param iterations Number of PBKDF2 iterations (default: 100000)
/// @param keyLen Desired key length in bytes (default: 32 for AES-256)
/// @return Derived key as hex string
std::string deriveKey(const std::string& password, const std::string& salt, int iterations = 100000, size_t keyLen = 32);

/// Generate random salt for key derivation
///
/// @param length Salt length in bytes (default: 16)
/// @return Salt as hex string
std::string generateSalt(size_t length = 16);

/// ========== Utility Functions ==========

/// Encode data to base64
std::string base64Encode(const std::vector<uint8_t>& data);

/// Decode base64 to data
std::vector<uint8_t> base64Decode(const std::string& encoded);

/// Convert bytes to hex string
std::string bytesToHex(const std::vector<uint8_t>& bytes);

/// Convert hex string to bytes
std::vector<uint8_t> hexToBytes(const std::string& hex);

/// Generate random bytes
std::vector<uint8_t> randomBytes(size_t length);

/// Hash data using SHA-256
std::string sha256(const std::string& data);

/// Hash data using SHA-512
std::string sha512(const std::string& data);

} // namespace wingman::crypt
