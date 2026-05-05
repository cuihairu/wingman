# 账号管理模块设计

## 概述

管理多游戏账号，支持分组、批次进度跟踪。

## 数据结构

```
accounts/
├── steam/
│   ├── account1.json
│   ├── account2.json
│   └── group1/
│       └── account3.json
├── game2/
│   └── ...
└── batches/
    ├── batch_001.json
    └── batch_002.json
```

## API 设计

### C++ API

```cpp
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
};

// 账号管理器
class AccountManager {
public:
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
    std::vector<Batch> listBatches(const std::string& game);

    // 更新批次状态
    bool updateBatchStatus(const std::string& batchId,
                           const std::string& accountId,
                           AccountStatus status,
                           const std::string& step = "",
                           const std::string& error = "");

    // 获取批次进度
    struct BatchProgress {
        int total;
        int completed;
        int success;
        int failed;
        int running;
        double percent;
    };
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
};

} // namespace wingman
```

### Lua API

```lua
-- 添加账号
accounts.add({
    game = "steam",
    username = "user1",
    password = "pass1",
    email = "user1@example.com",
    totp_secret = "SECRET",
    group = "batch1"
})

-- 获取账号
local acc = accounts.get("steam", "account_id")

-- 列出账号
local list = accounts.list("steam")
for _, acc in ipairs(list) do
    print(acc.username)
end

-- 按分组列出
local group = accounts.list_group("steam", "batch1")

-- 创建批次
local batchId = batches.create({
    game = "steam",
    name = "今天的第一批",
    accounts = {"acc1", "acc2", "acc3"}
})

-- 更新进度
batches.update_status(batchId, "acc1", "running", "登录中")

-- 获取进度
local progress = batches.progress(batchId)
print(string.format("进度: %d/%d (%.1f%%)", progress.completed, progress.total, progress.percent))

-- 导入账号
accounts.import("steam", "accounts.csv")
```

## 数据文件格式

### account.json

```json
{
    "id": "acc_001",
    "game": "steam",
    "username": "user1",
    "password": "encrypted_password",
    "email": "user1@example.com",
    "totp_secret": "encrypted_secret",
    "group": "batch1",
    "attributes": {
        "region": "cn",
        "level": 100
    },
    "token": "",
    "cookie": "",
    "token_expiry": null
}
```

### batch.json

```json
{
    "id": "batch_001",
    "game": "steam",
    "name": "今天的第一批",
    "account_ids": ["acc_001", "acc_002", "acc_003"],
    "status": {
        "acc_001": "success",
        "acc_002": "running",
        "acc_003": "idle"
    },
    "current_step": {
        "acc_002": "登录验证"
    },
    "error_message": {},
    "total": 3,
    "completed": 1,
    "success": 1,
    "failed": 0,
    "start_time": "2026-05-06T00:00:00Z",
    "end_time": null
}
```

### CSV 导入格式

```csv
username,password,email,totp_secret,group
user1,pass1,user1@example.com,SECRET1,batch1
user2,pass2,user2@example.com,SECRET2,batch1
```

## 实现文件

```
include/wingman/accounts.hpp
src/accounts.cpp
src/account_storage.cpp
bindings/lua_accounts.cpp
```
