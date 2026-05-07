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

// 登录状态
enum class QRLoginState {
    Pending,       // 等待扫码
    Scanned,       // 已扫码，等待确认
    Confirmed,     // 已确认，登录成功
    Expired,       // 二维码过期
    Cancelled,     // 用户取消
    Error          // 错误
};

// 登录状态转字符串
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

// 二维码登录配置
struct QRLoginConfig {
    // API 端点
    std::string qrUrl;           // 获取二维码的 URL
    std::string statusUrl;       // 查询状态的 URL
    std::string method = "GET";  // HTTP 方法

    // 参数配置
    nlohmann::json qrParams;     // 获取二维码的参数
    nlohmann::json statusParams; // 查询状态的参数

    // 响应解析（JSON 路径）
    std::string qrCodeField = "qr_code";      // 二维码内容字段
    std::string statusField = "status";       // 状态字段
    std::string tokenField = "token";         // token 字段
    std::string sessionIdField = "session_id"; // 会话 ID 字段

    // 轮询配置
    int pollInterval = 2000;     // 轮询间隔（毫秒）
    int timeout = 120000;        // 超时时间（毫秒）
    int maxAttempts = 60;        // 最大尝试次数

    // 头部配置
    std::map<std::string, std::string> headers;

    // 会话 ID（从响应中获取）
    std::string sessionId;
};

// 登录结果
struct QRLoginResult {
    QRLoginState state;
    std::string message;
    std::string token;           // 登录凭证
    std::string sessionId;       // 会话 ID
    nlohmann::json data;         // 完整响应数据
};

// 扫码登录管理器
class QRLoginManager {
public:
    QRLoginManager();
    ~QRLoginManager();

    // ========== 获取二维码 ==========

    // 获取二维码内容
    std::optional<std::string> getQRCode(const QRLoginConfig& config);

    // 获取二维码并保存为图片
    std::optional<std::string> getQRCodeImage(const QRLoginConfig& config,
                                               const std::string& outputPath);

    // 从屏幕区域识别二维码
    std::optional<std::string> detectQRCode(int x, int y, int width, int height);

    // ========== 登录流程 ==========

    // 执行完整的扫码登录流程（阻塞）
    QRLoginResult login(const QRLoginConfig& config);

    // 执行扫码登录（异步，带回调）
    void loginAsync(const QRLoginConfig& config,
                    std::function<void(const QRLoginResult&)> callback);

    // 轮询登录状态
    QRLoginResult pollStatus(const QRLoginConfig& config);

    // 取消登录
    void cancel();

    // ========== 预设配置 ==========

    // Steam 扫码登录配置
    static QRLoginConfig steamConfig();

    // 微信扫码登录配置
    static QRLoginConfig wechatConfig();

    // 通用扫码登录配置
    static QRLoginConfig genericConfig(const std::string& qrUrl,
                                        const std::string& statusUrl);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman
