#pragma once

#include <string>
#include <optional>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>

namespace wingman {

// TOTP type
enum class TOTPType {
    Steam,      // Steam Guard
    Google,     // Google Authenticator
    Authy,      // Authy
    Microsoft,  // Microsoft Authenticator
    Blizzard,   // Blizzard Authenticator
};

// TOTP configuration
struct TOTPConfig {
    TOTPType type = TOTPType::Steam;
    std::string secret;      // Base32 secret key
    int digits = 5;          // Steam defaults to 5 digits, others usually 6 digits
    int period = 30;         // Refresh period (seconds)
    std::string account;     // Account identifier
};

// Email configuration
struct EmailConfig {
    std::string imapServer;
    int imapPort = 993;
    bool useSSL = true;
    std::string username;
    std::string password;    // Or app-specific password
    std::string senderFilter;  // Sender filter
    int timeoutSeconds = 60;
};

// Verification code type
enum class VerificationType {
    TOTP,
    Email,
    SMS,
};

// Verification code result
struct VerificationCode {
    VerificationType type;
    std::string code;
    std::chrono::system_clock::time_point expiry;
    int remainingSeconds;  // TOTP remaining valid time
};

// Verification code manager
class VerificationManager {
public:
    VerificationManager();
    ~VerificationManager();

    // ========== TOTP related ==========

    // Generate TOTP verification code
    std::string generateTOTP(const TOTPConfig& config);
    std::string generateTOTP(const std::string& account);

    // Verify TOTP code
    bool verifyTOTP(const TOTPConfig& config, const std::string& code, int window = 1);
    bool verifyTOTP(const std::string& account, const std::string& code, int window = 1);

    // Get remaining valid time
    int getRemainingSeconds(const TOTPConfig& config);

    // Steam Guard special handling
    std::string generateSteamGuard(const std::string& secret);
    std::string generateSteamGuardTime(const std::string& secret, uint64_t time);

    // ========== Email related ==========

    // Get email verification code (blocking wait)
    std::optional<std::string> getEmailCode(const EmailConfig& config);

    // Get email verification code asynchronously
    void getEmailCodeAsync(const EmailConfig& config,
                           std::function<void(std::optional<std::string>)> callback);

    // Stop email listener
    void stopEmailListener();

    // ========== Profile management ==========

    // Save TOTP configuration
    bool saveTOTP(const std::string& account, const TOTPConfig& config);

    // Load TOTP configuration
    std::optional<TOTPConfig> loadTOTP(const std::string& account);

    // List all TOTP accounts
    std::vector<std::string> listTOTPAccounts();

    // Remove TOTP configuration
    bool removeTOTP(const std::string& account);

    // Save email configuration
    bool saveEmail(const std::string& account, const EmailConfig& config);

    // Load email configuration
    std::optional<EmailConfig> loadEmail(const std::string& account);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
