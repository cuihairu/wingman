#include "wingman/verification.hpp"
#include "wingman/config.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <regex>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#else
#include <openssl/hmac.h>
#include <openssl/evp.h>
#endif

namespace wingman {

// ========== Base32 编解码 ==========

static const char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static std::string base32Decode(const std::string& encoded) {
    std::string result;
    int buffer = 0;
    int bitsLeft = 0;

    for (char c : encoded) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;

        const char* p = std::strchr(BASE32_ALPHABET, std::toupper(c));
        if (!p) continue;  // 跳过非法字符

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

// 简单的 SHA1 实现（用于 HMAC）
static std::vector<uint8_t> sha1(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> result(20);

#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return result;
    }

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
    // OpenSSL
    unsigned int len = 20;
    EVP_Digest(data.data(), data.size(), result.data(), &len, EVP_sha1(), nullptr);
#endif

    return result;
}

static std::vector<uint8_t> hmacSha1(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    // HMAC-SHA1 实现 (RFC 2104)
    const size_t blockSize = 64;  // SHA1 block size

    std::vector<uint8_t> keyCopy(key);

    // 如果 key 长于 block size，先 hash
    if (key.size() > blockSize) {
        keyCopy = sha1(key);
    }

    // 填充 key 到 block size
    keyCopy.resize(blockSize, 0);

    // 生成 ipad 和 opad
    std::vector<uint8_t> ipad(blockSize, 0x36);
    std::vector<uint8_t> opad(blockSize, 0x5c);

    for (size_t i = 0; i < blockSize; i++) {
        ipad[i] ^= keyCopy[i];
        opad[i] ^= keyCopy[i];
    }

    // 内层 hash: SHA1(key ^ ipad || data)
    std::vector<uint8_t> inner;
    inner.insert(inner.end(), ipad.begin(), ipad.end());
    inner.insert(inner.end(), data.begin(), data.end());
    std::vector<uint8_t> innerHash = sha1(inner);

    // 外层 hash: SHA1(key ^ opad || innerHash)
    std::vector<uint8_t> outer;
    outer.insert(outer.end(), opad.begin(), opad.end());
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());

    return sha1(outer);
}

// ========== TOTP 算法 ==========

static uint64_t getCurrentTimeCounter(int period = 30) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return static_cast<uint64_t>(timestamp / period);
}

static std::string generateTOTPInternal(const std::string& secret, int digits, int period, uint64_t timeCounter) {
    // 1. 解码 Base32 密钥
    std::string key = base32Decode(secret);
    if (key.empty()) {
        return "";
    }

    // 2. 将时间计数器转为 8 字节大端序
    std::vector<uint8_t> timeBytes(8);
    for (int i = 7; i >= 0; i--) {
        timeBytes[i] = timeCounter & 0xFF;
        timeCounter >>= 8;
    }

    // 3. 计算 HMAC-SHA1
    std::vector<uint8_t> keyBytes(key.begin(), key.end());
    std::vector<uint8_t> hmac = hmacSha1(keyBytes, timeBytes);

    // 4. 动态截取
    int offset = hmac[19] & 0x0F;
    int binary = ((hmac[offset] & 0x7F) << 24)
               | ((hmac[offset + 1] & 0xFF) << 16)
               | ((hmac[offset + 2] & 0xFF) << 8)
               | (hmac[offset + 3] & 0xFF);

    // 5. 生成验证码
    int otp = binary % static_cast<int>(std::pow(10, digits));
    std::ostringstream oss;
    oss << std::setw(digits) << std::setfill('0') << otp;
    return oss.str();
}

// ========== Steam Guard 特殊实现 ==========

// Steam Guard 使用 modified Base64 和时间偏移
static std::string base64DecodeSteam(const std::string& encoded) {
    // Steam 的 Base64 变体：替换字符
    std::string normalized = encoded;
    for (char& c : normalized) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    // 标准 Base64 解码
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
    // Steam 使用自己的编码方式
    std::string key = base64DecodeSteam(secret);
    if (key.empty()) {
        // 尝试标准 Base32
        key = base32Decode(secret);
        if (key.empty()) return "";
    }

    // Steam Guard 时间偏移（Unix 时间戳修正）
    timeCounter = timeCounter & 0xFFFFFFFF;

    // 将时间计数器转为 8 字节大端序
    std::vector<uint8_t> timeBytes(8);
    for (int i = 7; i >= 0; i--) {
        timeBytes[i] = timeCounter & 0xFF;
        timeCounter >>= 8;
    }

    // 计算 HMAC-SHA1
    std::vector<uint8_t> keyBytes(key.begin(), key.end());
    std::vector<uint8_t> hmac = hmacSha1(keyBytes, timeBytes);

    // Steam 使用特定的截取方式
    int offset = hmac[19] & 0x0F;
    int binary = ((hmac[offset] & 0x7F) << 24)
               | ((hmac[offset + 1] & 0xFF) << 16)
               | ((hmac[offset + 2] & 0xFF) << 8)
               | (hmac[offset + 3] & 0xFF);

    // Steam Guard 是 5 位数字
    int otp = binary % 100000;
    std::ostringstream oss;
    oss << std::setw(5) << std::setfill('0') << otp;
    return oss.str();
}

// ========== Implementation ==========

class VerificationManager::Impl {
public:
    std::string configDir;

    Impl(const std::string& configDir = "config") : configDir(configDir) {
        std::filesystem::create_directories(configDir);
    }

    std::string getConfigPath() const {
        return (std::filesystem::path(configDir) / "verification.json").string();
    }

    nlohmann::json loadConfig() const {
        std::string path = getConfigPath();
        if (!std::filesystem::exists(path)) {
            return nlohmann::json::object();
        }

        try {
            std::ifstream file(path);
            nlohmann::json j;
            file >> j;
            return j;
        } catch (...) {
            return nlohmann::json::object();
        }
    }

    bool saveConfig(const nlohmann::json& j) const {
        try {
            std::string path = getConfigPath();
            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << j.dump(2);
            return true;
        } catch (...) {
            return false;
        }
    }
};

VerificationManager::VerificationManager()
    : impl_(std::make_unique<Impl>("config")) {}

VerificationManager::~VerificationManager() = default;

// ========== TOTP 实现 ==========

std::string VerificationManager::generateTOTP(const TOTPConfig& config) {
    uint64_t timeCounter = getCurrentTimeCounter(config.period);
    return generateTOTPInternal(config.secret, config.digits, config.period, timeCounter);
}

std::string VerificationManager::generateTOTP(const std::string& account) {
    auto config = loadTOTP(account);
    if (!config) return "";
    return generateTOTP(*config);
}

bool VerificationManager::verifyTOTP(const TOTPConfig& config, const std::string& code, int window) {
    uint64_t timeCounter = getCurrentTimeCounter(config.period);

    // 检查当前窗口和前后各 window 个窗口
    for (int i = -window; i <= window; i++) {
        std::string generated = generateTOTPInternal(config.secret, config.digits, config.period, timeCounter + i);
        if (generated == code) return true;
    }
    return false;
}

bool VerificationManager::verifyTOTP(const std::string& account, const std::string& code, int window) {
    auto config = loadTOTP(account);
    if (!config) return false;
    return verifyTOTP(*config, code, window);
}

int VerificationManager::getRemainingSeconds(const TOTPConfig& config) {
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return config.period - (timestamp % config.period);
}

std::string VerificationManager::generateSteamGuard(const std::string& secret) {
    uint64_t timeCounter = getCurrentTimeCounter(30);
    return generateSteamGuardInternal(secret, timeCounter);
}

std::string VerificationManager::generateSteamGuardTime(const std::string& secret, uint64_t time) {
    return generateSteamGuardInternal(secret, time / 30);
}

// ========== 邮件实现（占位） ==========

std::optional<std::string> VerificationManager::getEmailCode(const EmailConfig& config) {
    // TODO: 实现 IMAP 邮件读取
    // 需要 libcurl 或其他 IMAP 库
    std::cout << "[EMAIL] Reading email from " << config.imapServer << "\n";
    return std::nullopt;
}

void VerificationManager::getEmailCodeAsync(const EmailConfig& config,
                                            std::function<void(std::optional<std::string>)> callback) {
    // TODO: 实现异步邮件监听
    std::thread([this, config, callback]() {
        auto code = getEmailCode(config);
        callback(code);
    }).detach();
}

void VerificationManager::stopEmailListener() {
    // TODO: 停止邮件监听
}

// ========== 配置管理 ==========

bool VerificationManager::saveTOTP(const std::string& account, const TOTPConfig& config) {
    nlohmann::json j = impl_->loadConfig();

    if (!j.contains("totp")) j["totp"] = nlohmann::json::object();

    nlohmann::json configJson;
    configJson["type"] = static_cast<int>(config.type);
    configJson["secret"] = config.secret;
    configJson["digits"] = config.digits;
    configJson["period"] = config.period;
    configJson["account"] = config.account;

    j["totp"][account] = configJson;
    return impl_->saveConfig(j);
}

std::optional<TOTPConfig> VerificationManager::loadTOTP(const std::string& account) {
    nlohmann::json j = impl_->loadConfig();

    if (!j.contains("totp") || !j["totp"].contains(account)) {
        return std::nullopt;
    }

    try {
        nlohmann::json configJson = j["totp"][account];
        TOTPConfig config;
        config.type = static_cast<TOTPType>(configJson.value("type", 0));
        config.secret = configJson.value("secret", "");
        config.digits = configJson.value("digits", 6);
        config.period = configJson.value("period", 30);
        config.account = configJson.value("account", account);
        return config;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> VerificationManager::listTOTPAccounts() {
    nlohmann::json j = impl_->loadConfig();
    std::vector<std::string> accounts;

    if (j.contains("totp") && j["totp"].is_object()) {
        for (auto& [key, _] : j["totp"].items()) {
            accounts.push_back(key);
        }
    }

    return accounts;
}

bool VerificationManager::removeTOTP(const std::string& account) {
    nlohmann::json j = impl_->loadConfig();

    if (j.contains("totp") && j["totp"].contains(account)) {
        j["totp"].erase(account);
        return impl_->saveConfig(j);
    }

    return false;
}

bool VerificationManager::saveEmail(const std::string& account, const EmailConfig& config) {
    nlohmann::json j = impl_->loadConfig();

    if (!j.contains("email")) j["email"] = nlohmann::json::object();

    nlohmann::json configJson;
    configJson["imapServer"] = config.imapServer;
    configJson["imapPort"] = config.imapPort;
    configJson["useSSL"] = config.useSSL;
    configJson["username"] = config.username;
    configJson["password"] = config.password;
    configJson["senderFilter"] = config.senderFilter;
    configJson["timeoutSeconds"] = config.timeoutSeconds;

    j["email"][account] = configJson;
    return impl_->saveConfig(j);
}

std::optional<EmailConfig> VerificationManager::loadEmail(const std::string& account) {
    nlohmann::json j = impl_->loadConfig();

    if (!j.contains("email") || !j["email"].contains(account)) {
        return std::nullopt;
    }

    try {
        nlohmann::json configJson = j["email"][account];
        EmailConfig config;
        config.imapServer = configJson.value("imapServer", "");
        config.imapPort = configJson.value("imapPort", 993);
        config.useSSL = configJson.value("useSSL", true);
        config.username = configJson.value("username", "");
        config.password = configJson.value("password", "");
        config.senderFilter = configJson.value("senderFilter", "");
        config.timeoutSeconds = configJson.value("timeoutSeconds", 60);
        return config;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace wingman
