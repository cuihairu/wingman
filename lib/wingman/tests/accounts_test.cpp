#include <gtest/gtest.h>
#include "wingman/accounts.hpp"

#include <filesystem>
#include <fstream>

using namespace wingman;

namespace {
std::string makeTempDir() {
    auto path = std::filesystem::temp_directory_path() /
        ("wingman_test_accounts_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    std::filesystem::create_directories(path);
    return path.string();
}
}

// ========== Account Status ==========

TEST(AccountStatusTest, ToStringAndBack) {
    EXPECT_EQ(accountStatusToString(AccountStatus::Idle), "idle");
    EXPECT_EQ(accountStatusToString(AccountStatus::Running), "running");
    EXPECT_EQ(accountStatusToString(AccountStatus::Success), "success");
    EXPECT_EQ(accountStatusToString(AccountStatus::Failed), "failed");
    EXPECT_EQ(accountStatusToString(AccountStatus::Banned), "banned");
    EXPECT_EQ(accountStatusToString(AccountStatus::NeedVerify), "need_verify");
}

TEST(AccountStatusTest, StringToAccountStatus) {
    EXPECT_EQ(stringToAccountStatus("idle"), AccountStatus::Idle);
    EXPECT_EQ(stringToAccountStatus("running"), AccountStatus::Running);
    EXPECT_EQ(stringToAccountStatus("success"), AccountStatus::Success);
    EXPECT_EQ(stringToAccountStatus("failed"), AccountStatus::Failed);
    EXPECT_EQ(stringToAccountStatus("banned"), AccountStatus::Banned);
    EXPECT_EQ(stringToAccountStatus("need_verify"), AccountStatus::NeedVerify);
    EXPECT_EQ(stringToAccountStatus("unknown"), AccountStatus::Idle);
}

// ========== Account Serialization ==========

TEST(AccountTest, ToJsonFromJsonRoundtrip) {
    Account original;
    original.id = "acc1";
    original.game = "game1";
    original.username = "user1";
    original.password = "pass1";
    original.email = "user@example.com";
    original.totpSecret = "SECRET";
    original.group = "group1";
    original.attributes["key1"] = "val1";
    original.token = "tok123";
    original.cookie = "cookie123";

    auto j = original.toJson();
    auto restored = Account::fromJson(j);

    EXPECT_EQ(restored.id, "acc1");
    EXPECT_EQ(restored.game, "game1");
    EXPECT_EQ(restored.username, "user1");
    EXPECT_EQ(restored.password, "pass1");
    EXPECT_EQ(restored.email, "user@example.com");
    EXPECT_EQ(restored.totpSecret, "SECRET");
    EXPECT_EQ(restored.group, "group1");
    EXPECT_EQ(restored.attributes["key1"], "val1");
    EXPECT_EQ(restored.token, "tok123");
    EXPECT_EQ(restored.cookie, "cookie123");
}

TEST(AccountTest, FromJsonMissingFields) {
    nlohmann::json j;
    auto acc = Account::fromJson(j);
    EXPECT_TRUE(acc.id.empty());
    EXPECT_TRUE(acc.game.empty());
}

// ========== Batch Serialization ==========

TEST(BatchTest, ToJsonFromJsonRoundtrip) {
    Batch original;
    original.id = "batch1";
    original.game = "game1";
    original.name = "Test Batch";
    original.accountIds = {"acc1", "acc2"};
    original.status["acc1"] = AccountStatus::Success;
    original.status["acc2"] = AccountStatus::Running;
    original.currentStep["acc1"] = "done";
    original.errorMessage["acc2"] = "in progress";
    original.total = 2;
    original.completed = 1;
    original.success = 1;
    original.failed = 0;

    auto j = original.toJson();
    auto restored = Batch::fromJson(j);

    EXPECT_EQ(restored.id, "batch1");
    EXPECT_EQ(restored.game, "game1");
    EXPECT_EQ(restored.name, "Test Batch");
    EXPECT_EQ(restored.accountIds.size(), 2u);
    EXPECT_EQ(restored.status["acc1"], AccountStatus::Success);
    EXPECT_EQ(restored.status["acc2"], AccountStatus::Running);
    EXPECT_EQ(restored.total, 2);
    EXPECT_EQ(restored.completed, 1);
    EXPECT_EQ(restored.success, 1);
}

TEST(BatchTest, FromJsonMissingFields) {
    nlohmann::json j;
    auto batch = Batch::fromJson(j);
    EXPECT_TRUE(batch.id.empty());
    EXPECT_EQ(batch.total, 0);
}

// ========== BatchProgress ==========

TEST(BatchProgressTest, DefaultValues) {
    BatchProgress p;
    EXPECT_EQ(p.total, 0);
    EXPECT_EQ(p.completed, 0);
    EXPECT_EQ(p.success, 0);
    EXPECT_EQ(p.failed, 0);
    EXPECT_EQ(p.running, 0);
    EXPECT_DOUBLE_EQ(p.percent, 0.0);
}

// ========== AccountManager CRUD ==========

class AccountManagerTest : public ::testing::Test {
protected:
    std::string tempDir;
    std::unique_ptr<AccountManager> mgr;

    void SetUp() override {
        tempDir = makeTempDir() + "/" + std::to_string(std::rand());
        std::filesystem::create_directories(tempDir);
        mgr = std::make_unique<AccountManager>(tempDir);
    }

    void TearDown() override {
        mgr.reset();
        std::filesystem::remove_all(tempDir);
    }
};

TEST_F(AccountManagerTest, AddAccountRequiresIdAndGame) {
    Account acc;
    EXPECT_FALSE(mgr->addAccount(acc));

    acc.id = "acc1";
    EXPECT_FALSE(mgr->addAccount(acc));

    acc.game = "game1";
    EXPECT_TRUE(mgr->addAccount(acc));
}

TEST_F(AccountManagerTest, GetAccount) {
    Account acc;
    acc.id = "a1";
    acc.game = "g1";
    acc.username = "user1";
    mgr->addAccount(acc);

    auto result = mgr->getAccount("g1", "a1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "user1");

    auto missing = mgr->getAccount("g1", "nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST_F(AccountManagerTest, RemoveAccount) {
    Account acc;
    acc.id = "a1";
    acc.game = "g1";
    mgr->addAccount(acc);

    EXPECT_TRUE(mgr->removeAccount("g1", "a1"));
    EXPECT_FALSE(mgr->getAccount("g1", "a1").has_value());
    EXPECT_FALSE(mgr->removeAccount("g1", "a1"));
}

TEST_F(AccountManagerTest, ListAccounts) {
    Account acc1;
    acc1.id = "a1"; acc1.game = "g1"; acc1.username = "user1";
    mgr->addAccount(acc1);

    Account acc2;
    acc2.id = "a2"; acc2.game = "g1"; acc2.username = "user2";
    mgr->addAccount(acc2);

    Account acc3;
    acc3.id = "a3"; acc3.game = "g2"; acc3.username = "user3";
    mgr->addAccount(acc3);

    auto g1 = mgr->listAccounts("g1");
    EXPECT_EQ(g1.size(), 2u);

    auto g2 = mgr->listAccounts("g2");
    EXPECT_EQ(g2.size(), 1u);

    auto all = mgr->listAccounts("");
    EXPECT_GE(all.size(), 3u);
}

TEST_F(AccountManagerTest, UpdateAccount) {
    Account acc;
    acc.id = "a1"; acc.game = "g1"; acc.username = "original";
    mgr->addAccount(acc);

    acc.username = "updated";
    EXPECT_TRUE(mgr->updateAccount("g1", acc));

    auto result = mgr->getAccount("g1", "a1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->username, "updated");
}

TEST_F(AccountManagerTest, UpdateAccountGameMismatch) {
    Account acc;
    acc.id = "a1"; acc.game = "g1";
    mgr->addAccount(acc);

    acc.game = "g2";
    EXPECT_FALSE(mgr->updateAccount("g1", acc));
}

TEST_F(AccountManagerTest, UpdateNonexistentAccount) {
    Account acc;
    acc.id = "nonexistent"; acc.game = "g1";
    EXPECT_FALSE(mgr->updateAccount("g1", acc));
}

// ========== Batch CRUD ==========

TEST_F(AccountManagerTest, CreateAndGetBatch) {
    std::string batchId = mgr->createBatch("g1", "Test Batch", {"acc1", "acc2"});
    EXPECT_FALSE(batchId.empty());

    auto batch = mgr->getBatch(batchId);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->name, "Test Batch");
    EXPECT_EQ(batch->total, 2);
    EXPECT_EQ(batch->status["acc1"], AccountStatus::Idle);
}

TEST_F(AccountManagerTest, GetNonexistentBatch) {
    auto batch = mgr->getBatch("nonexistent");
    EXPECT_FALSE(batch.has_value());
}

TEST_F(AccountManagerTest, ListBatches) {
    mgr->createBatch("g1", "B1", {"a1"});
    mgr->createBatch("g1", "B2", {"a2"});
    mgr->createBatch("g2", "B3", {"a3"});

    auto g1 = mgr->listBatches("g1");
    EXPECT_EQ(g1.size(), 2u);

    auto all = mgr->listBatches("");
    EXPECT_GE(all.size(), 3u);
}

TEST_F(AccountManagerTest, RemoveBatch) {
    std::string batchId = mgr->createBatch("g1", "B1", {"a1"});
    EXPECT_TRUE(mgr->removeBatch(batchId));
    EXPECT_FALSE(mgr->getBatch(batchId).has_value());
    EXPECT_FALSE(mgr->removeBatch(batchId));
}

TEST_F(AccountManagerTest, UpdateBatchStatus) {
    std::string batchId = mgr->createBatch("g1", "B1", {"a1", "a2"});

    mgr->updateBatchStatus(batchId, "a1", AccountStatus::Success, "step1", "");
    mgr->updateBatchStatus(batchId, "a2", AccountStatus::Failed, "step2", "error");

    auto batch = mgr->getBatch(batchId);
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch->status["a1"], AccountStatus::Success);
    EXPECT_EQ(batch->status["a2"], AccountStatus::Failed);
    EXPECT_EQ(batch->completed, 2);
    EXPECT_EQ(batch->success, 1);
    EXPECT_EQ(batch->failed, 1);
}

TEST_F(AccountManagerTest, GetBatchProgress) {
    std::string batchId = mgr->createBatch("g1", "B1", {"a1", "a2"});
    mgr->updateBatchStatus(batchId, "a1", AccountStatus::Success, "", "");

    auto progress = mgr->getBatchProgress(batchId);
    EXPECT_EQ(progress.total, 2);
    EXPECT_EQ(progress.completed, 1);
    EXPECT_EQ(progress.success, 1);
    EXPECT_DOUBLE_EQ(progress.percent, 50.0);
}

TEST_F(AccountManagerTest, GetBatchProgressNonexistent) {
    auto progress = mgr->getBatchProgress("nonexistent");
    EXPECT_EQ(progress.total, 0);
}

// ========== Groups ==========

TEST_F(AccountManagerTest, CreateGroup) {
    EXPECT_TRUE(mgr->createGroup("g1", "vip"));
    EXPECT_FALSE(mgr->createGroup("g1", "vip")); // duplicate
}

TEST_F(AccountManagerTest, ListGroups) {
    mgr->createGroup("g1", "vip");
    mgr->createGroup("g1", "normal");
    mgr->createGroup("g2", "other");

    auto groups = mgr->listGroups("g1");
    EXPECT_EQ(groups.size(), 2u);
}

TEST_F(AccountManagerTest, RemoveGroup) {
    Account acc;
    acc.id = "a1"; acc.game = "g1"; acc.group = "vip";
    mgr->addAccount(acc);
    mgr->createGroup("g1", "vip");
    mgr->addToGroup("g1", "a1", "vip");

    EXPECT_TRUE(mgr->removeGroup("g1", "vip"));
    auto result = mgr->getAccount("g1", "a1");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->group.empty());
}

TEST_F(AccountManagerTest, AddToGroup) {
    Account acc;
    acc.id = "a1"; acc.game = "g1";
    mgr->addAccount(acc);
    mgr->createGroup("g1", "grp1");

    EXPECT_TRUE(mgr->addToGroup("g1", "a1", "grp1"));

    auto result = mgr->getAccount("g1", "a1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->group, "grp1");
}

TEST_F(AccountManagerTest, AddToGroupNonexistentAccount) {
    mgr->createGroup("g1", "grp1");
    EXPECT_FALSE(mgr->addToGroup("g1", "nonexistent", "grp1"));
}

TEST_F(AccountManagerTest, ListByGroup) {
    Account acc1;
    acc1.id = "a1"; acc1.game = "g1";
    mgr->addAccount(acc1);

    Account acc2;
    acc2.id = "a2"; acc2.game = "g1";
    mgr->addAccount(acc2);

    mgr->createGroup("g1", "grp1");
    mgr->addToGroup("g1", "a1", "grp1");
    mgr->addToGroup("g1", "a2", "grp1");

    auto members = mgr->listByGroup("g1", "grp1");
    EXPECT_EQ(members.size(), 2u);
}

// ========== Import/Export ==========

TEST_F(AccountManagerTest, ExportAndImportJson) {
    Account acc;
    acc.id = "a1"; acc.game = "g1"; acc.username = "exported";
    mgr->addAccount(acc);

    std::string exportPath = tempDir + "/export.json";
    EXPECT_TRUE(mgr->exportAccounts("g1", exportPath, "json"));

    // New manager, import
    AccountManager mgr2(tempDir + "/imported");
    EXPECT_TRUE(mgr2.importAccounts("g1", exportPath, "json"));
}

TEST_F(AccountManagerTest, ImportNonexistentFile) {
    EXPECT_FALSE(mgr->importAccounts("g1", "/nonexistent/file.json", "json"));
}

TEST_F(AccountManagerTest, ExportNonexistentFormat) {
    EXPECT_FALSE(mgr->exportAccounts("g1", tempDir + "/export.csv", "csv"));
}

// ========== BatchManager ==========

TEST_F(AccountManagerTest, BatchManagerDelegates) {
    BatchManager bm(*mgr);
    std::string batchId = bm.create("g1", "via_bm", {"a1"});
    EXPECT_FALSE(batchId.empty());

    auto batch = bm.get(batchId);
    ASSERT_TRUE(batch.has_value());

    auto list = bm.list("g1");
    EXPECT_GE(list.size(), 1u);

    EXPECT_TRUE(bm.updateStatus(batchId, "a1", AccountStatus::Success, "done", ""));
    auto progress = bm.progress(batchId);
    EXPECT_EQ(progress.success, 1);

    EXPECT_TRUE(bm.remove(batchId));
}
