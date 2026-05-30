#include "wingman/qrcode.hpp"
#include "wingman/http.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <fstream>

namespace wingman {

// ========== JSON Path Parsing ==========

static std::optional<nlohmann::json> getJsonByPath(const nlohmann::json& j, const std::string& path) {
    try {
        std::vector<std::string> parts;
        std::stringstream ss(path);
        std::string part;

        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }

        nlohmann::json current = j;
        for (const auto& p : parts) {
            if (current.contains(p)) {
                current = current[p];
            } else {
                return std::nullopt;
            }
        }

        return current;
    } catch (...) {
        return std::nullopt;
    }
}

// ========== Implementation ==========

class QRLoginManager::Impl {
public:
    std::atomic<bool> cancelled{false};

    // HTTP GET request
    std::optional<std::string> httpGet(const std::string& url,
                                        const std::map<std::string, std::string>& headers) {
        try {
            HttpClient client;
            HttpOptions options;
            options.timeout = 30;
            for (const auto& [k, v] : headers) {
                options.headers[k] = v;
            }

            HttpResponse response = client.get(url, options);

            if (response.statusCode == 200) {
                return response.body;
            }
        } catch (...) {
            return std::nullopt;
        }
        return std::nullopt;
    }

    // HTTP POST request
    std::optional<std::string> httpPost(const std::string& url,
                                         const std::string& body,
                                         const std::map<std::string, std::string>& headers) {
        try {
            HttpClient client;
            HttpOptions options;
            options.timeout = 30;
            for (const auto& [k, v] : headers) {
                options.headers[k] = v;
            }

            HttpResponse response = client.post(url, body, options);

            if (response.statusCode == 200) {
                return response.body;
            }
        } catch (...) {
            return std::nullopt;
        }
        return std::nullopt;
    }

    // Build URL parameters
    std::string buildUrl(const std::string& baseUrl, const nlohmann::json& params) {
        if (baseUrl.empty()) {
            return "";
        }

        if (params.empty()) {
            return baseUrl;
        }

        std::stringstream ss;
        ss << baseUrl;
        ss << "?";

        bool first = true;
        for (auto& [key, value] : params.items()) {
            if (!first) ss << "&";
            first = false;

            std::string valueStr;
            if (value.is_string()) {
                valueStr = value.get<std::string>();
            } else {
                valueStr = value.dump();
            }

            // URL encoding (simple implementation)
            ss << key << "=" << valueStr;
        }

        return ss.str();
    }
};

QRLoginManager::QRLoginManager()
    : impl_(std::make_unique<Impl>()) {}

QRLoginManager::~QRLoginManager() {
    cancel();

    std::thread asyncThread;
    {
        std::lock_guard<std::mutex> lock(asyncMutex_);
        asyncThread = std::move(asyncThread_);
    }

    if (asyncThread.joinable()) {
        if (asyncThread.get_id() == std::this_thread::get_id()) {
            asyncThread.detach();
        } else {
            asyncThread.join();
        }
    }
}

// ========== Get QR Code ==========

std::optional<std::string> QRLoginManager::getQRCode(const QRLoginConfig& config) {
    impl_->cancelled = false;

    std::string url = impl_->buildUrl(config.qrUrl, config.qrParams);
    auto response = impl_->httpGet(url, config.headers);

    if (!response) {
        return std::nullopt;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(*response);
        auto qrCode = getJsonByPath(j, config.qrCodeField);

        if (qrCode && qrCode->is_string()) {
            return qrCode->get<std::string>();
        }
    } catch (...) {}

    // If parsing fails, the QR code content may have been returned directly
    return response;
}

std::optional<std::string> QRLoginManager::getQRCodeImage(const QRLoginConfig& config,
                                                           const std::string& outputPath) {
    auto qrCode = getQRCode(config);
    if (!qrCode) {
        return std::nullopt;
    }

    // TODO: Use qrencode library to generate QR code image
    // Save content as text for now
    std::ofstream file(outputPath);
    if (file.is_open()) {
        file << *qrCode;
        file.close();
        return outputPath;
    }

    return std::nullopt;
}

std::optional<std::string> QRLoginManager::detectQRCode(int x, int y, int width, int height) {
    // TODO: Use ZXing-C++ for QR code recognition
    std::cout << "[QR] Detecting QR code at (" << x << ", " << y << ") size "
              << width << "x" << height << "\n";
    std::cout << "[QR] QR code recognition not yet implemented\n";
    return std::nullopt;
}

// ========== Login Flow ==========

QRLoginResult QRLoginManager::login(const QRLoginConfig& config) {
    impl_->cancelled = false;

    QRLoginResult result;
    result.state = QRLoginState::Pending;
    result.message = "Waiting for QR code scan";

    auto startTime = std::chrono::steady_clock::now();
    int attempts = 0;

    while (!impl_->cancelled && attempts < config.maxAttempts) {
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed > config.timeout) {
            result.state = QRLoginState::Expired;
            result.message = "QR code expired";
            return result;
        }

        // Poll status
        QRLoginResult status = pollStatus(config);

        if (status.state == QRLoginState::Confirmed) {
            return status;
        } else if (status.state == QRLoginState::Error) {
            result.state = QRLoginState::Error;
            result.message = status.message;
            return result;
        } else if (status.state == QRLoginState::Expired) {
            return status;
        }

        // Update status
        result.state = status.state;
        result.message = status.message;

        // Wait for next poll
        std::this_thread::sleep_for(std::chrono::milliseconds(config.pollInterval));
        attempts++;
    }

    if (impl_->cancelled) {
        result.state = QRLoginState::Cancelled;
        result.message = "Login cancelled by user";
    } else {
        result.state = QRLoginState::Expired;
        result.message = "Maximum attempts reached";
    }

    return result;
}

void QRLoginManager::loginAsync(const QRLoginConfig& config,
                                 std::function<void(const QRLoginResult&)> callback) {
    std::thread worker([this, config, callback = std::move(callback)]() {
        QRLoginResult result = login(config);
        if (callback) {
            callback(result);
        }
    });

    std::lock_guard<std::mutex> lock(asyncMutex_);
    if (asyncThread_.joinable()) {
        asyncThread_.join();
    }
    asyncThread_ = std::move(worker);
}

QRLoginResult QRLoginManager::pollStatus(const QRLoginConfig& config) {
    QRLoginResult result;
    result.state = QRLoginState::Pending;

    // Validate status URL before attempting request
    if (config.statusUrl.empty()) {
        result.state = QRLoginState::Error;
        result.message = "Status URL is empty";
        return result;
    }

    std::string url = impl_->buildUrl(config.statusUrl, config.statusParams);

    // Add session ID
    if (!config.sessionId.empty()) {
        url += "&session_id=" + config.sessionId;
    }

    auto response = impl_->httpGet(url, config.headers);

    if (!response) {
        result.state = QRLoginState::Error;
        result.message = "Failed to poll status";
        return result;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(*response);
        result.data = j;

        // Parse status
        auto status = getJsonByPath(j, config.statusField);
        if (status && status->is_string()) {
            std::string statusStr = status->get<std::string>();

            if (statusStr == "scanned" || statusStr == "pending_confirm") {
                result.state = QRLoginState::Scanned;
                result.message = "QR code scanned, waiting for confirmation";
            } else if (statusStr == "confirmed" || statusStr == "success" || statusStr == "completed") {
                result.state = QRLoginState::Confirmed;
                result.message = "Login successful";

                // Extract token
                auto token = getJsonByPath(j, config.tokenField);
                if (token && token->is_string()) {
                    result.token = token->get<std::string>();
                }
            } else if (statusStr == "expired") {
                result.state = QRLoginState::Expired;
                result.message = "QR code expired";
            } else if (statusStr == "cancelled") {
                result.state = QRLoginState::Cancelled;
                result.message = "User cancelled login";
            } else if (statusStr == "error") {
                result.state = QRLoginState::Error;
                result.message = j.value("error", "Unknown error");
            }
        } else {
            // Try numeric status code
            auto statusCode = getJsonByPath(j, config.statusField);
            if (statusCode && statusCode->is_number()) {
                int code = statusCode->get<int>();
                switch (code) {
                    case 0:
                        result.state = QRLoginState::Pending;
                        result.message = "Waiting for scan";
                        break;
                    case 1:
                        result.state = QRLoginState::Scanned;
                        result.message = "Scanned, waiting for confirmation";
                        break;
                    case 2: {
                        result.state = QRLoginState::Confirmed;
                        result.message = "Login successful";

                        auto token = getJsonByPath(j, config.tokenField);
                        if (token && token->is_string()) {
                            result.token = token->get<std::string>();
                        }
                        break;
                    }
                    case 3:
                        result.state = QRLoginState::Expired;
                        result.message = "QR code expired";
                        break;
                    default:
                        result.state = QRLoginState::Error;
                        result.message = "Unknown status code";
                }
            }
        }
    } catch (const std::exception& e) {
        result.state = QRLoginState::Error;
        result.message = std::string("Failed to parse response: ") + e.what();
    }

    return result;
}

void QRLoginManager::cancel() {
    impl_->cancelled = true;
}

// ========== Preset Configurations ==========

QRLoginConfig QRLoginManager::steamConfig() {
    QRLoginConfig config;
    // Steam mobile login flow requires more complex handling
    // This is a simplified example
    config.qrUrl = "https://steamcommunity.com/login/homeqr";
    config.statusUrl = "https://steamcommunity.com/login/qrcoderestate";
    config.pollInterval = 3000;
    config.timeout = 180000;
    config.maxAttempts = 60;
    config.qrCodeField = "qr_challenge";
    config.statusField = "state";
    config.tokenField = "access_token";
    return config;
}

QRLoginConfig QRLoginManager::wechatConfig() {
    QRLoginConfig config;
    config.qrUrl = "https://open.weixin.qq.com/connect/qrconnect";
    config.statusUrl = "https://open.weixin.qq.com/connect/l/qrconnect";
    config.pollInterval = 2000;
    config.timeout = 120000;
    config.maxAttempts = 60;
    return config;
}

QRLoginConfig QRLoginManager::genericConfig(const std::string& qrUrl,
                                             const std::string& statusUrl) {
    QRLoginConfig config;
    config.qrUrl = qrUrl;
    config.statusUrl = statusUrl;
    return config;
}

} // namespace wingman
