#include "wingman/verification.hpp"
#include <cstring>
#include <cstdint>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#else
#include <openssl/hmac.h>
#include <openssl/evp.h>
#endif

namespace wingman {

// ========== Base32 Decode ==========

static const char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static std::string base32Decode(const std::string& encoded) {
    std::string result;
    int buffer = 0;
    int bitsLeft = 0;

    for (char c : encoded) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;

        const char* p = std::strchr(BASE32_ALPHABET, std::toupper(c));
        if (!p) continue;

        int value = p - BASE32_ALPHABET;
        buffer = (buffer << 5) | value;
        bitsLeft += 5;

        if (bitsLeft >= 8) {
            result.push_back(static_cast<char>((buffer >> (bitsLeft - 8)) & 0xFF));
            bitsLeft -= 8;
        }
    }

    return result;
}

// ========== HMAC-SHA1 ==========

static std::vector<uint8_t> sha1(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result(20);

#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return result;
    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return result;
    }
    if (!CryptHashData(hHash, reinterpret_cast<const BYTE*>(data.data()), data.size(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return result;
    }

    DWORD hashLen = 20;
    CryptGetHashParam(hHash, HP_HASHVAL, reinterpret_cast<BYTE*>(result.data()), &hashLen, 0);

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
#else
    unsigned int len = 20;
    EVP_Digest(data.data(), data.size(), result.data(), &len, EVP_sha1(), nullptr);
#endif

    return result;
}

static std::vector<uint8_t> hmacSha1(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    const size_t blockSize = 64;

    std::vector<uint8_t> keyCopy(key);
    if (key.size() > blockSize)
        keyCopy = sha1(key);
    keyCopy.resize(blockSize, 0);

    std::vector<uint8_t> ipad(blockSize, 0x36);
    std::vector<uint8_t> opad(blockSize, 0x5c);

    for (size_t i = 0; i < blockSize; i++) {
        ipad[i] ^= keyCopy[i];
        opad[i] ^= keyCopy[i];
    }

    std::vector<uint8_t> inner;
    inner.insert(inner.end(), ipad.begin(), ipad.end());
    inner.insert(inner.end(), data.begin(), data.end());
    auto innerHash = sha1(inner);

    std::vector<uint8_t> outer;
    outer.insert(outer.end(), opad.begin(), opad.end());
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());
    return sha1(outer);
}

// ========== TOTP Core ==========

static uint64_t getCurrentTimeCounter(int period) {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return static_cast<uint64_t>(timestamp / period);
}

static std::string generateTOTPInternal(const std::string& secret, int digits, uint64_t timeCounter) {
    std::string key = base32Decode(secret);
    if (key.empty()) return "";

    std::vector<uint8_t> timeBytes(8);
    for (int i = 7; i >= 0; i--) {
        timeBytes[i] = timeCounter & 0xFF;
        timeCounter >>= 8;
    }

    std::vector<uint8_t> keyBytes(key.begin(), key.end());
    auto hmac = hmacSha1(keyBytes, timeBytes);

    int offset = hmac[19] & 0x0F;
    int binary = ((hmac[offset] & 0x7F) << 24)
               | ((hmac[offset + 1] & 0xFF) << 16)
               | ((hmac[offset + 2] & 0xFF) << 8)
               | (hmac[offset + 3] & 0xFF);

    int otp = binary % static_cast<int>(std::pow(10, digits));
    std::ostringstream oss;
    oss << std::setw(digits) << std::setfill('0') << otp;
    return oss.str();
}

// ========== Steam Guard ==========

static std::string base64DecodeSteam(const std::string& encoded) {
    std::string normalized = encoded;
    for (char& c : normalized) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    std::string result;
    int buffer = 0;
    int bitsLeft = 0;

    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (char c : normalized) {
        if (c == '=') break;
        size_t pos = alphabet.find(c);
        if (pos == std::string::npos) continue;

        buffer = (buffer << 6) | pos;
        bitsLeft += 6;

        if (bitsLeft >= 8) {
            result.push_back(static_cast<char>((buffer >> (bitsLeft - 8)) & 0xFF));
            bitsLeft -= 8;
        }
    }

    return result;
}

static std::string generateSteamGuardInternal(const std::string& secret, uint64_t timeCounter) {
    std::string key = base64DecodeSteam(secret);
    if (key.empty()) {
        key = base32Decode(secret);
        if (key.empty()) return "";
    }

    timeCounter = timeCounter & 0xFFFFFFFF;

    std::vector<uint8_t> timeBytes(8);
    for (int i = 7; i >= 0; i--) {
        timeBytes[i] = timeCounter & 0xFF;
        timeCounter >>= 8;
    }

    std::vector<uint8_t> keyBytes(key.begin(), key.end());
    auto hmac = hmacSha1(keyBytes, timeBytes);

    int offset = hmac[19] & 0x0F;
    int binary = ((hmac[offset] & 0x7F) << 24)
               | ((hmac[offset + 1] & 0xFF) << 16)
               | ((hmac[offset + 2] & 0xFF) << 8)
               | (hmac[offset + 3] & 0xFF);

    int otp = binary % 100000;
    std::ostringstream oss;
    oss << std::setw(5) << std::setfill('0') << otp;
    return oss.str();
}

// ========== Public API ==========

std::string generateTOTP(const std::string& secret, int digits, int period) {
    return generateTOTPInternal(secret, digits, getCurrentTimeCounter(period));
}

bool verifyTOTP(const std::string& secret, const std::string& code, int digits, int period, int window) {
    uint64_t timeCounter = getCurrentTimeCounter(period);
    for (int i = -window; i <= window; i++) {
        if (generateTOTPInternal(secret, digits, timeCounter + i) == code)
            return true;
    }
    return false;
}

int getRemainingSeconds(int period) {
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return period - static_cast<int>(timestamp % period);
}

std::string generateSteamGuard(const std::string& secret) {
    return generateSteamGuardInternal(secret, getCurrentTimeCounter(30));
}

} // namespace wingman
