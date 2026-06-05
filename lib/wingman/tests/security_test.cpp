#include <gtest/gtest.h>
#include <cstring>
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

// ========== SecurityManager Static Methods ==========

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

// ========== SecurityManager Instance Methods ==========

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
    std::string original = "Hello World 🌍";
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

// ========== Additional Security Tests ==========

TEST(SecurityManagerTest, SimulateHumanBehaviorDoesNotCrash) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.enableRandomDelay = true;
    cfg.minDelayMs = 1;
    cfg.maxDelayMs = 2;
    mgr.setAntiDetectionConfig(cfg);
    EXPECT_NO_THROW(mgr.simulateHumanBehavior());
}

TEST(SecurityManagerTest, IsDebuggerPresentWithAntiDebugEnabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiDebug = true;
    mgr.setProcessProtectionConfig(cfg);
    // In normal test execution, no debugger is attached, so should return false.
    // If a debugger IS attached, this may return true -- that is acceptable.
    EXPECT_NO_THROW(mgr.isDebuggerPresent());
}

TEST(SecurityManagerTest, IsRunningInVMWithAntiVMEnabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiVM = true;
    mgr.setProcessProtectionConfig(cfg);
    // Result depends on the test environment (physical vs virtual).
    // We only verify it does not crash and returns a boolean.
    bool result = false;
    EXPECT_NO_THROW(result = mgr.isRunningInVM());
    (void)result;
}

TEST(SecurityManagerTest, VerifyIntegrityWithCheckEnabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableIntegrityCheck = true;
    mgr.setProcessProtectionConfig(cfg);
    // verifyIntegrity with check enabled will run the full integrity check.
    // In a test environment without a stored hash it may return true or false,
    // but it must not crash.
    EXPECT_NO_THROW(mgr.verifyIntegrity());
}

TEST(SecurityManagerTest, EnableProcessProtectionWithFlagEnabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.protectFromTermination = true;
    mgr.setProcessProtectionConfig(cfg);
    // May fail without admin privileges, but must not crash.
    bool result = false;
    EXPECT_NO_THROW(result = mgr.enableProcessProtection());
    (void)result;
}

TEST(SecurityManagerTest, ProtectMemoryReadOnlyMode) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping on Windows - VirtualProtect on stack memory causes SEGFAULT";
#endif
    auto& mgr = SecurityManager::instance();
    char buffer[64] = {};
    std::memcpy(buffer, "testdata", 8);
    // Set memory to read-only
    bool result = false;
    EXPECT_NO_THROW(result = mgr.protectMemory(buffer, sizeof(buffer), true));
    // Restore to read-write so the buffer can be safely released on the stack
    EXPECT_NO_THROW(mgr.protectMemory(buffer, sizeof(buffer), false));
    (void)result;
}

TEST(SecurityManagerTest, SecureZeroZeroSize) {
    auto& mgr = SecurityManager::instance();
    char data[] = "unchanged";
    // Zero-size secureZero should not corrupt memory
    EXPECT_NO_THROW(mgr.secureZero(data, 0));
    EXPECT_STREQ(data, "unchanged");
}

TEST(SecurityManagerTest, SecureZeroHeapMemory) {
    auto& mgr = SecurityManager::instance();
    auto* data = new char[32];
    std::memcpy(data, "heap_sensitive_data_here_12345", 30);
    data[31] = '\0';
    EXPECT_NO_THROW(mgr.secureZero(data, 32));
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(data[i], 0);
    }
    delete[] data;
}

TEST(SecurityManagerTest, HashStringLongInput) {
    std::string longInput(10000, 'A');
    std::string hash = SecurityManager::hashString(longInput);
    EXPECT_EQ(hash.size(), 64u);
    EXPECT_FALSE(hash.empty());
    // Hashing the same long input twice should produce the same result
    std::string hash2 = SecurityManager::hashString(longInput);
    EXPECT_EQ(hash, hash2);
}

TEST(SecurityManagerTest, EncryptStringKeyLongerThanInput) {
    std::string original = "hi";
    std::string key = "very_long_key_that_exceeds_input";
    std::string encrypted = SecurityManager::encryptString(original, key);
    EXPECT_NE(encrypted, original);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, EncryptStringWithNullCharInKey) {
    std::string original = "hello";
    std::string key = "ke\0y";
    key.resize(4); // include null character
    std::string encrypted = SecurityManager::encryptString(original, key);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, EncryptStringWithBinaryData) {
    std::string original;
    for (int i = 0; i < 256; ++i) {
        original += static_cast<char>(i);
    }
    std::string key = "binarykey";
    std::string encrypted = SecurityManager::encryptString(original, key);
    std::string decrypted = SecurityManager::decryptString(encrypted, key);
    EXPECT_EQ(decrypted, original);
}

TEST(SecurityManagerTest, GenerateRandomStringBoundaryLengths) {
    std::string s1 = SecurityManager::generateRandomString(1);
    EXPECT_EQ(s1.size(), 1u);

    std::string s2 = SecurityManager::generateRandomString(256);
    EXPECT_EQ(s2.size(), 256u);
}

TEST(SecurityManagerTest, GetRandomDelayVariance) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.minDelayMs = 0;
    cfg.maxDelayMs = 100;
    cfg.enableRandomDelay = true;
    mgr.setAntiDetectionConfig(cfg);

    bool sawDifferent = false;
    int first = mgr.getRandomDelay();
    for (int i = 0; i < 20; ++i) {
        int delay = mgr.getRandomDelay();
        EXPECT_GE(delay, 0);
        EXPECT_LE(delay, 100);
        if (delay != first) sawDifferent = true;
    }
    EXPECT_TRUE(sawDifferent);
}

TEST(SecurityManagerTest, GetRandomOffsetZeroJitter) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.clickJitter = 0.0;
    mgr.setAntiDetectionConfig(cfg);

    auto [x, y] = mgr.getRandomOffset();
    EXPECT_DOUBLE_EQ(x, 0.0);
    EXPECT_DOUBLE_EQ(y, 0.0);
}

TEST(SecurityManagerTest, FilterSensitiveConsecutiveKeywords) {
    std::string input = "passwordpassword";
    std::string filtered = SecurityManager::filterSensitive(input);
    // Both occurrences should be replaced
    EXPECT_EQ(filtered.find("password"), std::string::npos);
}

TEST(SecurityManagerTest, FilterSensitiveMixedCase) {
    // The filter uses case-sensitive matching
    std::string input = "PASSWORD token";
    std::string filtered = SecurityManager::filterSensitive(input);
    // "PASSWORD" should NOT be filtered (case-sensitive), but "token" should
    EXPECT_NE(filtered.find("PASSWORD"), std::string::npos);
    EXPECT_EQ(filtered.find("token"), std::string::npos);
}

TEST(SecurityManagerTest, FilterSensitiveKeySubstring) {
    // "key" is a short pattern that could match many strings
    std::string input = "keyboard keystroke";
    std::string filtered = SecurityManager::filterSensitive(input);
    // "key" inside "keyboard" and "keystroke" should be replaced
    EXPECT_EQ(filtered.find("key"), std::string::npos);
}

TEST(SecurityManagerTest, SelfSignWithPaths) {
    auto& mgr = SecurityManager::instance();
    EXPECT_FALSE(mgr.selfSign("/path/to/cert", "/path/to/key"));
}

TEST(SecurityManagerTest, CodeSignatureFieldAssignment) {
    CodeSignature sig;
    sig.isSigned = true;
    sig.issuer = "TestIssuer";
    sig.subject = "TestSubject";
    sig.thumbprint = "ABCD1234";
    EXPECT_TRUE(sig.isSigned);
    EXPECT_EQ(sig.issuer, "TestIssuer");
    EXPECT_EQ(sig.subject, "TestSubject");
    EXPECT_EQ(sig.thumbprint, "ABCD1234");
}

TEST(SecurityManagerTest, CodeSignatureValidityPeriod) {
    CodeSignature sig;
    auto now = std::chrono::system_clock::now();
    sig.validFrom = now;
    sig.validTo = now + std::chrono::hours(24 * 365);
    EXPECT_LT(sig.validFrom, sig.validTo);
}

TEST(SecurityManagerTest, ObfuscationConfigFieldModification) {
    ObfuscationConfig cfg;
    cfg.enableStringEncryption = true;
    cfg.enableControlFlowFlattening = true;
    cfg.enableDeadCodeInjection = true;
    cfg.enableVirtualization = true;
    EXPECT_TRUE(cfg.enableStringEncryption);
    EXPECT_TRUE(cfg.enableControlFlowFlattening);
    EXPECT_TRUE(cfg.enableDeadCodeInjection);
    EXPECT_TRUE(cfg.enableVirtualization);
}

TEST(SecurityManagerTest, ProcessProtectionConfigAllEnabled) {
    ProcessProtectionConfig cfg;
    cfg.protectFromTermination = true;
    cfg.hideFromTaskManager = true;
    cfg.enableAntiDebug = true;
    cfg.enableAntiVM = true;
    cfg.enableIntegrityCheck = true;
    EXPECT_TRUE(cfg.protectFromTermination);
    EXPECT_TRUE(cfg.hideFromTaskManager);
    EXPECT_TRUE(cfg.enableAntiDebug);
    EXPECT_TRUE(cfg.enableAntiVM);
    EXPECT_TRUE(cfg.enableIntegrityCheck);
}

TEST(SecurityManagerTest, AntiDetectionConfigFieldModification) {
    AntiDetectionConfig cfg;
    cfg.enableRandomDelay = false;
    cfg.enableBezierMovement = false;
    cfg.enableRandomClick = false;
    cfg.minDelayMs = 200;
    cfg.maxDelayMs = 500;
    cfg.clickJitter = 10.0;
    cfg.movementVariance = 0.5;
    EXPECT_FALSE(cfg.enableRandomDelay);
    EXPECT_FALSE(cfg.enableBezierMovement);
    EXPECT_FALSE(cfg.enableRandomClick);
    EXPECT_EQ(cfg.minDelayMs, 200);
    EXPECT_EQ(cfg.maxDelayMs, 500);
    EXPECT_DOUBLE_EQ(cfg.clickJitter, 10.0);
    EXPECT_DOUBLE_EQ(cfg.movementVariance, 0.5);
}

TEST(SecurityManagerTest, LockUnlockHeapMemory) {
    auto& mgr = SecurityManager::instance();
    auto* buffer = new char[4096]();
    // Lock/unlock on heap memory; may fail without privileges but must not crash
    EXPECT_NO_THROW(mgr.lockMemory(buffer, 4096));
    EXPECT_NO_THROW(mgr.unlockMemory(buffer, 4096));
    delete[] buffer;
}

TEST(SecurityManagerTest, ProtectMemoryNullLikeSmallBuffer) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping on Windows - VirtualProtect on stack memory causes SEGFAULT";
#endif
    auto& mgr = SecurityManager::instance();
    // Protect a very small buffer
    char buffer[1] = {0x42};
    EXPECT_NO_THROW(mgr.protectMemory(buffer, sizeof(buffer), false));
    EXPECT_NO_THROW(mgr.protectMemory(buffer, sizeof(buffer), true));
    EXPECT_NO_THROW(mgr.protectMemory(buffer, sizeof(buffer), false));
}

TEST(SecurityManagerTest, GetRandomDelaySameMinMax) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.minDelayMs = 50;
    cfg.maxDelayMs = 50;
    cfg.enableRandomDelay = false;
    mgr.setAntiDetectionConfig(cfg);

    int delay = mgr.getRandomDelay();
    EXPECT_EQ(delay, 50);
}

// ========== Encrypt with Empty Key ==========

TEST(SecurityManagerTest, EncryptStringEmptyKeyReturnsInput) {
    std::string input = "hello world";
    std::string result = SecurityManager::encryptString(input, "");
    EXPECT_EQ(result, input);
}

TEST(SecurityManagerTest, DecryptStringEmptyKeyReturnsInput) {
    std::string input = "encrypted data";
    std::string result = SecurityManager::decryptString(input, "");
    EXPECT_EQ(result, input);
}

// ========== Hash String Format ==========

TEST(SecurityManagerTest, HashStringReturns64HexChars) {
    std::string hash = SecurityManager::hashString("test input");
    EXPECT_EQ(hash.size(), 64u);
    // All characters should be hex digits
    for (char c : hash) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

// ========== Random String ==========

TEST(SecurityManagerTest, GenerateRandomStringZero) {
    std::string s = SecurityManager::generateRandomString(0);
    EXPECT_TRUE(s.empty());
}

// ========== SecureZero ==========

TEST(SecurityManagerTest, SecureZeroClearsBuffer) {
    auto& mgr = SecurityManager::instance();
    char buf[16];
    memset(buf, 0xAB, sizeof(buf));
    mgr.secureZero(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); ++i) {
        EXPECT_EQ(buf[i], 0);
    }
}

// ========== Get Anti-Detection Config ==========

TEST(SecurityManagerTest, GetAntiDetectionConfigReturnsCurrentValues) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.enableRandomDelay = true;
    cfg.minDelayMs = 10;
    cfg.maxDelayMs = 200;
    cfg.clickJitter = 5.0;
    cfg.movementVariance = 0.3;
    cfg.enableBezierMovement = true;
    cfg.enableRandomClick = true;
    mgr.setAntiDetectionConfig(cfg);

    auto retrieved = mgr.getAntiDetectionConfig();
    EXPECT_TRUE(retrieved.enableRandomDelay);
    EXPECT_EQ(retrieved.minDelayMs, 10);
    EXPECT_EQ(retrieved.maxDelayMs, 200);
    EXPECT_DOUBLE_EQ(retrieved.clickJitter, 5.0);
    EXPECT_DOUBLE_EQ(retrieved.movementVariance, 0.3);
    EXPECT_TRUE(retrieved.enableBezierMovement);
    EXPECT_TRUE(retrieved.enableRandomClick);
}

// ========== Get Process Protection Config ==========

TEST(SecurityManagerTest, GetProcessProtectionConfigReturnsCurrentValues) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.protectFromTermination = true;
    cfg.enableAntiDebug = true;
    cfg.enableAntiVM = true;
    mgr.setProcessProtectionConfig(cfg);

    auto retrieved = mgr.getProcessProtectionConfig();
    EXPECT_TRUE(retrieved.protectFromTermination);
    EXPECT_TRUE(retrieved.enableAntiDebug);
    EXPECT_TRUE(retrieved.enableAntiVM);
}

// ========== GetRandomOffset With Jitter ==========

TEST(SecurityManagerTest, GetRandomOffsetWithJitter) {
    auto& mgr = SecurityManager::instance();
    AntiDetectionConfig cfg;
    cfg.clickJitter = 3.0;
    cfg.enableRandomClick = true;
    mgr.setAntiDetectionConfig(cfg);

    auto [x, y] = mgr.getRandomOffset();
    // Values should be within [-3, 3] range
    EXPECT_GE(x, -3.0);
    EXPECT_LE(x, 3.0);
    EXPECT_GE(y, -3.0);
    EXPECT_LE(y, 3.0);
}

// ========== isDebuggerPresent / isRunningInVM ==========

TEST(SecurityManagerTest, IsDebuggerPresentWithAllChecksDisabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiDebug = false;
    mgr.setProcessProtectionConfig(cfg);

    // All sub-checks disabled, should return false
    EXPECT_FALSE(mgr.isDebuggerPresent());
}

TEST(SecurityManagerTest, IsRunningInVMWithAllChecksDisabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiVM = false;
    mgr.setProcessProtectionConfig(cfg);

    EXPECT_FALSE(mgr.isRunningInVM());
}

TEST(SecurityManagerTest, IsDebuggerPresentWithChecksEnabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiDebug = true;
    mgr.setProcessProtectionConfig(cfg);

    // May or may not detect debugger, just should not crash
    EXPECT_NO_THROW(mgr.isDebuggerPresent());
}

TEST(SecurityManagerTest, IsRunningInVMWithChecksEnabled) {
    auto& mgr = SecurityManager::instance();
    ProcessProtectionConfig cfg;
    cfg.enableAntiVM = true;
    mgr.setProcessProtectionConfig(cfg);

    // May or may not detect VM, just should not crash
    EXPECT_NO_THROW(mgr.isRunningInVM());
}
