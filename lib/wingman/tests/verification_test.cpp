#include <gtest/gtest.h>
#include "wingman/verification.hpp"

using namespace wingman;

// ========== TOTPConfig ==========

TEST(TOTPConfigTest, DefaultValues) {
    TOTPConfig cfg;
    EXPECT_EQ(cfg.type, TOTPType::Steam);
    EXPECT_EQ(cfg.digits, 5);
    EXPECT_EQ(cfg.period, 30);
    EXPECT_TRUE(cfg.secret.empty());
    EXPECT_TRUE(cfg.account.empty());
}

// ========== EmailConfig ==========

TEST(EmailConfigTest, DefaultValues) {
    EmailConfig cfg;
    EXPECT_EQ(cfg.imapPort, 993);
    EXPECT_TRUE(cfg.useSSL);
    EXPECT_EQ(cfg.timeoutSeconds, 60);
}

// ========== VerificationManager TOTP ==========

TEST(VerificationManagerTest, GenerateAndVerifyTOTP) {
    VerificationManager mgr;

    TOTPConfig cfg;
    cfg.type = TOTPType::Google;
    cfg.secret = "JBSWY3DPEHPK3PXP"; // "Hello!" in Base32
    cfg.digits = 6;
    cfg.period = 30;

    std::string code = mgr.generateTOTP(cfg);
    EXPECT_EQ(code.size(), 6u);
    EXPECT_FALSE(code.empty());

    // Verify the same code should pass
    EXPECT_TRUE(mgr.verifyTOTP(cfg, code, 1));

    // Wrong code should fail
    EXPECT_FALSE(mgr.verifyTOTP(cfg, "000000", 0));
}

TEST(VerificationManagerTest, GetRemainingSeconds) {
    VerificationManager mgr;

    TOTPConfig cfg;
    cfg.period = 30;
    int remaining = mgr.getRemainingSeconds(cfg);
    EXPECT_GE(remaining, 0);
    EXPECT_LE(remaining, 30);
}

TEST(VerificationManagerTest, SteamGuardGenerates5DigitCode) {
    VerificationManager mgr;

    // Use a known test secret
    std::string code = mgr.generateSteamGuard("JBSWY3DPEHPK3PXP");
    EXPECT_EQ(code.size(), 5u);
}

TEST(VerificationManagerTest, SteamGuardWithFixedTime) {
    VerificationManager mgr;

    std::string code1 = mgr.generateSteamGuardTime("JBSWY3DPEHPK3PXP", 1700000000ULL);
    std::string code2 = mgr.generateSteamGuardTime("JBSWY3DPEHPK3PXP", 1700000000ULL);
    EXPECT_EQ(code1, code2);
    EXPECT_EQ(code1.size(), 5u);

    // Different time should likely produce different code
    std::string code3 = mgr.generateSteamGuardTime("JBSWY3DPEHPK3PXP", 1700000030ULL);
    // Not guaranteed to be different, but typically is
    EXPECT_EQ(code3.size(), 5u);
}

// ========== TOTP 配置管理 ==========

TEST(VerificationManagerTest, SaveAndLoadTOTP) {
    VerificationManager mgr;

    TOTPConfig cfg;
    cfg.type = TOTPType::Google;
    cfg.secret = "JBSWY3DPEHPK3PXP";
    cfg.digits = 6;
    cfg.account = "test_save_load_acct";

    EXPECT_TRUE(mgr.saveTOTP("test_acct_save_load", cfg));

    auto loaded = mgr.loadTOTP("test_acct_save_load");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->type, TOTPType::Google);
    EXPECT_EQ(loaded->secret, "JBSWY3DPEHPK3PXP");
    EXPECT_EQ(loaded->digits, 6);

    EXPECT_TRUE(mgr.removeTOTP("test_acct_save_load"));
    EXPECT_FALSE(mgr.loadTOTP("test_acct_save_load").has_value());
}

TEST(VerificationManagerTest, ListTOTPAccounts) {
    VerificationManager mgr;

    TOTPConfig cfg;
    cfg.secret = "JBSWY3DPEHPK3PXP";
    cfg.account = "list_test_1";
    mgr.saveTOTP("list_test_acct_1", cfg);

    cfg.account = "list_test_2";
    mgr.saveTOTP("list_test_acct_2", cfg);

    auto accounts = mgr.listTOTPAccounts();
    EXPECT_GE(accounts.size(), 2u);

    mgr.removeTOTP("list_test_acct_1");
    mgr.removeTOTP("list_test_acct_2");
}

TEST(VerificationManagerTest, LoadNonExistentTOTP) {
    VerificationManager mgr;
    auto loaded = mgr.loadTOTP("totally_nonexistent_account");
    EXPECT_FALSE(loaded.has_value());
}

TEST(VerificationManagerTest, RemoveNonExistentTOTP) {
    VerificationManager mgr;
    EXPECT_NO_THROW(mgr.removeTOTP("nonexistent_remove_test"));
}

// ========== Email 配置管理 ==========

TEST(VerificationManagerTest, SaveAndLoadEmail) {
    VerificationManager mgr;

    EmailConfig cfg;
    cfg.imapServer = "imap.example.com";
    cfg.imapPort = 993;
    cfg.username = "user@example.com";
    cfg.password = "app_password";
    cfg.senderFilter = "noreply@example.com";

    EXPECT_TRUE(mgr.saveEmail("test_email_save", cfg));

    auto loaded = mgr.loadEmail("test_email_save");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->imapServer, "imap.example.com");
    EXPECT_EQ(loaded->username, "user@example.com");
}

TEST(VerificationManagerTest, LoadNonExistentEmail) {
    VerificationManager mgr;
    auto loaded = mgr.loadEmail("nonexistent_email");
    EXPECT_FALSE(loaded.has_value());
}

// ========== TOTP by account name ==========

TEST(VerificationManagerTest, GenerateTOTPByAccount) {
    VerificationManager mgr;

    TOTPConfig cfg;
    cfg.type = TOTPType::Google;
    cfg.secret = "JBSWY3DPEHPK3PXP";
    cfg.digits = 6;
    cfg.period = 30;
    mgr.saveTOTP("gen_by_name_acct", cfg);

    std::string code = mgr.generateTOTP("gen_by_name_acct");
    EXPECT_EQ(code.size(), 6u);

    mgr.removeTOTP("gen_by_name_acct");
}

TEST(VerificationManagerTest, VerifyTOTPByAccount) {
    VerificationManager mgr;

    TOTPConfig cfg;
    cfg.type = TOTPType::Google;
    cfg.secret = "JBSWY3DPEHPK3PXP";
    cfg.digits = 6;
    cfg.period = 30;
    mgr.saveTOTP("verify_by_name_acct", cfg);

    std::string code = mgr.generateTOTP("verify_by_name_acct");
    EXPECT_TRUE(mgr.verifyTOTP("verify_by_name_acct", code, 1));

    mgr.removeTOTP("verify_by_name_acct");
}
