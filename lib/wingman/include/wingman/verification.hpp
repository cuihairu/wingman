#pragma once

#include <string>
#include <optional>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>

namespace wingman {

// TOTP 类型
enum class TOTPType {
    Steam,      // Steam Guard
    Google,     // Google Authenticator
    Authy,      // Authy
    Microsoft,  // Microsoft Authenticator
    Blizzard,   // Blizzard Authenticator
};

// TOTP 配置
struct TOTPConfig {
    TOTPType type = TOTPType::Steam;
    std::string secret;      // Base32 密钥
    int digits = 5;          // Steam 默认 5 位，其他通常 6 位
    int period = 30;         // 刷新周期（秒）
    std::string account;     // 账号标识
};

// 邮箱配置
struct EmailConfig {
    std::string imapServer;
    int imapPort = 993;
    bool useSSL = true;
    std::string username;
    std::string password;    // 或应用专用密码
    std::string senderFilter;  // 发件人过滤
    int timeoutSeconds = 60;
};

// 验证码类型
enum class VerificationType {
    TOTP,
    Email,
    SMS,
};

// 验证码结果
struct VerificationCode {
    VerificationType type;
    std::string code;
    std::chrono::system_clock::time_point expiry;
    int remainingSeconds;  // TOTP 剩余有效时间
};

// 验证码管理器
class VerificationManager {
public:
    VerificationManager();
    ~VerificationManager();

    // ========== TOTP 相关 ==========

    // 生成 TOTP 验证码
    std::string generateTOTP(const TOTPConfig& config);
    std::string generateTOTP(const std::string& account);

    // 验证 TOTP 码
    bool verifyTOTP(const TOTPConfig& config, const std::string& code, int window = 1);
    bool verifyTOTP(const std::string& account, const std::string& code, int window = 1);

    // 获取剩余有效时间
    int getRemainingSeconds(const TOTPConfig& config);

    // Steam Guard 特殊处理
    std::string generateSteamGuard(const std::string& secret);
    std::string generateSteamGuardTime(const std::string& secret, uint64_t time);

    // ========== 邮件相关 ==========

    // 获取邮箱验证码（阻塞等待）
    std::optional<std::string> getEmailCode(const EmailConfig& config);

    // 异步获取邮箱验证码
    void getEmailCodeAsync(const EmailConfig& config,
                           std::function<void(std::optional<std::string>)> callback);

    // 停止邮件监听
    void stopEmailListener();

    // ========== 配置管理 ==========

    // 保存 TOTP 配置
    bool saveTOTP(const std::string& account, const TOTPConfig& config);

    // 加载 TOTP 配置
    std::optional<TOTPConfig> loadTOTP(const std::string& account);

    // 列出所有 TOTP 账号
    std::vector<std::string> listTOTPAccounts();

    // 删除 TOTP 配置
    bool removeTOTP(const std::string& account);

    // 保存邮箱配置
    bool saveEmail(const std::string& account, const EmailConfig& config);

    // 加载邮箱配置
    std::optional<EmailConfig> loadEmail(const std::string& account);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
