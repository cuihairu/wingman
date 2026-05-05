#pragma once

#include <string>
#include <optional>
#include <vector>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

namespace wingman {

// 账号状态
enum class AccountStatus {
    Idle,          // 空闲
    Running,       // 运行中
    Success,       // 成功
    Failed,        // 失败
    Banned,        // 封禁
    NeedVerify,    // 需要验证
};

// 账号状态转字符串
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

// 字符串转账号状态
inline AccountStatus stringToAccountStatus(const std::string& str) {
    if (str == "idle") return AccountStatus::Idle;
    if (str == "running") return AccountStatus::Running;
    if (str == "success") return AccountStatus::Success;
    if (str == "failed") return AccountStatus::Failed;
    if (str == "banned") return AccountStatus::Banned;
    if (str == "need_verify") return AccountStatus::NeedVerify;
    return AccountStatus::Idle;
}

// 账号信息
struct Account {
    std::string id;              // 账号 ID
    std::string game;            // 游戏名称
    std::string username;        // 用户名
    std::string password;        // 密码（加密）
    std::string email;           // 邮箱
    std::string totpSecret;      // TOTP 密钥（加密）
    std::string group;           // 分组名称
    std::map<std::string, std::string> attributes;  // 自定义属性

    // 登录凭证
    std::string token;           // access token
    std::string cookie;          // cookie
    std::chrono::system_clock::time_point tokenExpiry;

    // 序列化
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["game"] = game;
        j["username"] = username;
        j["password"] = password;  // 实际使用时应加密
        j["email"] = email;
        j["totp_secret"] = totpSecret;  // 实际使用时应加密
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

// 批次信息
struct Batch {
    std::string id;              // 批次 ID
    std::string game;            // 游戏名称
    std::string name;            // 批次名称
    std::vector<std::string> accountIds;  // 账号 ID 列表

    // 进度跟踪
    std::map<std::string, AccountStatus> status;  // 账号状态
    std::map<std::string, std::string> currentStep;  // 当前步骤
    std::map<std::string, std::string> errorMessage;  // 错误信息

    // 统计
    int total = 0;
    int completed = 0;
    int success = 0;
    int failed = 0;

    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;

    // 序列化
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

        return batch;
    }
};

// 批次进度
struct BatchProgress {
    int total = 0;
    int completed = 0;
    int success = 0;
    int failed = 0;
    int running = 0;
    double percent = 0.0;
};

// 账号管理器
class AccountManager {
public:
    AccountManager(const std::string& dataDir = "data/accounts");
    ~AccountManager();

    // ========== 账号管理 ==========

    // 添加账号
    bool addAccount(const Account& account);

    // 删除账号
    bool removeAccount(const std::string& gameId, const std::string& accountId);

    // 获取账号
    std::optional<Account> getAccount(const std::string& gameId, const std::string& accountId);

    // 列出游戏的所有账号
    std::vector<Account> listAccounts(const std::string& game);

    // 列出分组的账号
    std::vector<Account> listByGroup(const std::string& game, const std::string& group);

    // 更新账号
    bool updateAccount(const std::string& gameId, const Account& account);

    // ========== 批次管理 ==========

    // 创建批次
    std::string createBatch(const std::string& game, const std::string& name,
                            const std::vector<std::string>& accountIds);

    // 获取批次
    std::optional<Batch> getBatch(const std::string& batchId);

    // 列出批次
    std::vector<Batch> listBatches(const std::string& game = "");

    // 删除批次
    bool removeBatch(const std::string& batchId);

    // 更新批次状态
    bool updateBatchStatus(const std::string& batchId,
                           const std::string& accountId,
                           AccountStatus status,
                           const std::string& step = "",
                           const std::string& error = "");

    // 获取批次进度
    BatchProgress getBatchProgress(const std::string& batchId);

    // ========== 分组管理 ==========

    // 创建分组
    bool createGroup(const std::string& game, const std::string& group);

    // 删除分组
    bool removeGroup(const std::string& game, const std::string& group);

    // 列出分组
    std::vector<std::string> listGroups(const std::string& game);

    // 将账号添加到分组
    bool addToGroup(const std::string& gameId, const std::string& accountId,
                    const std::string& group);

    // ========== 导入/导出 ==========

    // 导入账号（CSV/JSON）
    bool importAccounts(const std::string& game, const std::string& filePath,
                        const std::string& format = "json");

    // 导出账号
    bool exportAccounts(const std::string& game, const std::string& filePath,
                        const std::string& format = "json");

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 批次管理器（独立类，便于 Lua 绑定）
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
