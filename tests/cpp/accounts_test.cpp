#include <gtest/gtest.h>
#include "wingman/accounts.hpp"

using namespace wingman;

class AccountsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// AccountStatus Tests
// ============================================================================

TEST_F(AccountsTest, AccountStatusValues) {
    EXPECT_EQ(static_cast<int>(AccountStatus::Idle), 0);
    EXPECT_EQ(static_cast<int>(AccountStatus::Running), 1);
    EXPECT_EQ(static_cast<int>(AccountStatus::Success), 2);
    EXPECT_EQ(static_cast<int>(AccountStatus::Failed), 3);
    EXPECT_EQ(static_cast<int>(AccountStatus::Banned), 4);
    EXPECT_EQ(static_cast<int>(AccountStatus::NeedVerify), 5);
}

TEST_F(AccountsTest, AccountStatusToString) {
    EXPECT_EQ(accountStatusToString(AccountStatus::Idle), "idle");
    EXPECT_EQ(accountStatusToString(AccountStatus::Running), "running");
    EXPECT_EQ(accountStatusToString(AccountStatus::Success), "success");
    EXPECT_EQ(accountStatusToString(AccountStatus::Failed), "failed");
    EXPECT_EQ(accountStatusToString(AccountStatus::Banned), "banned");
    EXPECT_EQ(accountStatusToString(AccountStatus::NeedVerify), "need_verify");
}

TEST_F(AccountsTest, StringToAccountStatus) {
    EXPECT_EQ(stringToAccountStatus("idle"), AccountStatus::Idle);
    EXPECT_EQ(stringToAccountStatus("running"), AccountStatus::Running);
    EXPECT_EQ(stringToAccountStatus("success"), AccountStatus::Success);
    EXPECT_EQ(stringToAccountStatus("failed"), AccountStatus::Failed);
    EXPECT_EQ(stringToAccountStatus("banned"), AccountStatus::Banned);
    EXPECT_EQ(stringToAccountStatus("need_verify"), AccountStatus::NeedVerify);
    EXPECT_EQ(stringToAccountStatus("unknown"), AccountStatus::Idle); // Default
}

// ============================================================================
// Account Tests
// ============================================================================

TEST_F(AccountsTest, AccountDefaults) {
    Account account;
    EXPECT_TRUE(account.id.empty());
    EXPECT_TRUE(account.game.empty());
    EXPECT_TRUE(account.username.empty());
    EXPECT_TRUE(account.password.empty());
    EXPECT_TRUE(account.email.empty());
    EXPECT_TRUE(account.totpSecret.empty());
    EXPECT_TRUE(account.group.empty());
    EXPECT_TRUE(account.attributes.empty());
    EXPECT_TRUE(account.token.empty());
    EXPECT_TRUE(account.cookie.empty());
}

TEST_F(AccountsTest, AccountWithValues) {
    Account account;
    account.id = "acc001";
    account.game = "TestGame";
    account.username = "player1";
    account.password = "encrypted_pass";
    account.email = "player@example.com";
    account.group = "group1";

    EXPECT_EQ(account.id, "acc001");
    EXPECT_EQ(account.game, "TestGame");
    EXPECT_EQ(account.username, "player1");
    EXPECT_EQ(account.email, "player@example.com");
}

TEST_F(AccountsTest, AccountToJson) {
    Account account;
    account.id = "acc001";
    account.username = "player1";
    account.email = "player@example.com";

    auto json = account.toJson();
    EXPECT_EQ(json["id"], "acc001");
    EXPECT_EQ(json["username"], "player1");
    EXPECT_EQ(json["email"], "player@example.com");
}

TEST_F(AccountsTest, AccountFromJson) {
    nlohmann::json j;
    j["id"] = "acc001";
    j["username"] = "player1";
    j["email"] = "player@example.com";
    j["game"] = "TestGame";

    Account account = Account::fromJson(j);
    EXPECT_EQ(account.id, "acc001");
    EXPECT_EQ(account.username, "player1");
    EXPECT_EQ(account.email, "player@example.com");
    EXPECT_EQ(account.game, "TestGame");
}

TEST_F(AccountsTest, AccountAttributes) {
    Account account;
    account.attributes["level"] = "50";
    account.attributes["class"] = "warrior";
    account.attributes["server"] = "us-west";

    EXPECT_EQ(account.attributes.size(), 3);
    EXPECT_EQ(account.attributes["level"], "50");
    EXPECT_EQ(account.attributes["class"], "warrior");
}

// ============================================================================
// AccountManager Tests
// ============================================================================

TEST_F(AccountsTest, AccountManagerConstruction) {
    AccountManager manager;
    EXPECT_NO_THROW();
}

TEST_F(AccountsTest, AccountManagerListAccounts) {
    AccountManager manager;
    // Initially empty for a test game
    auto accounts = manager.listAccounts("TestGame");
    EXPECT_EQ(accounts.size(), 0);
}

TEST_F(AccountsTest, AccountManagerAddAccount) {
    AccountManager manager;

    Account account;
    account.id = "test001";
    account.game = "TestGame";
    account.username = "player1";

    EXPECT_TRUE(manager.addAccount(account));
}

TEST_F(AccountsTest, AccountManagerGetAccount) {
    AccountManager manager("data/test_accounts");

    Account account;
    account.id = "test001";
    account.game = "TestGame";
    account.username = "player1";
    manager.addAccount(account);

    auto retrieved = manager.getAccount("TestGame", "test001");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->username, "player1");

    auto missing = manager.getAccount("TestGame", "nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(AccountsTest, AccountManagerRemoveAccount) {
    AccountManager manager("data/test_accounts");

    Account account;
    account.id = "test001";
    account.game = "TestGame";
    manager.addAccount(account);

    EXPECT_TRUE(manager.removeAccount("TestGame", "test001"));
    EXPECT_FALSE(manager.removeAccount("TestGame", "nonexistent"));
    EXPECT_FALSE(manager.getAccount("TestGame", "test001").has_value());
}

TEST_F(AccountsTest, AccountManagerUpdateAccount) {
    AccountManager manager("data/test_accounts");

    Account account;
    account.id = "test001";
    account.game = "TestGame";
    account.username = "player1";
    manager.addAccount(account);

    account.username = "player2";
    EXPECT_TRUE(manager.updateAccount("TestGame", account));

    auto retrieved = manager.getAccount("TestGame", "test001");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->username, "player2");
}

TEST_F(AccountsTest, AccountManagerListByGroup) {
    AccountManager manager("data/test_accounts");

    Account acc1;
    acc1.id = "acc1";
    acc1.game = "TestGame";
    acc1.group = "group1";

    Account acc2;
    acc2.id = "acc2";
    acc2.game = "TestGame";
    acc2.group = "group1";

    Account acc3;
    acc3.id = "acc3";
    acc3.game = "TestGame";
    acc3.group = "group2";

    manager.addAccount(acc1);
    manager.addAccount(acc2);
    manager.addAccount(acc3);

    auto group1Accounts = manager.listByGroup("TestGame", "group1");
    EXPECT_EQ(group1Accounts.size(), 2);

    auto group2Accounts = manager.listByGroup("TestGame", "group2");
    EXPECT_EQ(group2Accounts.size(), 1);
}

TEST_F(AccountsTest, AccountManagerListByGame) {
    AccountManager manager("data/test_accounts");

    Account acc1;
    acc1.id = "acc1";
    acc1.game = "GameA";

    Account acc2;
    acc2.id = "acc2";
    acc2.game = "GameA";

    Account acc3;
    acc3.id = "acc3";
    acc3.game = "GameB";

    manager.addAccount(acc1);
    manager.addAccount(acc2);
    manager.addAccount(acc3);

    auto gameAAccounts = manager.listAccounts("GameA");
    EXPECT_EQ(gameAAccounts.size(), 2);
}

// ============================================================================
// BatchManager Tests
// ============================================================================

TEST_F(AccountsTest, BatchManagerConstruction) {
    AccountManager am("data/test_accounts");
    BatchManager manager(am);
    EXPECT_NO_THROW();
}

TEST_F(AccountsTest, BatchManagerCreateBatch) {
    AccountManager am("data/test_accounts");
    BatchManager manager(am);

    auto batchId = manager.create("TestGame", "batch1", {"acc1", "acc2"});
    EXPECT_FALSE(batchId.empty());
}

TEST_F(AccountsTest, BatchManagerListBatches) {
    AccountManager am("data/test_accounts");
    BatchManager manager(am);

    manager.create("TestGame", "batch1", {"acc1", "acc2"});
    manager.create("TestGame", "batch2", {"acc3"});

    auto batches = manager.list("TestGame");
    EXPECT_GE(batches.size(), 2);
}

TEST_F(AccountsTest, BatchManagerGetBatch) {
    AccountManager am("data/test_accounts");
    BatchManager manager(am);

    auto batchId = manager.create("TestGame", "batch1", {"acc1", "acc2"});

    auto batch = manager.get(batchId);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->accountIds.size(), 2);

    auto missing = manager.get("nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(AccountsTest, BatchManagerRemoveBatch) {
    AccountManager am("data/test_accounts");
    BatchManager manager(am);

    auto batchId = manager.create("TestGame", "batch1", {"acc1"});

    EXPECT_TRUE(manager.remove(batchId));
    EXPECT_FALSE(manager.remove(batchId));
}
