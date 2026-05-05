# 扫码登录模块设计

## 概述

支持各种游戏的扫码登录流程：
- Steam 扫码登录
- 微信扫码登录
- QQ 扫码登录
- 其他游戏的二维码登录

## 扫码登录流程

```
1. 获取二维码图片
   ├─ 方式A: API 返回二维码图片 URL
   ├─ 方式B: API 返回二维码内容（需本地生成）
   └─ 方式C: 直接返回二维码图片数据

2. 显示二维码
   ├─ 保存到文件（供手动扫描）
   ├─ 在 GUI 窗口中显示
   └─ 截图识别（从游戏客户端中提取）

3. 轮询登录状态
   ├─ 短轮询：每隔 N 秒检查一次
   ├─ 长轮询：阻塞等待结果
   └─ WebSocket：实时推送

4. 获取登录凭证
   ├─ 返回 access_token
   ├─ 返回 cookie
   └─ 返回会话信息
```

## API 设计

### C++ API

```cpp
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

// 二维码登录配置
struct QRLoginConfig {
    // API 端点
    std::string qrUrl;           // 获取二维码的 URL
    std::string statusUrl;       // 查询状态的 URL
    std::string method = "GET";  // HTTP 方法

    // 参数配置
    nlohmann::json qrParams;     // 获取二维码的参数
    nlohmann::json statusParams; // 查询状态的参数

    // 响应解析
    std::string qrCodeField;     // 二维码内容字段（JSON 路径）
    std::string statusField;     // 状态字段
    std::string tokenField;      // token 字段

    // 轮询配置
    int pollInterval = 2000;     // 轮询间隔（毫秒）
    int timeout = 120000;        // 超时时间（毫秒）
    int maxAttempts = 60;        // 最大尝试次数

    // 头部配置
    std::map<std::string, std::string> headers;
};

// 登录结果
struct QRLoginResult {
    QRLoginState state;
    std::string message;
    std::string token;           // 登录凭证
    nlohmann::json data;         // 完整响应数据
};

// 扫码登录管理器
class QRLoginManager {
public:
    // ========== 获取二维码 ==========

    // 获取二维码（返回内容）
    std::optional<std::string> getQRCode(const QRLoginConfig& config);

    // 获取二维码（保存图片）
    std::optional<std::string> getQRCodeImage(const QRLoginConfig& config,
                                               const std::string& outputPath);

    // 从屏幕截图中识别二维码
    std::optional<std::string> detectQRCode(int x, int y, int width, int height);

    // ========== 登录流程 ==========

    // 执行完整的扫码登录流程（阻塞）
    QRLoginResult login(const QRLoginConfig& config);

    // 执行扫码登录（异步，带回调）
    void loginAsync(const QRLoginConfig& config,
                    std::function<void(const QRLoginResult&)> callback);

    // 轮询登录状态
    QRLoginResult pollStatus(const QRLoginConfig& config,
                             const std::string& sessionId);

    // 取消登录
    void cancel();

    // ========== 预设配置 ==========

    // Steam 扫码登录配置
    static QRLoginConfig steamConfig();

    // 微信扫码登录配置
    static QRLoginConfig wechatConfig();

    // QQ 扫码登录配置
    static QRLoginConfig qqConfig();
};

} // namespace wingman
```

### Lua API

```lua
-- 获取二维码
local qr = qrcode.login({
    qrUrl = "https://api.example.com/qrcode",
    statusUrl = "https://api.example.com/status",
    pollInterval = 2000,
    timeout = 120000
})

-- 等待登录结果
if qr.state == "success" then
    print("登录成功，token: " .. qr.token)
end

-- Steam 扫码登录示例
local steam = qrcode.steam()
local result = steam:login()
if result.state == "success" then
    print("Steam 登录成功")
    print("Token: " .. result.token)
end

-- 从屏幕识别二维码
local code = qrcode.detect(x, y, width, height)
print("识别到的二维码内容: " .. code)
```

## 预设游戏配置

### Steam 扫码登录

```cpp
QRLoginConfig QRLoginManager::steamConfig() {
    QRLoginConfig config;
    config.qrUrl = "https://api.steampowered.com/ISteamRemoteDiscovery/GetProtobufManifest/v1/";
    // Steam 实际流程更复杂，需要 RsaKey 等
    config.pollInterval = 3000;
    config.timeout = 180000;
    return config;
}
```

### 微信扫码登录

```cpp
QRLoginConfig QRLoginManager::wechatConfig() {
    QRLoginConfig config;
    config.qrUrl = "https://open.weixin.qq.com/connect/qrconnect";
    config.qrParams = {
        {"appid", "YOUR_APPID"},
        {"redirect_uri", "YOUR_REDIRECT_URI"},
        {"response_type", "code"}
    };
    config.statusUrl = "https://open.weixin.qq.com/connect/l/qrconnect";
    config.pollInterval = 2000;
    config.timeout = 120000;
    return config;
}
```

## 二维码识别

### 使用 ZXing-C++

```cmake
# CMakeLists.txt
find_package(ZXing REQUIRED)
target_link_libraries(wingman ZXing::ZXing)
```

### OpenCV + ZXing

```cpp
#include <ZXing/ReadBarcode.h>
#include <ZXing/TextUtfEncoding.h>
#include <opencv2/opencv.hpp>

std::optional<std::string> detectQRCode(cv::Mat image) {
    auto hints = ZXing::ReaderOptions();
    hints.setFormats(ZXing::BarcodeFormat::QR_CODE);

    auto result = ZXing::ReadBarcode(image, hints);
    if (result.isValid()) {
        return ZXing::TextUtfEncoding::ToUtf8(result.text());
    }
    return std::nullopt;
}
```

## 实现文件结构

```
include/wingman/qrcode.hpp       # 头文件
src/qrcode.cpp                    # 核心实现
src/qrcode_detector.cpp           # 二维码识别
src/qrcode_generator.cpp          # 二维码生成
bindings/lua_qrcode.cpp           # Lua 绑定
```

## 完整示例

### Steam 扫码登录流程

```lua
-- 1. 获取 Steam 二维码
local steam = qrcode.steam()
local qrImage = steam:getQRImage("steam_qr.png")

print("请扫描二维码登录 Steam")
print("二维码已保存到: " .. qrImage)

-- 2. 开始轮询登录状态
local result = steam:login()

-- 3. 处理结果
if result.state == "success" then
    print("登录成功！")
    print("Access Token: " .. result.token)
    print("Steam ID: " .. result.data.steam_id)

    -- 保存凭证
    config.set("steam_token", result.token)
elseif result.state == "expired" then
    print("二维码已过期，请重试")
elseif result.state == "cancelled" then
    print("用户取消登录")
else
    print("登录失败: " .. result.message)
end
```

### 游戏内二维码识别

```lua
-- 1. 找到游戏窗口
local hwnd = window.find("GameWindow")

-- 2. 截取二维码区域
local x, y, w, h = window.getBounds(hwnd)
local qrX = x + w - 200
local qrY = y + 100
local qrW = 150
local qrH = 150

-- 3. 识别二维码
local code = qrcode.detect(qrX, qrY, qrW, qrH)

if code then
    print("识别到二维码: " .. code)

    -- 4. 解析并使用
    local data = json.decode(code)
    print("登录会话: " .. data.session_id)
else
    print("未识别到二维码")
end
```

## 技术依赖

### 二维码生成
- **libqrencode** (C) - 轻量级 QR 码生成库
- **ZXing-C++** - 跨平台条码库

### 二维码识别
- **ZXing-C++** - 推荐，支持多种格式
- **OpenCV + ZXing** - 结合使用

### HTTP 请求
- **libcurl** - 已有依赖

## 参考资料

- ZXing-C++: https://github.com/zxing-cpp/zxing-cpp
- libqrencode: https://github.com/fukuchi/libqrencode
- Steam 登录协议: https://github.com/BelowZero5/steam-login-flow
