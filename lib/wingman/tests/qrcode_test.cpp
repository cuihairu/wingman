#include <gtest/gtest.h>
#include "wingman/qrcode.hpp"

using namespace wingman;

// ========== QRLoginConfig ==========

TEST(QRLoginConfigTest, DefaultValues) {
    QRLoginConfig cfg;
    EXPECT_TRUE(cfg.qrUrl.empty());
    EXPECT_TRUE(cfg.statusUrl.empty());
    EXPECT_EQ(cfg.method, "GET");
    EXPECT_EQ(cfg.qrCodeField, "qr_code");
    EXPECT_EQ(cfg.statusField, "status");
    EXPECT_EQ(cfg.tokenField, "token");
    EXPECT_EQ(cfg.sessionIdField, "session_id");
    EXPECT_EQ(cfg.pollInterval, 2000);
    EXPECT_EQ(cfg.timeout, 120000);
    EXPECT_EQ(cfg.maxAttempts, 60);
}

TEST(QRLoginConfigTest, CustomValues) {
    QRLoginConfig cfg;
    cfg.qrUrl = "https://api.example.com/qr";
    cfg.statusUrl = "https://api.example.com/status";
    cfg.method = "POST";
    cfg.pollInterval = 1000;
    cfg.timeout = 60000;

    EXPECT_EQ(cfg.qrUrl, "https://api.example.com/qr");
    EXPECT_EQ(cfg.method, "POST");
    EXPECT_EQ(cfg.pollInterval, 1000);
}

// ========== QRLoginState ==========

TEST(QRLoginStateTest, StateToString) {
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Pending), "pending");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Scanned), "scanned");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Confirmed), "success");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Expired), "expired");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Cancelled), "cancelled");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Error), "error");
}

// ========== QRLoginResult ==========

TEST(QRLoginResultTest, DefaultValues) {
    QRLoginResult result;
    EXPECT_EQ(result.state, QRLoginState::Pending);
    EXPECT_TRUE(result.message.empty());
    EXPECT_TRUE(result.token.empty());
    EXPECT_TRUE(result.sessionId.empty());
}

// ========== QRLoginManager 预设配置 ==========

TEST(QRLoginManagerTest, SteamConfig) {
    auto cfg = QRLoginManager::steamConfig();
    EXPECT_FALSE(cfg.qrUrl.empty());
    EXPECT_FALSE(cfg.statusUrl.empty());
}

TEST(QRLoginManagerTest, WechatConfig) {
    auto cfg = QRLoginManager::wechatConfig();
    EXPECT_FALSE(cfg.qrUrl.empty());
    EXPECT_FALSE(cfg.statusUrl.empty());
}

TEST(QRLoginManagerTest, GenericConfig) {
    auto cfg = QRLoginManager::genericConfig(
        "https://example.com/qr",
        "https://example.com/status"
    );
    EXPECT_EQ(cfg.qrUrl, "https://example.com/qr");
    EXPECT_EQ(cfg.statusUrl, "https://example.com/status");
}

TEST(QRLoginManagerTest, CancelDoesNotCrash) {
    QRLoginManager mgr;
    EXPECT_NO_THROW(mgr.cancel());
}
