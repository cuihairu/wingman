#include <gtest/gtest.h>
#include "wingman/qrcode.hpp"

using namespace wingman;

class QRCodeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// QRLoginState Tests
// ============================================================================

TEST_F(QRCodeTest, QRLoginStateToString) {
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Pending), "pending");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Scanned), "scanned");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Confirmed), "success");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Expired), "expired");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Cancelled), "cancelled");
    EXPECT_EQ(qrLoginStateToString(QRLoginState::Error), "error");
}

// ============================================================================
// QRLoginConfig Tests
// ============================================================================

TEST_F(QRCodeTest, QRLoginConfigDefaults) {
    QRLoginConfig config;
    EXPECT_EQ(config.method, "GET");
    EXPECT_EQ(config.qrCodeField, "qr_code");
    EXPECT_EQ(config.statusField, "status");
    EXPECT_EQ(config.tokenField, "token");
    EXPECT_EQ(config.sessionIdField, "session_id");
    EXPECT_EQ(config.pollInterval, 2000);
    EXPECT_EQ(config.timeout, 120000);
    EXPECT_EQ(config.maxAttempts, 60);
}

// ============================================================================
// QRLoginResult Tests
// ============================================================================

TEST_F(QRCodeTest, QRLoginResultDefault) {
    QRLoginResult result;
    EXPECT_EQ(result.state, QRLoginState::Error); // Default initialization
    EXPECT_TRUE(result.message.empty());
    EXPECT_TRUE(result.token.empty());
    EXPECT_TRUE(result.sessionId.empty());
}

// ============================================================================
// QRLoginManager Tests
// ============================================================================

TEST_F(QRCodeTest, QRLoginManagerConstruction) {
    QRLoginManager manager;
    EXPECT_NO_THROW();
}

TEST_F(QRCodeTest, QRLoginManagerDestruction) {
    auto manager = new QRLoginManager();
    EXPECT_NO_THROW(delete manager);
}

TEST_F(QRCodeTest, SteamConfig) {
    auto config = QRLoginManager::steamConfig();
    EXPECT_FALSE(config.qrUrl.empty());
    EXPECT_FALSE(config.statusUrl.empty());
}

TEST_F(QRCodeTest, WechatConfig) {
    auto config = QRLoginManager::wechatConfig();
    EXPECT_FALSE(config.qrUrl.empty());
    EXPECT_FALSE(config.statusUrl.empty());
}

TEST_F(QRCodeTest, GenericConfig) {
    auto config = QRLoginManager::genericConfig(
        "https://example.com/qr",
        "https://example.com/status"
    );
    EXPECT_EQ(config.qrUrl, "https://example.com/qr");
    EXPECT_EQ(config.statusUrl, "https://example.com/status");
}

TEST_F(QRCodeTest, CancelLogin) {
    QRLoginManager manager;
    EXPECT_NO_THROW(manager.cancel());
}

// ============================================================================
// QRLoginResult State Tests
// ============================================================================

TEST_F(QRCodeTest, QRLoginResultStates) {
    QRLoginResult result;

    result.state = QRLoginState::Pending;
    EXPECT_EQ(qrLoginStateToString(result.state), "pending");

    result.state = QRLoginState::Scanned;
    EXPECT_EQ(qrLoginStateToString(result.state), "scanned");

    result.state = QRLoginState::Confirmed;
    EXPECT_EQ(qrLoginStateToString(result.state), "success");

    result.state = QRLoginState::Expired;
    EXPECT_EQ(qrLoginStateToString(result.state), "expired");

    result.state = QRLoginState::Cancelled;
    EXPECT_EQ(qrLoginStateToString(result.state), "cancelled");

    result.state = QRLoginState::Error;
    EXPECT_EQ(qrLoginStateToString(result.state), "error");
}

// ============================================================================
// QRLoginConfig Json Tests
// ============================================================================

TEST_F(QRCodeTest, QRLoginConfigJsonFields) {
    QRLoginConfig config;

    config.qrParams["key1"] = "value1";
    config.statusParams["key2"] = "value2";

    EXPECT_FALSE(config.qrParams.empty());
    EXPECT_FALSE(config.statusParams.empty());
}

TEST_F(QRCodeTest, QRLoginConfigHeaders) {
    QRLoginConfig config;

    config.headers["Authorization"] = "Bearer token123";
    config.headers["Content-Type"] = "application/json";

    EXPECT_EQ(config.headers.size(), 2);
    EXPECT_EQ(config.headers["Authorization"], "Bearer token123");
}
