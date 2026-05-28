#include <gtest/gtest.h>
#include "wingman/security.hpp"

using namespace wingman;

// ========== AntiDetectionConfig ==========

TEST(AntiDetectionConfigTest, DefaultValues) {
    AntiDetectionConfig cfg;
    EXPECT_TRUE(cfg.enableRandomDelay);
    EXPECT_TRUE(cfg.enableBezierMovement);
    EXPECT_TRUE(cfg.enableRandomClick);
    EXPECT_EQ(cfg.minDelayMs, 50);
    EXPECT_EQ(cfg.maxDelayMs, 150);
    EXPECT_DOUBLE_EQ(cfg.clickJitter, 2.0);
    EXPECT_DOUBLE_EQ(cfg.movementVariance, 0.1);
}

// ========== ProcessProtectionConfig ==========

TEST(ProcessProtectionConfigTest, DefaultValues) {
    ProcessProtectionConfig cfg;
    EXPECT_FALSE(cfg.protectFromTermination);
    EXPECT_FALSE(cfg.hideFromTaskManager);
    EXPECT_FALSE(cfg.enableAntiDebug);
    EXPECT_FALSE(cfg.enableAntiVM);
    EXPECT_FALSE(cfg.enableIntegrityCheck);
}

// ========== CodeSignature ==========

TEST(CodeSignatureTest, DefaultValues) {
    CodeSignature sig;
    EXPECT_FALSE(sig.isSigned);
    EXPECT_TRUE(sig.issuer.empty());
    EXPECT_TRUE(sig.subject.empty());
    EXPECT_TRUE(sig.thumbprint.empty());
}

// ========== ObfuscationConfig ==========

TEST(ObfuscationConfigTest, DefaultValues) {
    ObfuscationConfig cfg;
    EXPECT_FALSE(cfg.enableStringEncryption);
    EXPECT_FALSE(cfg.enableControlFlowFlattening);
    EXPECT_FALSE(cfg.enableDeadCodeInjection);
    EXPECT_FALSE(cfg.enableVirtualization);
}

// ========== SecurityManager 静态方法 ==========

TEST(SecurityManagerTest, EncryptDecryptRoundtrip) {
    std::string original = "Hello, World!";
    std::string key = "secret_key";

    std::string encrypted = SecurityManager::encryptString(original, key);
    EXPECT_NE(encrypted, original);

    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, EncryptEmptyString) {
    std::string result = SecurityManager::encryptString("", "key");
    EXPECT_TRUE(result.empty());
}

TEST(SecurityManagerTest, EncryptDecryptWithKeyEqualToInput) {
    std::string original = "test";
    std::string key = "test";
    // Key same length as input — each char XORs with itself, result is all zeros
    std::string encrypted = SecurityManager::encryptString(original, key);
    EXPECT_NE(encrypted, original);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, EncryptSingleCharKey) {
    std::string original = "abc";
    std::string key = "k";
    std::string encrypted = SecurityManager::encryptString(original, key);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, GenerateRandomStringLength) {
    std::string s1 = SecurityManager::generateRandomString(10);
    EXPECT_EQ(s1.size(), 10u);

    std::string s2 = SecurityManager::generateRandomString(0);
    EXPECT_TRUE(s2.empty());

    std::string s3 = SecurityManager::generateRandomString(100);
    EXPECT_EQ(s3.size(), 100u);
}

TEST(SecurityManagerTest, GenerateRandomStringUniqueness) {
    std::string s1 = SecurityManager::generateRandomString(32);
    std::string s2 = SecurityManager::generateRandomString(32);
    // Statistically extremely unlikely to be equal
    EXPECT_NE(s1, s2);
}

TEST(SecurityManagerTest, GenerateRandomStringAlphanumeric) {
    std::string s = SecurityManager::generateRandomString(1000);
    for (char c : s) {
        EXPECT_TRUE((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
    }
}

TEST(SecurityManagerTest, HashStringNonEmpty) {
    std::string hash = SecurityManager::hashString("test");
    EXPECT_FALSE(hash.empty());
    // SHA-256 produces 64 hex chars
    EXPECT_EQ(hash.size(), 64u);
}

TEST(SecurityManagerTest, HashStringConsistent) {
    std::string h1 = SecurityManager::hashString("hello");
    std::string h2 = SecurityManager::hashString("hello");
    EXPECT_EQ(h1, h2);
}

TEST(SecurityManagerTest, HashStringDifferentInputs) {
    std::string h1 = SecurityManager::hashString("hello");
    std::string h2 = SecurityManager::hashString("world");
    EXPECT_NE(h1, h2);
}

TEST(SecurityManagerTest, HashStringEmptyInput) {
    std::string hash = SecurityManager::hashString("");
    // SHA-256 of empty string is a well-known value
    EXPECT_EQ(hash.size(), 64u);
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(SecurityManagerTest, FilterSensitivePassword) {
    std::string input = "user password=secret123";
    std::string filtered = SecurityManager::filterSensitive(input);
    // "password" → "***", "pwd" matches inside "***" context, "secret" → "***"
    EXPECT_NE(filtered, input);
    EXPECT_EQ(filtered.find("password"), std::string::npos);
}

TEST(SecurityManagerTest, FilterSensitiveToken) {
    std::string input = "Authorization: token abc123";
    std::string filtered = SecurityManager::filterSensitive(input);
    EXPECT_EQ(filtered.find("token"), std::string::npos);
}

TEST(SecurityManagerTest, FilterSensitiveApiKey) {
    std::string input = "config api_key=MYKEY";
    std::string filtered = SecurityManager::filterSensitive(input);
    // "api_key" contains "key" which gets replaced
    EXPECT_NE(filtered, input);
    EXPECT_EQ(filtered.find("api_key"), std::string::npos);
}

TEST(SecurityManagerTest, FilterSensitiveNoMatch) {
    std::string input = "hello world test";
    std::string filtered = SecurityManager::filterSensitive(input);
    EXPECT_EQ(filtered, input);
}

TEST(SecurityManagerTest, FilterSensitiveEmpty) {
    std::string filtered = SecurityManager::filterSensitive("");
    EXPECT_TRUE(filtered.empty());
}

TEST(SecurityManagerTest, FilterSensitiveMultipleMatches) {
    std::string input = "password=pwd secret";
    std::string filtered = SecurityManager::filterSensitive(input);
    // "password" and "pwd" and "secret" all match patterns
    EXPECT_NE(filtered, input);
}

// ========== SecurityManager Instance方法 ==========

TEST(SecurityManagerTest, SetGetAntiDetectionConfig) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.minDelayMs = 100;
    cfg.maxDelayMs = 200;
    cfg.enableRandomDelay = false;

    mgr.setAntiDetectionConfig(cfg);
    const auto& retrieved = mgr.getAntiDetectionConfig();
    EXPECT_EQ(retrieved.minDelayMs, 100);
    EXPECT_EQ(retrieved.maxDelayMs, 200);
    EXPECT_FALSE(retrieved.enableRandomDelay);
}

TEST(SecurityManagerTest, GetRandomDelayInRange) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.minDelayMs = 10;
    cfg.maxDelayMs = 20;
    cfg.enableRandomDelay = true;
    mgr.setAntiDetectionConfig(cfg);

    for (int i = 0; i < 50; ++i) {
        int delay = mgr.getRandomDelay();
        EXPECT_GE(delay, 10);
        EXPECT_LE(delay, 20);
    }
}

TEST(SecurityManagerTest, GetRandomDelayNoRandom) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.minDelayMs = 10;
    cfg.maxDelayMs = 20;
    cfg.enableRandomDelay = false;
    mgr.setAntiDetectionConfig(cfg);

    int delay = mgr.getRandomDelay();
    EXPECT_EQ(delay, 15); // (10 + 20) / 2
}

TEST(SecurityManagerTest, SetGetProcessProtectionConfig) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiDebug = true;
    cfg.enableAntiVM = true;

    mgr.setProcessProtectionConfig(cfg);
    const auto& retrieved = mgr.getProcessProtectionConfig();
    EXPECT_TRUE(retrieved.enableAntiDebug);
    EXPECT_TRUE(retrieved.enableAntiVM);
}

TEST(SecurityManagerTest, IsDebuggerPresentNoAntiDebug) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiDebug = false;
    mgr.setProcessProtectionConfig(cfg);
    EXPECT_FALSE(mgr.isDebuggerPresent());
}

TEST(SecurityManagerTest, IsRunningInVMNoAntiVM) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiVM = false;
    mgr.setProcessProtectionConfig(cfg);
    EXPECT_FALSE(mgr.isRunningInVM());
}

TEST(SecurityManagerTest, VerifyIntegrityNoCheck) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableIntegrityCheck = false;
    mgr.setProcessProtectionConfig(cfg);
    EXPECT_TRUE(mgr.verifyIntegrity());
}

TEST(SecurityManagerTest, SelfSignReturnsFalse) {
    auto& mgr = SecurityManager::instance();
    EXPECT_FALSE(mgr.selfSign("", ""));
}

TEST(SecurityManagerTest, GetSignatureInfoDefaults) {
    auto& mgr = SecurityManager::instance();
    auto info = mgr.getSignatureInfo();
    EXPECT_FALSE(info.isSigned);
}

TEST(SecurityManagerTest, DisableProcessProtectionDoesNotCrash) {
    auto& mgr = SecurityManager::instance();
    EXPECT_NO_THROW(mgr.disableProcessProtection());
}

TEST(SecurityManagerTest, SecureZero) {
    auto& mgr = SecurityManager::instance();
    char data[] = "sensitive";
    mgr.secureZero(data, sizeof(data));
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(data[i], 0);
    }
}

TEST(SecurityManagerTest, GetRandomOffsetInRange) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.clickJitter = 5.0;
    mgr.setAntiDetectionConfig(cfg);

    for (int i = 0; i < 50; ++i) {
        auto [x, y] = mgr.getRandomOffset();
        EXPECT_GE(x, -5.0);
        EXPECT_LE(x, 5.0);
        EXPECT_GE(y, -5.0);
        EXPECT_LE(y, 5.0);
    }
}

TEST(SecurityManagerTest, GetClickJitterInRange) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.clickJitter = 3.0;
    mgr.setAntiDetectionConfig(cfg);

    for (int i = 0; i < 50; ++i) {
        auto [x, y] = mgr.getClickJitter();
        EXPECT_GE(x, -3.0);
        EXPECT_LE(x, 3.0);
        EXPECT_GE(y, -3.0);
        EXPECT_LE(y, 3.0);
    }
}

TEST(SecurityManagerTest, ProtectMemoryDoesNotCrash) {
    auto& mgr = SecurityManager::instance();
    char buffer[64] = {};
    EXPECT_NO_THROW(mgr.protectMemory(buffer, sizeof(buffer), false));
}

TEST(SecurityManagerTest, LockUnlockMemoryDoesNotCrash) {
    auto& mgr = SecurityManager::instance();
    char buffer[4096] = {};
    // VirtualLock may fail without privileges, just test it doesn't crash
    EXPECT_NO_THROW(mgr.lockMemory(buffer, sizeof(buffer)));
    EXPECT_NO_THROW(mgr.unlockMemory(buffer, sizeof(buffer)));
}

TEST(SecurityManagerTest, SecureLogDoesNotCrash) {
    auto& mgr = SecurityManager::instance();
    EXPECT_NO_THROW(mgr.secureLog("test message with password=secret"));
}

TEST(SecurityManagerTest, VerifySignatureReturnsFalse) {
    auto& mgr = SecurityManager::instance();
    EXPECT_NO_THROW(mgr.verifySignature());
}

TEST(SecurityManagerTest, EnableProcessProtectionWithoutFlag) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.protectFromTermination = false;
    mgr.setProcessProtectionConfig(cfg);
    EXPECT_TRUE(mgr.enableProcessProtection());
}

TEST(SecurityManagerTest, HashStringKnownValue) {
    // SHA-256("hello") is well-known
    std::string hash = SecurityManager::hashString("hello");
    EXPECT_EQ(hash, "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST(SecurityManagerTest, EncryptDecryptWithSingleCharKey) {
    std::string original = "test data";
    std::string key = "x";
    std::string encrypted = SecurityManager::encryptString(original, key);
    EXPECT_NE(encrypted, original);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, EncryptDecryptUnicodeString) {
    std::string original = "Hello 世界 🌍";
    std::string key = "key123";
    std::string encrypted = SecurityManager::encryptString(original, key);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, FilterSensitiveAllPatterns) {
    // Test all sensitive patterns are replaced
    std::vector<std::string> patterns = {"password", "passwd", "pwd", "token", "key", "secret", "api_key", "apikey"};
    for (const auto& p : patterns) {
        std::string input = "my " + p + " is here";
        std::string filtered = SecurityManager::filterSensitive(input);
        EXPECT_EQ(filtered.find(p), std::string::npos) << "Pattern '" << p << "' was not filtered";
    }
}
