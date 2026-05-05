#include "wingman/qrcode.hpp"
#include "wingman/http.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <fstream>

namespace wingman {

// ========== JSON 路径解析 ==========

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

    // HTTP GET 请求
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

    // HTTP POST 请求
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

    // 构建 URL 参数
    std::string buildUrl(const std::string& baseUrl, const nlohmann::json& params) {
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

            // URL 编码（简单实现）
            ss << key << "=" << valueStr;
        }

        return ss.str();
    }
};

QRLoginManager::QRLoginManager()
    : impl_(std::make_unique<Impl>()) {}

QRLoginManager::~QRLoginManager() = default;

// ========== 获取二维码 ==========

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

    // 如果解析失败，可能直接返回的是二维码内容
    return response;
}

std::optional<std::string> QRLoginManager::getQRCodeImage(const QRLoginConfig& config,
                                                           const std::string& outputPath) {
    auto qrCode = getQRCode(config);
    if (!qrCode) {
        return std::nullopt;
    }

    // TODO: 使用 qrencode 库生成二维码图片
    // 目前先保存内容为文本
    std::ofstream file(outputPath);
    if (file.is_open()) {
        file << *qrCode;
        file.close();
        return outputPath;
    }

    return std::nullopt;
}

std::optional<std::string> QRLoginManager::detectQRCode(int x, int y, int width, int height) {
    // TODO: 使用 ZXing-C++ 实现二维码识别
    std::cout << "[QR] Detecting QR code at (" << x << ", " << y << ") size "
              << width << "x" << height << "\n";
    std::cout << "[QR] QR code recognition not yet implemented\n";
    return std::nullopt;
}

// ========== 登录流程 ==========

QRLoginResult QRLoginManager::login(const QRLoginConfig& config) {
    impl_->cancelled = false;

    QRLoginResult result;
    result.state = QRLoginState::Pending;
    result.message = "Waiting for QR code scan";

    auto startTime = std::chrono::steady_clock::now();
    int attempts = 0;

    while (!impl_->cancelled && attempts < config.maxAttempts) {
        // 检查超时
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed > config.timeout) {
            result.state = QRLoginState::Expired;
            result.message = "QR code expired";
            return result;
        }

        // 轮询状态
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

        // 更新状态
        result.state = status.state;
        result.message = status.message;

        // 等待下次轮询
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
    std::thread([this, config, callback]() {
        QRLoginResult result = login(config);
        callback(result);
    }).detach();
}

QRLoginResult QRLoginManager::pollStatus(const QRLoginConfig& config) {
    QRLoginResult result;
    result.state = QRLoginState::Pending;

    std::string url = impl_->buildUrl(config.statusUrl, config.statusParams);

    // 添加会话 ID
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

        // 解析状态
        auto status = getJsonByPath(j, config.statusField);
        if (status && status->is_string()) {
            std::string statusStr = status->get<std::string>();

            if (statusStr == "scanned" || statusStr == "pending_confirm") {
                result.state = QRLoginState::Scanned;
                result.message = "QR code scanned, waiting for confirmation";
            } else if (statusStr == "confirmed" || statusStr == "success" || statusStr == "completed") {
                result.state = QRLoginState::Confirmed;
                result.message = "Login successful";

                // 提取 token
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
            // 尝试数字状态码
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

// ========== 预设配置 ==========

QRLoginConfig QRLoginManager::steamConfig() {
    QRLoginConfig config;
    // Steam 移动端登录流程需要更复杂的处理
    // 这里是简化的示例
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
