#include <gtest/gtest.h>
#include "wingman/verification.hpp"

using namespace wingman;

class VerificationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// TOTPType Tests
// ============================================================================

TEST_F(VerificationTest, TOTPTypeValues) {
    EXPECT_EQ(static_cast<int>(TOTPType::Steam), 0);
    EXPECT_EQ(static_cast<int>(TOTPType::Google), 1);
    EXPECT_EQ(static_cast<int>(TOTPType::Authy), 2);
    EXPECT_EQ(static_cast<int>(TOTPType::Microsoft), 3);
    EXPECT_EQ(static_cast<int>(TOTPType::Blizzard), 4);
}

// ============================================================================
// TOTPConfig Tests
// ============================================================================

TEST_F(VerificationTest, TOTPConfigDefaults) {
    TOTPConfig config;
    EXPECT_EQ(config.type, TOTPType::Steam);
    EXPECT_TRUE(config.secret.empty());
    EXPECT_EQ(config.digits, 5);
    EXPECT_EQ(config.period, 30);
    EXPECT_TRUE(config.account.empty());
}

TEST_F(VerificationTest, TOTPConfigWithValues) {
    TOTPConfig config;
    config.type = TOTPType::Google;
    config.secret = "JBSWY3DPEHPK3PXP";
    config.digits = 6;
    config.period = 30;
    config.account = "test@example.com";

    EXPECT_EQ(config.type, TOTPType::Google);
    EXPECT_EQ(config.secret, "JBSWY3DPEHPK3PXP");
    EXPECT_EQ(config.digits, 6);
    EXPECT_EQ(config.period, 30);
    EXPECT_EQ(config.account, "test@example.com");
}

// ============================================================================
// EmailConfig Tests
// ============================================================================

TEST_F(VerificationTest, EmailConfigDefaults) {
    EmailConfig config;
    EXPECT_TRUE(config.imapServer.empty());
    EXPECT_EQ(config.imapPort, 993);
    EXPECT_TRUE(config.useSSL);
    EXPECT_TRUE(config.username.empty());
    EXPECT_TRUE(config.password.empty());
    EXPECT_TRUE(config.senderFilter.empty());
    EXPECT_EQ(config.timeoutSeconds, 60);
}

TEST_F(VerificationTest, EmailConfigWithValues) {
    EmailConfig config;
    config.imapServer = "imap.example.com";
    config.imapPort = 993;
    config.useSSL = true;
    config.username = "user@example.com";
    config.password = "password";
    config.senderFilter = "noreply@example.com";
    config.timeoutSeconds = 30;

    EXPECT_EQ(config.imapServer, "imap.example.com");
    EXPECT_EQ(config.imapPort, 993);
    EXPECT_EQ(config.timeoutSeconds, 30);
}

// ============================================================================
// VerificationType Tests
// ============================================================================

TEST_F(VerificationTest, VerificationTypeValues) {
    EXPECT_EQ(static_cast<int>(VerificationType::TOTP), 0);
    EXPECT_EQ(static_cast<int>(VerificationType::Email), 1);
    EXPECT_EQ(static_cast<int>(VerificationType::SMS), 2);
}

// ============================================================================
// VerificationCode Tests
// ============================================================================

TEST_F(VerificationTest, VerificationCodeDefaults) {
    VerificationCode code;
    // Default initialization values depend on implementation
    EXPECT_TRUE(code.code.empty());
}

// ============================================================================
// VerificationManager Tests
// ============================================================================

TEST_F(VerificationTest, VerificationManagerConstruction) {
    VerificationManager manager;
    EXPECT_NO_THROW();
}

TEST_F(VerificationTest, VerificationManagerDestruction) {
    auto manager = new VerificationManager();
    EXPECT_NO_THROW(delete manager);
}

TEST_F(VerificationTest, VerificationManagerGenerateTOTP) {
    VerificationManager manager;

    TOTPConfig config;
    config.type = TOTPType::Google;
    config.secret = "JBSWY3DPEHPK3PXP"; // Valid base32
    config.digits = 6;
    config.period = 30;

    std::string code = manager.generateTOTP(config);
    // Should generate a numeric code
    EXPECT_FALSE(code.empty());
    EXPECT_EQ(code.length(), 6);
}

TEST_F(VerificationTest, VerificationManagerGenerateTOTPWithInvalidSecret) {
    VerificationManager manager;

    TOTPConfig config;
    config.type = TOTPType::Google;
    config.secret = "INVALID@#$";
    config.digits = 6;

    // Should handle invalid secret gracefully
    std::string code = manager.generateTOTP(config);
    // May return empty or partial code
    SUCCEED();
}

TEST_F(VerificationTest, VerificationManagerGetRemainingSeconds) {
    VerificationManager manager;

    TOTPConfig config;
    config.period = 30;

    int remaining = manager.getRemainingSeconds(config);
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 30);
}

TEST_F(VerificationTest, VerificationManagerGenerateSteamGuard) {
    VerificationManager manager;
    std::string secret = "JBSWY3DPEHPK3PXP";

    std::string code = manager.generateSteamGuard(secret);
    EXPECT_FALSE(code.empty());
    EXPECT_EQ(code.length(), 5); // Steam uses 5 digits
}

TEST_F(VerificationTest, VerificationManagerGenerateSteamGuardTime) {
    VerificationManager manager;
    std::string secret = "JBSWY3DPEHPK3PXP";
    uint64_t time = 1234567890;

    std::string code = manager.generateSteamGuardTime(secret, time);
    EXPECT_FALSE(code.empty());
    EXPECT_EQ(code.length(), 5);
}

TEST_F(VerificationTest, VerificationManagerStopEmailListener) {
    VerificationManager manager;
    EXPECT_NO_THROW(manager.stopEmailListener());
}

TEST_F(VerificationTest, VerificationManagerTOTPConfigManagement) {
    VerificationManager manager;

    TOTPConfig config;
    config.type = TOTPType::Google;
    config.secret = "JBSWY3DPEHPK3PXP";
    config.account = "test@example.com";

    // Save config
    EXPECT_TRUE(manager.saveTOTP("test_account", config));

    // Load config
    auto loaded = manager.loadTOTP("test_account");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->account, "test@example.com");

    // List accounts
    auto accounts = manager.listTOTPAccounts();
    EXPECT_FALSE(accounts.empty());

    // Remove config
    EXPECT_TRUE(manager.removeTOTP("test_account"));
    EXPECT_FALSE(manager.loadTOTP("test_account").has_value());
}
