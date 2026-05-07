#include "wingman/accounts.hpp"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace wingman {

// AccountManager 实现
class AccountManager::Impl {
public:
    std::string dataDir;
    std::string accountsFile;
    std::string batchesFile;

    std::map<std::string, Account> accounts;  // key: gameId:accountId
    std::map<std::string, Batch> batches;      // key: batchId
    std::map<std::string, std::vector<std::string>> groups;  // key: game:group

    Impl(const std::string& dataDir) : dataDir(dataDir) {
        accountsFile = dataDir + "/accounts.json";
        batchesFile = dataDir + "/batches.json";

        // 创建数据目录
        std::filesystem::create_directories(dataDir);

        // 加载数据
        load();
    }

    std::string makeAccountKey(const std::string& gameId, const std::string& accountId) const {
        return gameId + ":" + accountId;
    }

    std::string makeGroupKey(const std::string& game, const std::string& group) const {
        return game + ":" + group;
    }

    void load() {
        // 加载账号
        if (std::filesystem::exists(accountsFile)) {
            try {
                std::ifstream f(accountsFile);
                nlohmann::json j;
                f >> j;

                if (j.contains("accounts")) {
                    for (auto& [key, accJson] : j["accounts"].items()) {
                        Account acc = Account::fromJson(accJson);
                        accounts[makeAccountKey(acc.game, acc.id)] = acc;
                    }
                }
            } catch (const std::exception& e) {
                // 加载失败，使用空数据
            }
        }

        // 加载批次
        if (std::filesystem::exists(batchesFile)) {
            try {
                std::ifstream f(batchesFile);
                nlohmann::json j;
                f >> j;

                if (j.contains("batches")) {
                    for (auto& [key, batchJson] : j["batches"].items()) {
                        Batch batch = Batch::fromJson(batchJson);
                        batches[batch.id] = batch;
                    }
                }
            } catch (const std::exception& e) {
                // 加载失败，使用空数据
            }
        }
    }

    void save() {
        try {
            // 保存账号
            nlohmann::json j;
            j["accounts"] = nlohmann::json::object();

            for (const auto& [key, acc] : accounts) {
                j["accounts"][key] = acc.toJson();
            }

            std::ofstream f(accountsFile);
            f << j.dump(2);
        } catch (...) {
            // 保存失败
        }

        try {
            // 保存批次
            nlohmann::json j;
            j["batches"] = nlohmann::json::object();

            for (const auto& [key, batch] : batches) {
                j["batches"][key] = batch.toJson();
            }

            std::ofstream f(batchesFile);
            f << j.dump(2);
        } catch (...) {
            // 保存失败
        }
    }
};

AccountManager::AccountManager(const std::string& dataDir)
    : impl_(std::make_unique<Impl>(dataDir)) {}

AccountManager::~AccountManager() = default;

// ========== 账号管理 ==========

bool AccountManager::addAccount(const Account& account) {
    if (account.id.empty() || account.game.empty()) {
        return false;
    }

    std::string key = impl_->makeAccountKey(account.game, account.id);
    impl_->accounts[key] = account;
    impl_->save();
    return true;
}

bool AccountManager::removeAccount(const std::string& gameId, const std::string& accountId) {
    std::string key = impl_->makeAccountKey(gameId, accountId);
    auto it = impl_->accounts.find(key);
    if (it == impl_->accounts.end()) {
        return false;
    }

    // 从所有分组中移除
    if (!it->second.group.empty()) {
        std::string groupKey = impl_->makeGroupKey(gameId, it->second.group);
        auto& groupMembers = impl_->groups[groupKey];
        groupMembers.erase(
            std::remove(groupMembers.begin(), groupMembers.end(), accountId),
            groupMembers.end()
        );
    }

    impl_->accounts.erase(it);
    impl_->save();
    return true;
}

std::optional<Account> AccountManager::getAccount(const std::string& gameId, const std::string& accountId) {
    std::string key = impl_->makeAccountKey(gameId, accountId);
    auto it = impl_->accounts.find(key);
    if (it == impl_->accounts.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Account> AccountManager::listAccounts(const std::string& game) {
    std::vector<Account> result;
    for (const auto& [key, acc] : impl_->accounts) {
        if (game.empty() || acc.game == game) {
            result.push_back(acc);
        }
    }
    return result;
}

std::vector<Account> AccountManager::listByGroup(const std::string& game, const std::string& group) {
    std::vector<Account> result;
    std::string groupKey = impl_->makeGroupKey(game, group);

    auto it = impl_->groups.find(groupKey);
    if (it != impl_->groups.end()) {
        for (const auto& accountId : it->second) {
            std::string key = impl_->makeAccountKey(game, accountId);
            auto accIt = impl_->accounts.find(key);
            if (accIt != impl_->accounts.end()) {
                result.push_back(accIt->second);
            }
        }
    }
    return result;
}

bool AccountManager::updateAccount(const std::string& gameId, const Account& account) {
    if (account.id.empty() || account.game != gameId) {
        return false;
    }

    std::string key = impl_->makeAccountKey(gameId, account.id);
    auto it = impl_->accounts.find(key);
    if (it == impl_->accounts.end()) {
        return false;
    }

    it->second = account;
    impl_->save();
    return true;
}

// ========== 批次管理 ==========

std::string AccountManager::createBatch(const std::string& game, const std::string& name,
                                       const std::vector<std::string>& accountIds) {
    // 生成批次 ID
    std::string batchId = game + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    Batch batch;
    batch.id = batchId;
    batch.game = game;
    batch.name = name;
    batch.accountIds = accountIds;
    batch.total = static_cast<int>(accountIds.size());
    batch.startTime = std::chrono::system_clock::now();

    // 初始化状态
    for (const auto& accId : accountIds) {
        batch.status[accId] = AccountStatus::Idle;
    }

    impl_->batches[batchId] = batch;
    impl_->save();
    return batchId;
}

std::optional<Batch> AccountManager::getBatch(const std::string& batchId) {
    auto it = impl_->batches.find(batchId);
    if (it == impl_->batches.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<Batch> AccountManager::listBatches(const std::string& game) {
    std::vector<Batch> result;
    for (const auto& [key, batch] : impl_->batches) {
        if (game.empty() || batch.game == game) {
            result.push_back(batch);
        }
    }
    return result;
}

bool AccountManager::removeBatch(const std::string& batchId) {
    auto it = impl_->batches.find(batchId);
    if (it == impl_->batches.end()) {
        return false;
    }
    impl_->batches.erase(it);
    impl_->save();
    return true;
}

bool AccountManager::updateBatchStatus(const std::string& batchId,
                                      const std::string& accountId,
                                      AccountStatus status,
                                      const std::string& step,
                                      const std::string& error) {
    auto it = impl_->batches.find(batchId);
    if (it == impl_->batches.end()) {
        return false;
    }

    Batch& batch = it->second;
    batch.status[accountId] = status;

    if (!step.empty()) {
        batch.currentStep[accountId] = step;
    }
    if (!error.empty()) {
        batch.errorMessage[accountId] = error;
    }

    // 更新统计
    batch.completed = 0;
    batch.success = 0;
    batch.failed = 0;
    batch.running = 0;

    for (const auto& [accId, stat] : batch.status) {
        switch (stat) {
            case AccountStatus::Success:
                batch.success++;
                batch.completed++;
                break;
            case AccountStatus::Failed:
            case AccountStatus::Banned:
                batch.failed++;
                batch.completed++;
                break;
            case AccountStatus::Running:
                batch.running++;
                break;
            default:
                break;
        }
    }

    if (batch.completed >= batch.total) {
        batch.endTime = std::chrono::system_clock::now();
    }

    impl_->save();
    return true;
}

BatchProgress AccountManager::getBatchProgress(const std::string& batchId) {
    auto it = impl_->batches.find(batchId);
    if (it == impl_->batches.end()) {
        return {};
    }

    const Batch& batch = it->second;
    BatchProgress progress;
    progress.total = batch.total;
    progress.completed = batch.completed;
    progress.success = batch.success;
    progress.failed = batch.failed;
    progress.running = batch.running;

    if (progress.total > 0) {
        progress.percent = static_cast<double>(progress.completed) / progress.total * 100.0;
    }

    return progress;
}

// ========== 分组管理 ==========

bool AccountManager::createGroup(const std::string& game, const std::string& group) {
    std::string key = impl_->makeGroupKey(game, group);
    if (impl_->groups.find(key) != impl_->groups.end()) {
        return false;  // 已存在
    }
    impl_->groups[key] = {};
    impl_->save();
    return true;
}

bool AccountManager::removeGroup(const std::string& game, const std::string& group) {
    std::string key = impl_->makeGroupKey(game, group);
    auto it = impl_->groups.find(key);
    if (it == impl_->groups.end()) {
        return false;
    }

    // 清除账号的分组标记
    for (const auto& accountId : it->second) {
        std::string accKey = impl_->makeAccountKey(game, accountId);
        auto accIt = impl_->accounts.find(accKey);
        if (accIt != impl_->accounts.end()) {
            accIt->second.group.clear();
        }
    }

    impl_->groups.erase(it);
    impl_->save();
    return true;
}

std::vector<std::string> AccountManager::listGroups(const std::string& game) {
    std::vector<std::string> result;
    std::string prefix = game + ":";

    for (const auto& [key, members] : impl_->groups) {
        if (key.find(prefix) == 0) {
            std::string groupName = key.substr(prefix.length());
            result.push_back(groupName);
        }
    }
    return result;
}

bool AccountManager::addToGroup(const std::string& gameId, const std::string& accountId,
                               const std::string& group) {
    std::string accKey = impl_->makeAccountKey(gameId, accountId);
    auto accIt = impl_->accounts.find(accKey);
    if (accIt == impl_->accounts.end()) {
        return false;
    }

    // 从旧分组移除
    if (!accIt->second.group.empty()) {
        std::string oldGroupKey = impl_->makeGroupKey(gameId, accIt->second.group);
        auto& oldMembers = impl_->groups[oldGroupKey];
        oldMembers.erase(
            std::remove(oldMembers.begin(), oldMembers.end(), accountId),
            oldMembers.end()
        );
    }

    // 添加到新分组
    std::string newGroupKey = impl_->makeGroupKey(gameId, group);
    impl_->groups[newGroupKey].push_back(accountId);
    accIt->second.group = group;

    impl_->save();
    return true;
}

// ========== 导入/导出 ==========

bool AccountManager::importAccounts(const std::string& game, const std::string& filePath,
                                   const std::string& format) {
    try {
        std::ifstream f(filePath);
        nlohmann::json j;
        f >> j;

        std::vector<Account> imported;

        if (format == "json") {
            if (j.contains("accounts")) {
                for (const auto& accJson : j["accounts"]) {
                    Account acc = Account::fromJson(accJson);
                    if (acc.game == game || acc.game.empty()) {
                        acc.game = game;
                        imported.push_back(acc);
                    }
                }
            } else if (j.is_array()) {
                for (const auto& accJson : j) {
                    Account acc = Account::fromJson(accJson);
                    acc.game = game;
                    imported.push_back(acc);
                }
            }
        }

        for (const auto& acc : imported) {
            addAccount(acc);
        }

        return !imported.empty();
    } catch (...) {
        return false;
    }
}

bool AccountManager::exportAccounts(const std::string& game, const std::string& filePath,
                                   const std::string& format) {
    try {
        if (format == "json") {
            nlohmann::json j;
            j["accounts"] = nlohmann::json::array();

            auto accounts = listAccounts(game);
            for (const auto& acc : accounts) {
                j["accounts"].push_back(acc.toJson());
            }

            std::ofstream f(filePath);
            f << j.dump(2);
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

// ========== BatchManager 实现 ==========

BatchManager::BatchManager(AccountManager& accountManager)
    : accountManager_(accountManager) {}

std::string BatchManager::create(const std::string& game, const std::string& name,
                                const std::vector<std::string>& accountIds) {
    return accountManager_.createBatch(game, name, accountIds);
}

std::optional<Batch> BatchManager::get(const std::string& batchId) {
    return accountManager_.getBatch(batchId);
}

std::vector<Batch> BatchManager::list(const std::string& game) {
    return accountManager_.listBatches(game);
}

bool BatchManager::remove(const std::string& batchId) {
    return accountManager_.removeBatch(batchId);
}

bool BatchManager::updateStatus(const std::string& batchId, const std::string& accountId,
                               AccountStatus status, const std::string& step,
                               const std::string& error) {
    return accountManager_.updateBatchStatus(batchId, accountId, status, step, error);
}

BatchProgress BatchManager::progress(const std::string& batchId) {
    return accountManager_.getBatchProgress(batchId);
}

} // namespace wingman
