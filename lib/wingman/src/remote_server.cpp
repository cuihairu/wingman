#include "wingman/remote_server.hpp"

#ifdef _WIN32
#include "wingman/version.hpp"
#include "wingman/window.hpp"
#include "wingman/vision.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <vector>
#include <opencv2/opencv.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// ========== Base64 编码辅助函数 ==========

static std::string base64Encode(const std::vector<uchar>& data) {
    const std::string base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string base64;
    base64.reserve((data.size() * 4) / 3 + 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        auto b0 = data[i];
        auto b1 = (i + 1 < data.size()) ? data[i + 1] : 0;
        auto b2 = (i + 2 < data.size()) ? data[i + 2] : 0;

        base64.push_back(base64Chars[(b0 >> 2) & 0x3F]);
        base64.push_back(base64Chars[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        base64.push_back((i + 1 < data.size()) ? base64Chars[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=');
        base64.push_back((i + 2 < data.size()) ? base64Chars[b2 & 0x3F] : '=');
    }

    return base64;
}

namespace wingman {

// ========== RemoteRequest ==========

RemoteRequest RemoteRequest::fromJson(const std::string& jsonStr) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonStr);
        return fromJson(j);
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse RemoteRequest: {}", e.what());
        return RemoteRequest{};
    }
}

RemoteRequest RemoteRequest::fromJson(const nlohmann::json& j) {
    RemoteRequest req;
    req.action = j.value("action", "");
    req.params = j.value("params", nlohmann::json{});
    return req;
}

nlohmann::json RemoteRequest::toJson() const {
    nlohmann::json j;
    j["action"] = action;
    j["params"] = params;
    return j;
}

// ========== RemoteResponse ==========

std::string RemoteResponse::toJsonString() const {
    nlohmann::json j;
    j["success"] = success;
    j["data"] = data;
    j["error"] = error;
    return j.dump();
}

RemoteResponse RemoteResponse::fromJson(const nlohmann::json& j) {
    RemoteResponse resp;
    resp.success = j.value("success", false);
    resp.data = j.value("data", nlohmann::json{});
    resp.error = j.value("error", "");
    return resp;
}

// ========== RemoteServer 实现 ==========

class RemoteServer::Impl {
public:
    SOCKET listenSocket = INVALID_SOCKET;
    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    std::mutex clientThreadsMutex;
    bool shouldStop = false;
};

RemoteServer::RemoteServer()
    : impl_(std::make_unique<Impl>())
    , triggerManager_(std::make_unique<TriggerManager>())
    , macroRecorder_(std::make_unique<MacroRecorder>())
{
}

RemoteServer::~RemoteServer() {
    stop();
}

bool RemoteServer::start(int port) {
    if (running_) {
        spdlog::warn("RemoteServer already running on port {}", port_);
        return true;
    }

    port_ = port;

    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("WSAStartup failed");
        return false;
    }

    // 创建监听socket
    impl_->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->listenSocket == INVALID_SOCKET) {
        spdlog::error("socket creation failed");
        WSACleanup();
        return false;
    }

    // 允许地址重用，避免 TIME_WAIT 导致的端口冲突
    int reuseAddr = 1;
    setsockopt(impl_->listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddr, sizeof(reuseAddr));

    // 绑定地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(impl_->listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        spdlog::error("bind failed");
        closesocket(impl_->listenSocket);
        WSACleanup();
        return false;
    }

    // 开始监听
    if (listen(impl_->listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        spdlog::error("listen failed");
        closesocket(impl_->listenSocket);
        WSACleanup();
        return false;
    }

    running_ = true;
    spdlog::info("RemoteServer started on port {}", port_);

    // 启动接受连接线程
    impl_->acceptThread = std::thread([this]() {
        while (running_ && !impl_->shouldStop) {
            sockaddr_in clientAddr{};
            int clientAddrLen = sizeof(clientAddr);

            SOCKET clientSocket = accept(impl_->listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket == INVALID_SOCKET) {
                if (running_) {
                    spdlog::error("accept failed");
                }
                continue;
            }

            // 获取客户端地址
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            spdlog::info("Client connected from {}:{}", clientIP, ntohs(clientAddr.sin_port));

            // 设置接收超时，避免线程永久阻塞
            DWORD timeout = 1000; // 1秒超时
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

            // 启动客户端处理线程
            {
                std::lock_guard<std::mutex> lock(impl_->clientThreadsMutex);
                connectionCount_++;
            }

            impl_->clientThreads.emplace_back([this, clientSocket]() {
                handleClient(clientSocket);
            });
        }
    });

    return true;
}

void RemoteServer::stop() {
    if (!running_) return;

    running_ = false;
    impl_->shouldStop = true;

    // 关闭监听socket
    if (impl_->listenSocket != INVALID_SOCKET) {
        closesocket(impl_->listenSocket);
        impl_->listenSocket = INVALID_SOCKET;
    }

    // 等待所有线程结束
    if (impl_->acceptThread.joinable()) {
        impl_->acceptThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(impl_->clientThreadsMutex);
        for (auto& t : impl_->clientThreads) {
            if (t.joinable()) {
                t.join();
            }
        }
        impl_->clientThreads.clear();
    }

    WSACleanup();
    spdlog::info("RemoteServer stopped");
}

void RemoteServer::handleClient(SOCKET clientSocket) {
    char buffer[4096];
    std::string requestBuffer;

    while (running_ && !impl_->shouldStop) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;

        buffer[bytesRead] = '\0';
        requestBuffer.append(buffer);

        // 尝试解析JSON请求
        try {
            auto j = nlohmann::json::parse(requestBuffer);
            RemoteRequest req = RemoteRequest::fromJson(j);

            // 处理请求
            RemoteResponse resp = handleRequest(req);

            // 发送响应
            std::string respStr = resp.toJsonString();
            send(clientSocket, respStr.c_str(), static_cast<int>(respStr.length()), 0);

            requestBuffer.clear();
        } catch (const nlohmann::json::parse_error&) {
            // 数据不完整，继续等待
            continue;
        } catch (const std::exception& e) {
            spdlog::error("Error handling request: {}", e.what());
            RemoteResponse resp;
            resp.success = false;
            resp.error = e.what();
            std::string respStr = resp.toJsonString();
            send(clientSocket, respStr.c_str(), static_cast<int>(respStr.length()), 0);
            requestBuffer.clear();
        }
    }

    closesocket(clientSocket);

    {
        std::lock_guard<std::mutex> lock(impl_->clientThreadsMutex);
        --connectionCount_;
    }
}

RemoteResponse RemoteServer::handleRequest(const RemoteRequest& req) {
    try {
        if (req.action == "ping") return handlePing(req.params);
        if (req.action == "get_version") return handleGetVersion(req.params);
        if (req.action == "capture_screen") return handleCaptureScreen(req.params);
        if (req.action == "get_pixel") return handleGetPixel(req.params);
        if (req.action == "find_color") return handleFindColor(req.params);
        if (req.action == "find_image") return handleFindImage(req.params);
        if (req.action == "click") return handleClick(req.params);
        if (req.action == "move") return handleMove(req.params);
        if (req.action == "key") return handleKey(req.params);
        if (req.action == "type_text") return handleTypeText(req.params);
        if (req.action == "list_triggers") return handleListTriggers(req.params);
        if (req.action == "add_trigger") return handleAddTrigger(req.params);
        if (req.action == "remove_trigger") return handleRemoveTrigger(req.params);
        if (req.action == "enable_trigger") return handleEnableTrigger(req.params);
        if (req.action == "disable_trigger") return handleDisableTrigger(req.params);
        if (req.action == "record_macro") return handleRecordMacro(req.params);
        if (req.action == "stop_macro_recording") return handleStopMacroRecording(req.params);
        if (req.action == "play_macro") return handlePlayMacro(req.params);

        RemoteResponse resp;
        resp.success = false;
        resp.error = "Unknown action: " + req.action;
        return resp;
    } catch (const std::exception& e) {
        RemoteResponse resp;
        resp.success = false;
        resp.error = e.what();
        return resp;
    }
}

// ========== 动作处理器 ==========

RemoteResponse RemoteServer::handlePing(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    resp.success = true;
    resp.data["status"] = "ok";
    resp.data["version"] = WINGMAN_VERSION;
    return resp;
}

RemoteResponse RemoteServer::handleGetVersion(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    resp.success = true;
    resp.data["version"] = WINGMAN_VERSION;
    resp.data["name"] = "Wingman";
    return resp;
}

RemoteResponse RemoteServer::handleCaptureScreen(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        // 获取参数
        int x = params.value("x", 0);
        int y = params.value("y", 0);
        int reqWidth = params.value("width", 0);
        int reqHeight = params.value("height", 0);
        int quality = params.value("quality", 75);  // JPEG 质量 1-100
        std::string format = params.value("format", "jpeg");  // jpeg 或 png

        // 截图
        Rect region(x, y, reqWidth, reqHeight);
        auto bitmap = region.isEmpty() ? Screen::capture() : Screen::capture(region);

        if (!bitmap) {
            resp.success = false;
            resp.error = "Failed to capture screen";
            return resp;
        }

        // 转换为 OpenCV Mat
        cv::Mat mat(bitmap->getHeight(), bitmap->getWidth(), CV_8UC4,
                    const_cast<uchar*>(bitmap->getData()));

        // 转换为 BGR
        cv::Mat bgrMat;
        cv::cvtColor(mat, bgrMat, cv::COLOR_BGRA2BGR);

        // 编码为图像
        std::vector<uchar> buffer;
        std::string ext = (format == "png") ? ".png" : ".jpg";
        std::vector<int> encodeParams;

        if (format == "png") {
            encodeParams = {cv::IMWRITE_PNG_COMPRESSION, 9};  // 最高压缩
        } else {
            encodeParams = {cv::IMWRITE_JPEG_QUALITY, quality};
        }

        if (!cv::imencode(ext, bgrMat, buffer, encodeParams)) {
            resp.success = false;
            resp.error = "Failed to encode image";
            return resp;
        }

        // Base64 编码
        std::string base64 = base64Encode(buffer);

        // 构建响应
        resp.success = true;
        resp.data["width"] = bitmap->getWidth();
        resp.data["height"] = bitmap->getHeight();
        resp.data["format"] = format;
        resp.data["size"] = static_cast<int>(buffer.size());
        resp.data["data"] = base64;  // Base64 编码的图像数据

        spdlog::debug("Screenshot captured: {}x{}, {} bytes, base64: {} chars",
                     bitmap->getWidth(), bitmap->getHeight(), buffer.size(), base64.size());

    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = std::string("Screenshot error: ") + e.what();
        spdlog::error("Screenshot error: {}", e.what());
    }

    return resp;
}

RemoteResponse RemoteServer::handleFindImage(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        std::string templatePath = params["template_path"];
        double threshold = params.value("threshold", 0.8);

        // 解析搜索区域
        bool hasRegion = params.contains("region");
        ImageMatch result;

        if (hasRegion) {
            Rect region;
            region.x = params["region"].value("x", 0);
            region.y = params["region"].value("y", 0);
            region.width = params["region"].value("width", 0);
            region.height = params["region"].value("height", 0);
            result = Vision::findImage(templatePath, region, threshold);
        } else {
            result = Vision::findImage(templatePath, threshold);
        }

        resp.success = result.found;
        if (result.found) {
            resp.data["x"] = result.position.x;
            resp.data["y"] = result.position.y;
            resp.data["confidence"] = result.confidence;
            resp.data["region"]["x"] = result.region.x;
            resp.data["region"]["y"] = result.region.y;
            resp.data["region"]["width"] = result.region.width;
            resp.data["region"]["height"] = result.region.height;
        }
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteServer::handleGetPixel(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params["x"];
    int y = params["y"];

    Color color = Screen::getPixel(x, y);
    resp.success = true;
    resp.data["r"] = color.r;
    resp.data["g"] = color.g;
    resp.data["b"] = color.b;

    // 转换为hex字符串
    char hex[16];
    sprintf_s(hex, sizeof(hex), "0x%02X%02X%02X", color.r, color.g, color.b);
    resp.data["hex"] = hex;

    return resp;
}

RemoteResponse RemoteServer::handleFindColor(const nlohmann::json& params) {
    RemoteResponse resp;

    std::string colorStr = params["color"];
    uint32_t color = std::stoul(colorStr, nullptr, 0);

    int x = params.value("x", 0);
    int y = params.value("y", 0);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int width = params.value("width", screenWidth);
    int height = params.value("height", screenHeight);
    int tolerance = params.value("tolerance", 10);

    Rect region{ x, y, width, height };
    Point result;
    bool found = Screen::findColor(Color::fromRGB(color), region, tolerance, result);

    resp.success = found;
    if (found) {
        resp.data["x"] = result.x;
        resp.data["y"] = result.y;
    }
    return resp;
}

RemoteResponse RemoteServer::handleClick(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params["x"];
    int y = params["y"];
    std::string button = params.value("button", "left");

    MouseButton btn = MouseButton::Left;
    if (button == "right") btn = MouseButton::Right;
    else if (button == "middle") btn = MouseButton::Middle;

    Input::click(x, y, btn);
    resp.success = true;
    return resp;
}

RemoteResponse RemoteServer::handleMove(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params["x"];
    int y = params["y"];
    int duration = params.value("duration", 100);

    Input::move(x, y, duration);
    resp.success = true;
    return resp;
}

RemoteResponse RemoteServer::handleKey(const nlohmann::json& params) {
    RemoteResponse resp;

    int keyCode = params["key"];
    Input::key(keyCode);
    resp.success = true;
    return resp;
}

RemoteResponse RemoteServer::handleTypeText(const nlohmann::json& params) {
    RemoteResponse resp;

    std::string text = params["text"];
    int delay = params.value("delay", 50);

    Input::type(text, delay);
    resp.success = true;
    return resp;
}

RemoteResponse RemoteServer::handleAddTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        // 解析触发器配置
        TriggerConfig config;
        config.name = params["config"]["name"];
        config.enabled = params["config"].value("enabled", true);
        config.oneShot = params["config"].value("one_shot", false);
        config.cooldown = params["config"].value("cooldown", 0);

        // 解析条件
        const auto& cond = params["config"]["condition"];
        std::string typeStr = cond.value("type", "ColorFound");
        if (typeStr == "ColorFound") config.condition.type = TriggerType::ColorFound;
        else if (typeStr == "ColorLost") config.condition.type = TriggerType::ColorLost;
        else if (typeStr == "ImageFound") config.condition.type = TriggerType::ImageFound;
        else if (typeStr == "ImageLost") config.condition.type = TriggerType::ImageLost;
        else config.condition.type = TriggerType::ColorFound;

        config.condition.value = cond.value("value", "");
        config.condition.tolerance = cond.value("tolerance", 10);
        config.condition.interval = cond.value("interval", 100);

        if (cond.contains("region")) {
            config.condition.region.x = cond["region"].value("x", 0);
            config.condition.region.y = cond["region"].value("y", 0);
            config.condition.region.width = cond["region"].value("width", 0);
            config.condition.region.height = cond["region"].value("height", 0);
        }

        // 解析动作
        if (params["config"].contains("actions")) {
            for (const auto& actionJson : params["config"]["actions"]) {
                TriggerActionData action;
                std::string actionType = actionJson.value("type", "Click");
                if (actionType == "RunScript") action.type = BasicTriggerAction::RunScript;
                else if (actionType == "Click") action.type = BasicTriggerAction::Click;
                else if (actionType == "KeyPress") action.type = BasicTriggerAction::KeyPress;
                else if (actionType == "Type") action.type = BasicTriggerAction::Type;
                else action.type = BasicTriggerAction::Click;

                action.value = actionJson.value("value", "");
                action.x = actionJson.value("x", 0);
                action.y = actionJson.value("y", 0);
                action.delay = actionJson.value("delay", 0);
                config.actions.push_back(action);
            }
        }

        // 添加触发器
        size_t id = triggerManager_->add(config);

        resp.success = true;
        resp.data["id"] = id;
        resp.data["message"] = "Trigger added";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteServer::handleRemoveTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        size_t id = params["id"];
        triggerManager_->remove(id);
        resp.success = true;
        resp.data["message"] = "Trigger removed";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteServer::handleEnableTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        size_t id = params["id"];
        triggerManager_->enable(id);
        resp.success = true;
        resp.data["message"] = "Trigger enabled";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteServer::handleDisableTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    try {
        size_t id = params["id"];
        triggerManager_->disable(id);
        resp.success = true;
        resp.data["message"] = "Trigger disabled";
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteServer::handleListTriggers(const nlohmann::json& /*params*/) {
    RemoteResponse resp;

    auto triggers = triggerManager_->getAllTriggerConfigs();
    nlohmann::json j = nlohmann::json::array();
    for (const auto& t : triggers) {
        nlohmann::json item;
        item["name"] = t.name;
        item["enabled"] = t.enabled;
        j.push_back(item);
    }

    resp.success = true;
    resp.data["triggers"] = j;
    resp.data["count"] = triggers.size();
    return resp;
}

RemoteResponse RemoteServer::handleRecordMacro(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    macroRecorder_->start();
    resp.success = true;
    resp.data["message"] = "Recording started";
    return resp;
}

RemoteResponse RemoteServer::handleStopMacroRecording(const nlohmann::json& /*params*/) {
    RemoteResponse resp;
    macroRecorder_->stop();
    resp.success = true;
    resp.data["event_count"] = macroRecorder_->getEventCount();
    return resp;
}

RemoteResponse RemoteServer::handlePlayMacro(const nlohmann::json& params) {
    RemoteResponse resp;
    int speed = params.value("speed", 100);
    int repeat = params.value("repeat", 1);
    macroRecorder_->playback(speed, repeat);
    resp.success = true;
    return resp;
}

// ========== RemoteClient 实现 ==========

class RemoteClient::Impl {
public:
    SOCKET socket = INVALID_SOCKET;
};

RemoteClient::RemoteClient()
    : impl_(std::make_unique<Impl>())
{
}

RemoteClient::~RemoteClient() {
    disconnect();
}

bool RemoteClient::connect(const std::string& host, int port) {
    host_ = host;
    port_ = port;

    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        spdlog::error("WSAStartup failed");
        return false;
    }

    // 创建socket
    impl_->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->socket == INVALID_SOCKET) {
        spdlog::error("socket creation failed");
        WSACleanup();
        return false;
    }

    // 连接服务器
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port_));

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        // 尝试解析域名
        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0) {
            serverAddr.sin_addr = ((sockaddr_in*)result->ai_addr)->sin_addr;
            freeaddrinfo(result);
        } else {
            spdlog::error("Invalid address: {}", host);
            closesocket(impl_->socket);
            WSACleanup();
            return false;
        }
    }

    if (::connect(impl_->socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        spdlog::error("Connection to {}:{} failed", host, port);
        closesocket(impl_->socket);
        WSACleanup();
        return false;
    }

    // 设置接收超时
    DWORD timeout = 5000; // 5秒超时
    setsockopt(impl_->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    connected_ = true;
    spdlog::info("Connected to {}:{}", host, port);
    return true;
}

void RemoteClient::disconnect() {
    if (!connected_) return;

    if (impl_->socket != INVALID_SOCKET) {
        closesocket(impl_->socket);
        impl_->socket = INVALID_SOCKET;
    }

    WSACleanup();
    connected_ = false;
    spdlog::info("Disconnected from {}:{}", host_, port_);
}

RemoteResponse RemoteClient::send(const RemoteRequest& req) {
    RemoteResponse resp;
    if (!connected_) {
        resp.success = false;
        resp.error = "Not connected";
        return resp;
    }

    std::string reqStr = req.toJson().dump();
    if (::send(impl_->socket, reqStr.c_str(), static_cast<int>(reqStr.length()), 0) == SOCKET_ERROR) {
        resp.success = false;
        resp.error = "Send failed";
        return resp;
    }

    // 接收响应
    char buffer[4096];
    int bytesRead = recv(impl_->socket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        resp.success = false;
        resp.error = "Receive failed";
        return resp;
    }

    buffer[bytesRead] = '\0';
    try {
        nlohmann::json j = nlohmann::json::parse(buffer);
        resp = RemoteResponse::fromJson(j);
    } catch (const std::exception& e) {
        resp.success = false;
        resp.error = e.what();
    }

    return resp;
}

RemoteResponse RemoteClient::send(const std::string& action, const nlohmann::json& params) {
    RemoteRequest req;
    req.action = action;
    req.params = params;
    return send(req);
}

RemoteResponse RemoteClient::captureScreen(int x, int y, int width, int height) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    params["width"] = width;
    params["height"] = height;
    return send("capture_screen", params);
}

RemoteResponse RemoteClient::getPixel(int x, int y) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    return send("get_pixel", params);
}

RemoteResponse RemoteClient::findColor(uint32_t color, int x, int y, int width, int height, int tolerance) {
    nlohmann::json params;

    // 将颜色转换为hex字符串
    char colorHex[16];
    sprintf_s(colorHex, sizeof(colorHex), "0x%06X", color);
    params["color"] = colorHex;

    params["x"] = x;
    params["y"] = y;
    params["width"] = width;
    params["height"] = height;
    params["tolerance"] = tolerance;
    return send("find_color", params);
}

RemoteResponse RemoteClient::click(int x, int y, const std::string& button) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    params["button"] = button;
    return send("click", params);
}

RemoteResponse RemoteClient::move(int x, int y, int durationMs) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    params["duration"] = durationMs;
    return send("move", params);
}

RemoteResponse RemoteClient::key(int keyCode) {
    nlohmann::json params;
    params["key"] = keyCode;
    return send("key", params);
}

RemoteResponse RemoteClient::typeText(const std::string& text, int delayMs) {
    nlohmann::json params;
    params["text"] = text;
    params["delay"] = delayMs;
    return send("type_text", params);
}

RemoteResponse RemoteClient::addTrigger(const nlohmann::json& config) {
    nlohmann::json params;
    params["config"] = config;
    return send("add_trigger", params);
}

RemoteResponse RemoteClient::listTriggers() {
    return send("list_triggers");
}

RemoteResponse RemoteClient::enableTrigger(const std::string& id) {
    nlohmann::json params;
    params["id"] = id;
    return send("enable_trigger", params);
}

RemoteResponse RemoteClient::disableTrigger(const std::string& id) {
    nlohmann::json params;
    params["id"] = id;
    return send("disable_trigger", params);
}

RemoteResponse RemoteClient::ping() {
    return send("ping");
}

} // namespace wingman

#endif // _WIN32
