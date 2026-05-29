#pragma once

#include <string>
#include <optional>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <mutex>

namespace wingman {

// Login state
enum class QRLoginState {
    Pending,       // Waiting for scan
    Scanned,       // Scanned, waiting for confirmation
    Confirmed,     // Confirmed, login successful
    Expired,       // QR code expired
    Cancelled,     // User cancelled
    Error          // Error
};

// Convert login state to string
inline std::string qrLoginStateToString(QRLoginState state) {
    switch (state) {
        case QRLoginState::Pending: return "pending";
        case QRLoginState::Scanned: return "scanned";
        case QRLoginState::Confirmed: return "success";
        case QRLoginState::Expired: return "expired";
        case QRLoginState::Cancelled: return "cancelled";
        case QRLoginState::Error: return "error";
        default: return "unknown";
    }
}

// QR code login configuration
struct QRLoginConfig {
    // API endpoints
    std::string qrUrl;           // URL to get QR code
    std::string statusUrl;       // URL to query status
    std::string method = "GET";  // HTTP method

    // Parameter configuration
    nlohmann::json qrParams;     // Parameters to get QR code
    nlohmann::json statusParams; // Parameters to query status

    // Response parsing (JSON path)
    std::string qrCodeField = "qr_code";      // QR code content field
    std::string statusField = "status";       // Status field
    std::string tokenField = "token";         // Token field
    std::string sessionIdField = "session_id"; // Session ID field

    // Polling configuration
    int pollInterval = 2000;     // Polling interval (milliseconds)
    int timeout = 120000;        // Timeout (milliseconds)
    int maxAttempts = 60;        // Max attempts

    // Header configuration
    std::map<std::string, std::string> headers;

    // Session ID (obtained from response)
    std::string sessionId;
};

// Login result
struct QRLoginResult {
    QRLoginState state;
    std::string message;
    std::string token;           // Login credentials
    std::string sessionId;       // Session ID
    nlohmann::json data;         // Full response data
};

// QR code login manager
class QRLoginManager {
public:
    QRLoginManager();
    ~QRLoginManager();

    // ========== Get QR code ==========

    // Get QR code content
    std::optional<std::string> getQRCode(const QRLoginConfig& config);

    // Get QR code and save as image
    std::optional<std::string> getQRCodeImage(const QRLoginConfig& config,
                                               const std::string& outputPath);

    // Recognize QR code from screen region
    std::optional<std::string> detectQRCode(int x, int y, int width, int height);

    // ========== Login flow ==========

    // Execute full QR code login flow (blocking)
    QRLoginResult login(const QRLoginConfig& config);

    // Execute QR code login (async, with callback)
    void loginAsync(const QRLoginConfig& config,
                    std::function<void(const QRLoginResult&)> callback);

    // Poll login status
    QRLoginResult pollStatus(const QRLoginConfig& config);

    // Cancel login
    void cancel();

    // ========== Preset configurations ==========

    // Steam QR code login configuration
    static QRLoginConfig steamConfig();

    // WeChat QR code login configuration
    static QRLoginConfig wechatConfig();

    // Generic QR code login configuration
    static QRLoginConfig genericConfig(const std::string& qrUrl,
                                        const std::string& statusUrl);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
