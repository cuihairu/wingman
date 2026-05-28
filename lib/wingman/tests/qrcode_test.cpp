#include <gtest/gtest.h>
#include "wingman/qrcode.hpp"
#include <set>
#include <thread>
#include <atomic>

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
    // login() resets cancelled flag at start (line 180), and pollStatus to an
    // invalid URL returns Error before cancel can take effect.  We can only
    // test that cancel() itself does not crash and that login with invalid
    // URL returns a non-Pending state.
    QRLoginManager mgr;
    EXPECT_NO_THROW(mgr.cancel());

    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 1;
    cfg.pollInterval = 1;
    auto result = mgr.login(cfg);
    EXPECT_NE(result.state, QRLoginState::Pending);
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

// ========== QRLoginConfig 拷贝与移动语义 ==========

TEST(QRLoginConfigTest, CopySemantics) {
    QRLoginConfig original;
    original.qrUrl = "https://example.com/qr";
    original.statusUrl = "https://example.com/status";
    original.method = "POST";
    original.pollInterval = 500;
    original.timeout = 30000;
    original.maxAttempts = 10;
    original.qrCodeField = "code";
    original.headers["Authorization"] = "Bearer token123";
    original.qrParams = nlohmann::json{{"key", "value"}};

    QRLoginConfig copy = original;
    EXPECT_EQ(copy.qrUrl, original.qrUrl);
    EXPECT_EQ(copy.statusUrl, original.statusUrl);
    EXPECT_EQ(copy.method, original.method);
    EXPECT_EQ(copy.pollInterval, original.pollInterval);
    EXPECT_EQ(copy.timeout, original.timeout);
    EXPECT_EQ(copy.maxAttempts, original.maxAttempts);
    EXPECT_EQ(copy.qrCodeField, original.qrCodeField);
    EXPECT_EQ(copy.headers.size(), original.headers.size());
    EXPECT_EQ(copy.qrParams, original.qrParams);
}

TEST(QRLoginConfigTest, MoveSemantics) {
    QRLoginConfig original;
    original.qrUrl = "https://example.com/qr";
    original.statusUrl = "https://example.com/status";
    original.sessionId = "session_move_test";

    QRLoginConfig moved = std::move(original);
    EXPECT_EQ(moved.qrUrl, "https://example.com/qr");
    EXPECT_EQ(moved.statusUrl, "https://example.com/status");
    EXPECT_EQ(moved.sessionId, "session_move_test");
}

TEST(QRLoginConfigTest, AssignmentOperator) {
    QRLoginConfig a;
    a.qrUrl = "https://a.com";
    a.pollInterval = 999;

    QRLoginConfig b;
    b = a;
    EXPECT_EQ(b.qrUrl, "https://a.com");
    EXPECT_EQ(b.pollInterval, 999);
}

// ========== QRLoginConfig 字段修改 ==========

TEST(QRLoginConfigTest, FieldModifications) {
    QRLoginConfig cfg;
    cfg.qrUrl = "https://api.test.com/qr";
    cfg.statusUrl = "https://api.test.com/status";
    cfg.method = "POST";
    cfg.qrParams = nlohmann::json{{"app_id", "wx123"}};
    cfg.statusParams = nlohmann::json{{"token", "abc"}};
    cfg.qrCodeField = "qr_img";
    cfg.statusField = "login_status";
    cfg.tokenField = "access_key";
    cfg.sessionIdField = "sid";
    cfg.pollInterval = 500;
    cfg.timeout = 30000;
    cfg.maxAttempts = 15;
    cfg.headers["User-Agent"] = "TestAgent/1.0";
    cfg.headers["X-Custom"] = "custom-value";
    cfg.sessionId = "sess_abc";

    EXPECT_EQ(cfg.qrUrl, "https://api.test.com/qr");
    EXPECT_EQ(cfg.statusUrl, "https://api.test.com/status");
    EXPECT_EQ(cfg.method, "POST");
    EXPECT_EQ(cfg.qrParams["app_id"], "wx123");
    EXPECT_EQ(cfg.statusParams["token"], "abc");
    EXPECT_EQ(cfg.qrCodeField, "qr_img");
    EXPECT_EQ(cfg.statusField, "login_status");
    EXPECT_EQ(cfg.tokenField, "access_key");
    EXPECT_EQ(cfg.sessionIdField, "sid");
    EXPECT_EQ(cfg.pollInterval, 500);
    EXPECT_EQ(cfg.timeout, 30000);
    EXPECT_EQ(cfg.maxAttempts, 15);
    EXPECT_EQ(cfg.headers.size(), 2u);
    EXPECT_EQ(cfg.headers.at("User-Agent"), "TestAgent/1.0");
    EXPECT_EQ(cfg.sessionId, "sess_abc");
}

// ========== QRLoginResult 多种状态 ==========

TEST(QRLoginResultTest, ConfirmedState) {
    QRLoginResult result{};
    result.state = QRLoginState::Confirmed;
    result.token = "valid_token_xyz";
    result.sessionId = "confirmed_session";
    result.message = "Login successful";
    result.data = nlohmann::json{{"user", "test"}};
    EXPECT_EQ(result.state, QRLoginState::Confirmed);
    EXPECT_EQ(result.token, "valid_token_xyz");
    EXPECT_TRUE(result.data.contains("user"));
}

TEST(QRLoginResultTest, ExpiredState) {
    QRLoginResult result{};
    result.state = QRLoginState::Expired;
    result.message = "QR code expired";
    EXPECT_EQ(result.state, QRLoginState::Expired);
    EXPECT_TRUE(result.token.empty());
}

TEST(QRLoginResultTest, ErrorStateWithMessage) {
    QRLoginResult result{};
    result.state = QRLoginState::Error;
    result.message = "Network timeout";
    EXPECT_EQ(result.state, QRLoginState::Error);
    EXPECT_EQ(result.message, "Network timeout");
}

TEST(QRLoginResultTest, CancelledState) {
    QRLoginResult result{};
    result.state = QRLoginState::Cancelled;
    result.message = "Login cancelled by user";
    EXPECT_EQ(result.state, QRLoginState::Cancelled);
}

TEST(QRLoginResultTest, DataFieldHoldsJson) {
    QRLoginResult result{};
    result.data = nlohmann::json::parse(R"({"status":"ok","code":200,"nested":{"key":"val"}})");
    EXPECT_TRUE(result.data.contains("status"));
    EXPECT_EQ(result.data["code"], 200);
    EXPECT_EQ(result.data["nested"]["key"], "val");
}

// ========== qrLoginStateToString 全覆盖 ==========

TEST(QRLoginStateTest, AllStatesCovered) {
    // Verify every enum value has a non-empty string mapping
    EXPECT_FALSE(qrLoginStateToString(QRLoginState::Pending).empty());
    EXPECT_FALSE(qrLoginStateToString(QRLoginState::Scanned).empty());
    EXPECT_FALSE(qrLoginStateToString(QRLoginState::Confirmed).empty());
    EXPECT_FALSE(qrLoginStateToString(QRLoginState::Expired).empty());
    EXPECT_FALSE(qrLoginStateToString(QRLoginState::Cancelled).empty());
    EXPECT_FALSE(qrLoginStateToString(QRLoginState::Error).empty());

    // Ensure each state maps to a distinct string
    std::set<std::string> seen;
    for (auto state : {QRLoginState::Pending, QRLoginState::Scanned, QRLoginState::Confirmed,
                       QRLoginState::Expired, QRLoginState::Cancelled, QRLoginState::Error}) {
        auto str = qrLoginStateToString(state);
        EXPECT_TRUE(seen.insert(str).second) << "Duplicate string: " << str;
    }
}

// ========== 多实例不干扰 ==========

TEST(QRLoginManagerTest, MultipleInstancesDoNotInterfere) {
    QRLoginManager mgr1;
    QRLoginManager mgr2;

    // Cancel one manager should not affect the other
    EXPECT_NO_THROW(mgr1.cancel());

    // mgr2 should still work fine
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 0;
    auto result = mgr2.login(cfg);
    EXPECT_EQ(result.state, QRLoginState::Expired);
}

TEST(QRLoginManagerTest, SeparateInstancesIndependentCancel) {
    QRLoginManager mgr1;
    QRLoginManager mgr2;

    EXPECT_NO_THROW(mgr1.cancel());
    EXPECT_NO_THROW(mgr2.cancel());

    // Both cancelled, should still handle gracefully
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 1;
    cfg.pollInterval = 1;
    EXPECT_NO_THROW(mgr2.login(cfg));
}

// ========== loginAsync 行为 ==========

TEST(QRLoginManagerTest, LoginAsyncCallbackExecutes) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 0;

    std::atomic<bool> callbackCalled{false};
    QRLoginResult capturedResult{};
    EXPECT_NO_THROW(mgr.loginAsync(cfg, [&](const QRLoginResult& result) {
        capturedResult = result;
        callbackCalled = true;
    }));

    // Wait for the async thread to finish (detached, so we just wait briefly)
    for (int i = 0; i < 50 && !callbackCalled; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_TRUE(callbackCalled);
    EXPECT_NE(capturedResult.state, QRLoginState::Pending);
}

TEST(QRLoginManagerTest, LoginAsyncCancelDoesNotCrash) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "http://127.0.0.1:1/nonexistent";
    cfg.statusUrl = "http://127.0.0.1:1/nonexistent";
    cfg.maxAttempts = 1;
    cfg.pollInterval = 1;

    EXPECT_NO_THROW(mgr.loginAsync(cfg, [](const QRLoginResult&) {}));
    EXPECT_NO_THROW(mgr.cancel());
}

// ========== 状态解析边界情况 ==========

TEST(QRLoginManagerTest, PollStatusWithEmptyStatusUrl) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.statusUrl = "";
    auto result = mgr.pollStatus(cfg);
    EXPECT_EQ(result.state, QRLoginState::Error);
}

TEST(QRLoginManagerTest, GetQRCodeWithEmptyUrl) {
    QRLoginManager mgr;
    QRLoginConfig cfg;
    cfg.qrUrl = "";
    auto result = mgr.getQRCode(cfg);
    EXPECT_FALSE(result.has_value());
}

TEST(QRLoginManagerTest, DetectQRCodeWithZeroDimensions) {
    QRLoginManager mgr;
    auto result = mgr.detectQRCode(0, 0, 0, 0);
    EXPECT_FALSE(result.has_value());
}

// ========== 预设配置完整性 ==========

TEST(QRLoginManagerTest, SteamConfigAllFieldsNonDefault) {
    auto cfg = QRLoginManager::steamConfig();
    EXPECT_GT(cfg.pollInterval, 0);
    EXPECT_GT(cfg.timeout, 0);
    EXPECT_GT(cfg.maxAttempts, 0);
    EXPECT_FALSE(cfg.qrCodeField.empty());
    EXPECT_FALSE(cfg.statusField.empty());
    EXPECT_FALSE(cfg.tokenField.empty());
}

TEST(QRLoginManagerTest, WechatConfigAllFieldsValid) {
    auto cfg = QRLoginManager::wechatConfig();
    EXPECT_GT(cfg.pollInterval, 0);
    EXPECT_GT(cfg.timeout, 0);
    EXPECT_GT(cfg.maxAttempts, 0);
    EXPECT_EQ(cfg.method, "GET");
}

TEST(QRLoginManagerTest, GenericConfigUsesDefaultFieldValues) {
    auto cfg = QRLoginManager::genericConfig("https://qr.test.com", "https://status.test.com");
    EXPECT_EQ(cfg.qrCodeField, "qr_code");
    EXPECT_EQ(cfg.statusField, "status");
    EXPECT_EQ(cfg.tokenField, "token");
    EXPECT_EQ(cfg.sessionIdField, "session_id");
    EXPECT_EQ(cfg.pollInterval, 2000);
    EXPECT_EQ(cfg.timeout, 120000);
    EXPECT_EQ(cfg.maxAttempts, 60);
}

// ========== QRLoginConfig JSON 参数 ==========

TEST(QRLoginConfigTest, JsonParamsAssignment) {
    QRLoginConfig cfg;
    cfg.qrParams = nlohmann::json{{"size", 256}, {"format", "png"}};
    cfg.statusParams = nlohmann::json{{"session", "abc"}, {"retry", true}};

    EXPECT_EQ(cfg.qrParams["size"], 256);
    EXPECT_EQ(cfg.qrParams["format"], "png");
    EXPECT_EQ(cfg.statusParams["session"], "abc");
    EXPECT_EQ(cfg.statusParams["retry"], true);
}

TEST(QRLoginConfigTest, HeadersMapOperations) {
    QRLoginConfig cfg;
    cfg.headers["Content-Type"] = "application/json";
    cfg.headers["Authorization"] = "Bearer xyz";
    cfg.headers["X-Request-Id"] = "12345";

    EXPECT_EQ(cfg.headers.size(), 3u);
    EXPECT_EQ(cfg.headers.at("Content-Type"), "application/json");

    cfg.headers.erase("X-Request-Id");
    EXPECT_EQ(cfg.headers.size(), 2u);
    EXPECT_EQ(cfg.headers.count("X-Request-Id"), 0u);
}
