#include <gtest/gtest.h>
#include "wingman/qrcode.hpp"
#include <thread>
#include <chrono>

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
    QRLoginResult result{};
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

TEST(QRLoginManagerTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(QRLoginManager mgr);
}

TEST(QRLoginManagerTest, DetectQRCodeReturnsNull) {
    QRLoginManager mgr;
    auto result = mgr.detectQRCode(0, 0, 100, 100);
    EXPECT_FALSE(result.has_value());
}

TEST(QRLoginManagerTest, GetQRCodeReturnsNullForInvalidUrl) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    auto result = mgr.getQRCode(cfg);
    EXPECT_FALSE(result.has_value());
}

TEST(QRLoginManagerTest, GetQRCodeImageReturnsNullForInvalidUrl) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    auto result = mgr.getQRCodeImage(cfg, "nonexistent_output.png");
    EXPECT_FALSE(result.has_value());
}

TEST(QRLoginManagerTest, PollStatusReturnsErrorForInvalidUrl) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    auto result = mgr.pollStatus(cfg);
    EXPECT_EQ(result.state, QRLoginState::Error);
}

TEST(QRLoginManagerTest, LoginWithZeroMaxAttemptsReturnsExpired) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 0;
    auto result = mgr.login(cfg);
    EXPECT_EQ(result.state, QRLoginState::Expired);
}

TEST(QRLoginManagerTest, LoginCancelledImmediately) {
    // login() resets cancelled flag at start, so cancel() must happen concurrently.
    // Instead test that cancel() during login causes Cancelled state.
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 100;
    cfg.pollInterval = 1;

    std::thread canceller([&mgr]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        mgr.cancel();
    });

    auto result = mgr.login(cfg);
    canceller.join();
    EXPECT_EQ(result.state, QRLoginState::Cancelled);
}

TEST(QRLoginManagerTest, SteamConfigHasCorrectFields) {
    auto cfg = QRLoginManager::steamConfig();
    EXPECT_EQ(cfg.qrCodeField, "qr_challenge");
    EXPECT_EQ(cfg.statusField, "state");
    EXPECT_EQ(cfg.tokenField, "access_token");
    EXPECT_EQ(cfg.pollInterval, 3000);
    EXPECT_EQ(cfg.timeout, 180000);
}

TEST(QRLoginManagerTest, WechatConfigHasDefaults) {
    auto cfg = QRLoginManager::wechatConfig();
    EXPECT_EQ(cfg.pollInterval, 2000);
    EXPECT_EQ(cfg.timeout, 120000);
    EXPECT_EQ(cfg.maxAttempts, 60);
}

TEST(QRLoginManagerTest, GenericConfigPreservesUrls) {
    auto cfg = QRLoginManager::genericConfig("https://qr.example.com", "https://status.example.com");
    EXPECT_EQ(cfg.qrUrl, "https://qr.example.com");
    EXPECT_EQ(cfg.statusUrl, "https://status.example.com");
    EXPECT_EQ(cfg.method, "GET");
}

TEST(QRLoginResultTest, StateFieldAccess) {
    QRLoginResult result{};
    result.state = QRLoginState::Scanned;
    result.message = "Scanned by user";
    result.token = "abc123";
    result.sessionId = "sess_1";
    EXPECT_EQ(result.state, QRLoginState::Scanned);
    EXPECT_EQ(result.message, "Scanned by user");
    EXPECT_EQ(result.token, "abc123");
    EXPECT_EQ(result.sessionId, "sess_1");
}
