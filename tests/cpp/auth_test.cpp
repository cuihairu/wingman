#include <gtest/gtest.h>
#include "wingman/auth.hpp"
#include <filesystem>

using namespace wingman;

class AuthTest : public ::testing::Test {
protected:
    std::string testDbPath = "test_auth.db";
    AuthManager* authManager = nullptr;

    void SetUp() override {
        // Clean up any existing test database
        std::filesystem::remove(testDbPath);
        authManager = new AuthManager(testDbPath);
        authManager->init();
    }

    void TearDown() override {
        delete authManager;
        std::filesystem::remove(testDbPath);
    }
};

// ============================================================================
// AuthManager Tests
// ============================================================================

TEST_F(AuthTest, InitSuccess) {
    AuthManager newAuth("test_init.db");
    EXPECT_TRUE(newAuth.init());
    std::filesystem::remove("test_init.db");
}

TEST_F(AuthTest, LoginDefaultUser) {
    // Default user should be created during init
    auto user = authManager->login("admin", "admin123");
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->username, "admin");
    EXPECT_EQ(user->role, "admin");
}

TEST_F(AuthTest, LoginInvalidCredentials) {
    auto user = authManager->login("admin", "wrongpassword");
    EXPECT_FALSE(user.has_value());

    user = authManager->login("nonexistent", "password");
    EXPECT_FALSE(user.has_value());
}

TEST_F(AuthTest, CreateUserSuccess) {
    EXPECT_TRUE(authManager->createUser("testuser", "testpass123", "user"));

    auto user = authManager->login("testuser", "testpass123");
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->username, "testuser");
    EXPECT_EQ(user->role, "user");
}

TEST_F(AuthTest, CreateUserDuplicate) {
    EXPECT_TRUE(authManager->createUser("duplicate", "password123"));
    // Creating same user again should fail
    EXPECT_FALSE(authManager->createUser("duplicate", "password456"));
}

TEST_F(AuthTest, ChangePasswordSuccess) {
    EXPECT_TRUE(authManager->createUser("changepass", "oldpass"));

    // Verify old password works
    auto user = authManager->login("changepass", "oldpass");
    EXPECT_TRUE(user.has_value());

    // Change password
    EXPECT_TRUE(authManager->changePassword("changepass", "oldpass", "newpass"));

    // Verify new password works
    user = authManager->login("changepass", "newpass");
    EXPECT_TRUE(user.has_value());

    // Verify old password doesn't work
    user = authManager->login("changepass", "oldpass");
    EXPECT_FALSE(user.has_value());
}

TEST_F(AuthTest, ChangePasswordWrongOldPassword) {
    EXPECT_TRUE(authManager->createUser("wrongold", "correctpass"));

    EXPECT_FALSE(authManager->changePassword("wrongold", "wrongpass", "newpass"));

    // Original password should still work
    auto user = authManager->login("wrongold", "correctpass");
    EXPECT_TRUE(user.has_value());
}

TEST_F(AuthTest, ChangePasswordNonexistentUser) {
    EXPECT_FALSE(authManager->changePassword("nonexistent", "old", "new"));
}

TEST_F(AuthTest, GenerateToken) {
    auto user = authManager->login("admin", "admin123");
    ASSERT_TRUE(user.has_value());

    std::string token = authManager->generateToken(*user);
    EXPECT_FALSE(token.empty());
    EXPECT_GT(token.length(), 10);
}

TEST_F(AuthTest, VerifyTokenSuccess) {
    auto user = authManager->login("admin", "admin123");
    ASSERT_TRUE(user.has_value());

    std::string token = authManager->generateToken(*user);
    auto verifiedUser = authManager->verifyToken(token);
    ASSERT_TRUE(verifiedUser.has_value());
    EXPECT_EQ(verifiedUser->username, user->username);
    EXPECT_EQ(verifiedUser->role, user->role);
}

TEST_F(AuthTest, VerifyTokenInvalid) {
    auto verifiedUser = authManager->verifyToken("invalid_token_12345");
    EXPECT_FALSE(verifiedUser.has_value());
}

TEST_F(AuthTest, GenerateDifferentTokensForDifferentUsers) {
    EXPECT_TRUE(authManager->createUser("user1", "pass1"));
    EXPECT_TRUE(authManager->createUser("user2", "pass2"));

    auto u1 = authManager->login("user1", "pass1");
    auto u2 = authManager->login("user2", "pass2");

    std::string token1 = authManager->generateToken(*u1);
    std::string token2 = authManager->generateToken(*u2);

    EXPECT_NE(token1, token2);
}
