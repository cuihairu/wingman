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
