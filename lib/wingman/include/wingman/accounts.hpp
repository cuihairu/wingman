#pragma once

#include <string>
#include <optional>
#include <vector>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace wingman {

// Account status
enum class AccountStatus {
    Idle,          // Idle
    Running,       // Running
    Success,       // Success
    Failed,        // Failed
    Banned,        // Banned
    NeedVerify,    // Need verification
};

// Convert account status to string
inline std::string accountStatusToString(AccountStatus status) {
    switch (status) {
        case AccountStatus::Idle: return "idle";
        case AccountStatus::Running: return "running";
        case AccountStatus::Success: return "success";
        case AccountStatus::Failed: return "failed";
        case AccountStatus::Banned: return "banned";
        case AccountStatus::NeedVerify: return "need_verify";
        default: return "unknown";
    }
}

// Convert string to account status
inline AccountStatus stringToAccountStatus(const std::string& str) {
    if (str == "idle") return AccountStatus::Idle;
    if (str == "running") return AccountStatus::Running;
    if (str == "success") return AccountStatus::Success;
    if (str == "failed") return AccountStatus::Failed;
    if (str == "banned") return AccountStatus::Banned;
    if (str == "need_verify") return AccountStatus::NeedVerify;
    return AccountStatus::Idle;
}

// Account information
struct Account {
    std::string id;              // Account ID
    std::string game;            // Game name
    std::string username;        // Username
    std::string password;        // Password (encrypted)
    std::string email;           // Email
    std::string totpSecret;      // TOTP secret (encrypted)
    std::string group;           // Group name
    std::map<std::string, std::string> attributes;  // Custom attributes

    // Login credentials
    std::string token;           // access token
    std::string cookie;          // cookie
    std::chrono::system_clock::time_point tokenExpiry;

    // Serialization
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["game"] = game;
        j["username"] = username;
        j["password"] = password;  // Should be encrypted in production
        j["email"] = email;
        j["totp_secret"] = totpSecret;  // Should be encrypted in production
        j["group"] = group;
        j["attributes"] = attributes;
        j["token"] = token;
        j["cookie"] = cookie;
        return j;
    }

    static Account fromJson(const nlohmann::json& j) {
        Account acc;
        acc.id = j.value("id", "");
        acc.game = j.value("game", "");
        acc.username = j.value("username", "");
        acc.password = j.value("password", "");
        acc.email = j.value("email", "");
        acc.totpSecret = j.value("totp_secret", "");
        acc.group = j.value("group", "");
        if (j.contains("attributes")) {
            acc.attributes = j["attributes"].get<std::map<std::string, std::string>>();
        }
        acc.token = j.value("token", "");
        acc.cookie = j.value("cookie", "");
        return acc;
    }
};

// Batch information
struct Batch {
    std::string id;              // Batch ID
    std::string game;            // Game name
    std::string name;            // Batch name
    std::vector<std::string> accountIds;  // Account ID list

    // Progress tracking
    std::map<std::string, AccountStatus> status;  // Account status
    std::map<std::string, std::string> currentStep;  // Current step
    std::map<std::string, std::string> errorMessage;  // Error message

    // Statistics
    int total = 0;
    int completed = 0;
    int success = 0;
    int failed = 0;
    int running = 0;

    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;

    // Serialization
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["game"] = game;
        j["name"] = name;
        j["account_ids"] = accountIds;

        nlohmann::json statusJson;
        for (const auto& [accId, stat] : status) {
            statusJson[accId] = accountStatusToString(stat);
        }
        j["status"] = statusJson;

        j["current_step"] = currentStep;
        j["error_message"] = errorMessage;
        j["total"] = total;
        j["completed"] = completed;
        j["success"] = success;
        j["failed"] = failed;
        j["running"] = running;
        return j;
    }

    static Batch fromJson(const nlohmann::json& j) {
        Batch batch;
        batch.id = j.value("id", "");
        batch.game = j.value("game", "");
        batch.name = j.value("name", "");
        if (j.contains("account_ids")) {
            batch.accountIds = j["account_ids"].get<std::vector<std::string>>();
        }

        if (j.contains("status")) {
            for (auto& [accId, statStr] : j["status"].items()) {
                batch.status[accId] = stringToAccountStatus(statStr);
            }
        }

        if (j.contains("current_step")) {
            batch.currentStep = j["current_step"].get<std::map<std::string, std::string>>();
        }
        if (j.contains("error_message")) {
            batch.errorMessage = j["error_message"].get<std::map<std::string, std::string>>();
        }

        batch.total = j.value("total", 0);
        batch.completed = j.value("completed", 0);
        batch.success = j.value("success", 0);
        batch.failed = j.value("failed", 0);
        batch.running = j.value("running", 0);

        return batch;
    }
};

// Batch progress
struct BatchProgress {
    int total = 0;
    int completed = 0;
    int success = 0;
    int failed = 0;
    int running = 0;
    double percent = 0.0;
};

// Account manager
class AccountManager {
public:
    AccountManager(const std::string& dataDir = "data/accounts");
    ~AccountManager();

    // ========== Account management ==========

    // Add account
    bool addAccount(const Account& account);

    // Remove account
    bool removeAccount(const std::string& gameId, const std::string& accountId);

    // Get account
    std::optional<Account> getAccount(const std::string& gameId, const std::string& accountId);

    // List all accounts for a game
    std::vector<Account> listAccounts(const std::string& game);

    // List accounts by group
    std::vector<Account> listByGroup(const std::string& game, const std::string& group);

    // Update account
    bool updateAccount(const std::string& gameId, const Account& account);

    // ========== Batch management ==========

    // Create batch
    std::string createBatch(const std::string& game, const std::string& name,
                            const std::vector<std::string>& accountIds);

    // Get batch
    std::optional<Batch> getBatch(const std::string& batchId);

    // List batches
    std::vector<Batch> listBatches(const std::string& game = "");

    // Remove batch
    bool removeBatch(const std::string& batchId);

    // Update batch status
    bool updateBatchStatus(const std::string& batchId,
                           const std::string& accountId,
                           AccountStatus status,
                           const std::string& step = "",
                           const std::string& error = "");

    // Get batch progress
    BatchProgress getBatchProgress(const std::string& batchId);

    // ========== Group management ==========

    // Create group
    bool createGroup(const std::string& game, const std::string& group);

    // Remove group
    bool removeGroup(const std::string& game, const std::string& group);

    // List groups
    std::vector<std::string> listGroups(const std::string& game);

    // Add account to group
    bool addToGroup(const std::string& gameId, const std::string& accountId,
                    const std::string& group);

    // ========== Import/Export ==========

    // Import accounts (CSV/JSON)
    bool importAccounts(const std::string& game, const std::string& filePath,
                        const std::string& format = "json");

    // Export accounts
    bool exportAccounts(const std::string& game, const std::string& filePath,
                        const std::string& format = "json");

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Batch manager (standalone class, convenient for Lua binding)
class BatchManager {
public:
    BatchManager(AccountManager& accountManager);

    std::string create(const std::string& game, const std::string& name,
                       const std::vector<std::string>& accountIds);

    std::optional<Batch> get(const std::string& batchId);

    std::vector<Batch> list(const std::string& game = "");

    bool remove(const std::string& batchId);

    bool updateStatus(const std::string& batchId, const std::string& accountId,
                      AccountStatus status, const std::string& step = "",
                      const std::string& error = "");

    BatchProgress progress(const std::string& batchId);

private:
    AccountManager& accountManager_;
};

} // namespace wingman
