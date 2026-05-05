# 验证码管理模块设计

## 概述

支持游戏账号登录时的各种验证方式：
- 邮箱验证码
- TOTP 动态密码（Steam Guard、Google Authenticator 等）
- 短信验证码（需手机端配合）

## 模块结构

```
include/wingman/verification.hpp
src/verification.cpp
src/email_reader.cpp      # IMAP 邮件读取
src/totp_generator.cpp     # TOTP 算法实现
```

## API 设计

### C++ API

```cpp
namespace wingman {

// TOTP 类型
enum class TOTPType {
    Steam,      // Steam Guard
    Google,     // Google Authenticator
    Authy,      // Authy
    Microsoft,  // Microsoft Authenticator
};

// TOTP 配置
struct TOTPConfig {
    TOTPType type;
    std::string secret;      // Base32 密钥
    int digits = 6;          // 验证码位数
    int period = 30;         // 刷新周期（秒）
};

// 邮箱配置
struct EmailConfig {
    std::string imapServer;
    int imapPort = 993;
    bool useSSL = true;
    std::string username;
    std::string password;
    std::string senderFilter;  // 发件人过滤
};

// 验证码管理器
class VerificationManager {
public:
    // TOTP 相关
    std::string generateTOTP(const TOTPConfig& config);
    bool verifyTOTP(const TOTPConfig& config, const std::string& code);

    // 邮件相关
    std::string getEmailCode(const EmailConfig& config,
                             const std::string& sender,
                             int timeoutSeconds = 60);

    // 保存/加载配置
    void saveTOTP(const std::string& account, const TOTPConfig& config);
    std::optional<TOTPConfig> loadTOTP(const std::string& account);
};

} // namespace wingman
```

### Lua API

```lua
-- TOTP
local totp = verification.totp("Steam", "BASE32_SECRET")
local code = totp:generate()  -- 生成当前验证码

-- 邮件
local email = verification.email({
    imap = "imap.gmail.com",
    username = "user@gmail.com",
    password = "app_password"
})
local code = email:getCode("noreply@steam.com", 60)  -- 60秒超时

-- 示例：Steam 登录流程
local steam_totp = verification.totp("Steam", "XXXXXXXXX")
local code = steam_totp:generate()
game:input("totp_code", code)
```

## 技术方案

### TOTP 实现
- 基于 RFC 6238
- HMAC-SHA1/256
- Base32 编解码
- 支持不同步长和位数

### 邮件实现
- 使用 `libcurl` IMAP 协议
- 支持 SSL/TLS
- 实时轮询新邮件
- 正则匹配验证码

## 依赖库

```cmake
# CMakeLists.txt
find_package(OpenSSL REQUIRED)  # HMAC/SHA
find_package(CURL REQUIRED)      # IMAP
```

## 参考资料

- Steam Guard: https://github.com/dyc3/steam-guard
- TOTP RFC: https://tools.ietf.org/html/rfc6238
- Base32: RFC 4648
