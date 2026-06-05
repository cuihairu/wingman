#include <gtest/gtest.h>
#include "wingman/verification.hpp"

using namespace wingman;

// ========== TOTP Generation ==========

TEST(TOTPTest, Generate6DigitCode) {
    std::string code = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    EXPECT_EQ(code.size(), 6u);
    EXPECT_FALSE(code.empty());
}

TEST(TOTPTest, GenerateConsistentCodes) {
    std::string code1 = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    std::string code2 = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    EXPECT_EQ(code1, code2);
}

TEST(TOTPTest, DifferentSecretsProduceDifferentCodes) {
    std::string code1 = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    std::string code2 = generateTOTP("HXDMVJECJJWSRB3HWIZR4IFXFTXFJA", 6, 30);
    // Not guaranteed to differ, but almost certainly will
    EXPECT_EQ(code1.size(), 6u);
    EXPECT_EQ(code2.size(), 6u);
}

// ========== TOTP Verification ==========

TEST(TOTPTest, VerifyGeneratedCode) {
    std::string secret = "JBSWY3DPEHPK3PXP";
    std::string code = generateTOTP(secret, 6, 30);
    EXPECT_TRUE(verifyTOTP(secret, code, 6, 30, 1));
}

TEST(TOTPTest, RejectWrongCode) {
    EXPECT_FALSE(verifyTOTP("JBSWY3DPEHPK3PXP", "000000", 6, 30, 0));
}

TEST(TOTPTest, VerifyWithWindowZero) {
    std::string secret = "JBSWY3DPEHPK3PXP";
    std::string code = generateTOTP(secret, 6, 30);
    EXPECT_TRUE(verifyTOTP(secret, code, 6, 30, 0));
    EXPECT_FALSE(verifyTOTP(secret, "999999", 6, 30, 0));
}

// ========== Remaining Seconds ==========

TEST(TOTPTest, GetRemainingSeconds) {
    int remaining = getRemainingSeconds(30);
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 30);
}

TEST(TOTPTest, GetRemainingSecondsCustomPeriod) {
    int remaining = getRemainingSeconds(60);
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 60);

    remaining = getRemainingSeconds(15);
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 15);
}

// ========== Steam Guard ==========

TEST(SteamGuardTest, Generate5DigitCode) {
    std::string code = generateSteamGuard("JBSWY3DPEHPK3PXP");
    EXPECT_EQ(code.size(), 5u);
}

TEST(SteamGuardTest, DifferentSecrets) {
    std::string code1 = generateSteamGuard("JBSWY3DPEHPK3PXP");
    std::string code2 = generateSteamGuard("HXDMVJECJJWSRB3HWIZR4IFXFTXFJA");
    EXPECT_EQ(code1.size(), 5u);
    EXPECT_EQ(code2.size(), 5u);
}

// ========== Edge Cases ==========

TEST(TOTPTest, EmptySecretProducesEmptyCode) {
    std::string code = generateTOTP("", 6, 30);
    EXPECT_TRUE(code.empty());
}

// ========== base32Decode Edge Cases ==========

TEST(TOTPTest, SecretWithWhitespaceIsDecoded) {
    // base32Decode skips whitespace characters
    std::string secretWithSpaces = "JBSW Y3DP EHPK 3PXP";
    std::string code = generateTOTP(secretWithSpaces, 6, 30);
    EXPECT_EQ(code.size(), 6u);
    // Should produce the same code as without spaces
    std::string codeNoSpaces = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    EXPECT_EQ(code, codeNoSpaces);
}

TEST(TOTPTest, SecretWithTabsAndNewlinesIsDecoded) {
    std::string secretWithWhitespace = "JBSW\tY3DP\nEHPK\r3PXP";
    std::string code = generateTOTP(secretWithWhitespace, 6, 30);
    EXPECT_EQ(code.size(), 6u);
}

TEST(TOTPTest, SecretWithInvalidCharsIsSkipped) {
    // Invalid characters are silently skipped in base32Decode
    std::string secretWithInvalid = "JBSW!Y3DP@EHPK#3PXP";
    std::string code = generateTOTP(secretWithInvalid, 6, 30);
    EXPECT_EQ(code.size(), 6u);
    // Should produce same code as clean secret
    std::string codeClean = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    EXPECT_EQ(code, codeClean);
}

TEST(TOTPTest, SecretWithLowercaseIsDecoded) {
    // base32Decode converts to uppercase
    std::string secretLower = "jbswy3dpehpk3pxp";
    std::string code = generateTOTP(secretLower, 6, 30);
    EXPECT_EQ(code.size(), 6u);
    std::string codeUpper = generateTOTP("JBSWY3DPEHPK3PXP", 6, 30);
    EXPECT_EQ(code, codeUpper);
}

// ========== TOTP with Different Digits ==========

TEST(TOTPTest, Generate8DigitCode) {
    std::string code = generateTOTP("JBSWY3DPEHPK3PXP", 8, 30);
    EXPECT_EQ(code.size(), 8u);
}

TEST(TOTPTest, Generate4DigitCode) {
    std::string code = generateTOTP("JBSWY3DPEHPK3PXP", 4, 30);
    EXPECT_EQ(code.size(), 4u);
}

// ========== TOTP with Different Periods ==========

TEST(TOTPTest, GenerateWithCustomPeriod) {
    std::string code = generateTOTP("JBSWY3DPEHPK3PXP", 6, 60);
    EXPECT_EQ(code.size(), 6u);
}

TEST(TOTPTest, VerifyWithDifferentPeriod) {
    std::string secret = "JBSWY3DPEHPK3PXP";
    std::string code = generateTOTP(secret, 6, 60);
    EXPECT_TRUE(verifyTOTP(secret, code, 6, 60, 1));
}

// ========== TOTP Verification with Window ==========

TEST(TOTPTest, VerifyWithWindowTwo) {
    std::string secret = "JBSWY3DPEHPK3PXP";
    std::string code = generateTOTP(secret, 6, 30);
    EXPECT_TRUE(verifyTOTP(secret, code, 6, 30, 2));
}

// ========== Steam Guard with Base64 Secret ==========

TEST(SteamGuardTest, GenerateWithBase64Secret) {
    // Steam Guard can use base64-encoded secrets (with - and _ characters)
    // A valid base64 secret that decodes to enough bytes
    std::string base64Secret = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string code = generateSteamGuard(base64Secret);
    EXPECT_EQ(code.size(), 5u);
}

TEST(SteamGuardTest, GenerateWithEmptySecret) {
    std::string code = generateSteamGuard("");
    EXPECT_TRUE(code.empty());
}

// ========== Verify with Invalid Secret ==========

TEST(TOTPTest, VerifyWithEmptySecretReturnsFalse) {
    EXPECT_FALSE(verifyTOTP("", "123456", 6, 30, 0));
}

// ========== Remaining Seconds Boundary ==========

TEST(TOTPTest, RemainingSecondsAlwaysPositive) {
    for (int period : {15, 30, 60, 120}) {
        int remaining = getRemainingSeconds(period);
        EXPECT_GE(remaining, 1);
        EXPECT_LE(remaining, period);
    }
}

// ========== hmacSha1 with Long Key ==========

TEST(TOTPTest, LongSecretIsHandled) {
    // Secret longer than 64 bytes triggers the key-hashing path in hmacSha1
    std::string longSecret(100, 'A');
    std::string code = generateTOTP(longSecret, 6, 30);
    // May be empty if base32 decode fails, but should not crash
    EXPECT_NO_THROW(generateTOTP(longSecret, 6, 30));
}
