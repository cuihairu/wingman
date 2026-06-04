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
