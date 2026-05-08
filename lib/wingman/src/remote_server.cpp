#include "wingman/remote_server.hpp"
#include "wingman/window.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

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

    // 绑定地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

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
        if (req.action == "click") return handleClick(req.params);
        if (req.action == "move") return handleMove(req.params);
        if (req.action == "key") return handleKey(req.params);
        if (req.action == "type_text") return handleTypeText(req.params);
        if (req.action == "list_triggers") return handleListTriggers(req.params);
        if (req.action == "enable_trigger") return handleEnableTrigger(req.params);
        if (req.action == "disable_trigger") return handleDisableTrigger(req.params);

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

RemoteResponse RemoteServer::handlePing(const nlohmann::json& params) {
    RemoteResponse resp;
    resp.success = true;
    resp.data["status"] = "ok";
    resp.data["version"] = "0.2.0";
    return resp;
}

RemoteResponse RemoteServer::handleGetVersion(const nlohmann::json& params) {
    RemoteResponse resp;
    resp.success = true;
    resp.data["version"] = "0.2.0";
    resp.data["name"] = "Wingman";
    return resp;
}

RemoteResponse RemoteServer::handleCaptureScreen(const nlohmann::json& params) {
    RemoteResponse resp;

    int x = params.value("x", 0);
    int y = params.value("y", 0);
    int width = params.value("width", 0);
    int height = params.value("height", 0);

    // TODO: 实现截图功能（需要返回base64编码的图像）
    resp.success = true;
    resp.data["width"] = width;
    resp.data["height"] = height;
    resp.data["message"] = "Screenshot not fully implemented";
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
    // TODO: 实现添加触发器
    resp.success = false;
    resp.error = "Not implemented";
    return resp;
}

RemoteResponse RemoteServer::handleRemoveTrigger(const nlohmann::json& params) {
    RemoteResponse resp;
    // TODO: 实现移除触发器
    resp.success = false;
    resp.error = "Not implemented";
    return resp;
}

RemoteResponse RemoteServer::handleEnableTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    std::string id = params["id"];
    // TODO: 根据ID启用触发器
    resp.success = false;
    resp.error = "Not implemented";
    return resp;
}

RemoteResponse RemoteServer::handleDisableTrigger(const nlohmann::json& params) {
    RemoteResponse resp;

    std::string id = params["id"];
    // TODO: 根据ID禁用触发器
    resp.success = false;
    resp.error = "Not implemented";
    return resp;
}

RemoteResponse RemoteServer::handleListTriggers(const nlohmann::json& params) {
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

RemoteResponse RemoteServer::handleRecordMacro(const nlohmann::json& params) {
    RemoteResponse resp;
    macroRecorder_->start();
    resp.success = true;
    resp.data["message"] = "Recording started";
    return resp;
}

RemoteResponse RemoteServer::handleStopMacroRecording(const nlohmann::json& params) {
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
    serverAddr.sin_port = htons(port_);

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
