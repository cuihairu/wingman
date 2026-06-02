#include "wingman/remote_server.hpp"

#if defined(__linux__) || defined(__APPLE__)

#include <spdlog/spdlog.h>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

namespace wingman {

// ========== RemoteControlServer Platform Implementation ==========

class RemoteControlServer::Impl {
public:
    SOCKET listenSocket = INVALID_SOCKET;
    std::thread acceptThread;
    std::vector<std::thread> clientThreads;
    std::mutex clientThreadsMutex;
    bool shouldStop = false;
};

RemoteControlServer::RemoteControlServer()
    : impl_(std::make_unique<Impl>())
    , input_(platform::defaultSharedInput())
    , triggerManager_(std::make_unique<TriggerManager>(input_))
    , macroRecorder_(std::make_unique<MacroRecorder>())
{
}

RemoteControlServer::RemoteControlServer(std::shared_ptr<platform::IInput> input)
    : impl_(std::make_unique<Impl>())
    , input_(std::move(input))
    , triggerManager_(std::make_unique<TriggerManager>(input_))
    , macroRecorder_(std::make_unique<MacroRecorder>())
{
}

RemoteControlServer::~RemoteControlServer() {
    stop();
}

bool RemoteControlServer::start(int port) {
    if (running_) {
        spdlog::warn("RemoteControlServer already running on port {}", port_);
        return true;
    }

    port_ = port;

    impl_->listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->listenSocket == INVALID_SOCKET) {
        spdlog::error("socket creation failed");
        return false;
    }

    int reuseAddr = 1;
    setsockopt(impl_->listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(impl_->listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        spdlog::error("bind failed");
        close(impl_->listenSocket);
        return false;
    }

    if (listen(impl_->listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        spdlog::error("listen failed");
        close(impl_->listenSocket);
        return false;
    }

    running_ = true;
    spdlog::info("RemoteControlServer started on port {}", port_);

    impl_->acceptThread = std::thread([this]() {
        while (running_ && !impl_->shouldStop) {
            sockaddr_in clientAddr{};
            socklen_t clientAddrLen = sizeof(clientAddr);

            SOCKET clientSocket = accept(impl_->listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket == INVALID_SOCKET) {
                if (running_) {
                    spdlog::error("accept failed");
                }
                continue;
            }

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            spdlog::info("Client connected from {}:{}", clientIP, ntohs(clientAddr.sin_port));

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

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

void RemoteControlServer::stop() {
    if (!running_) return;

    running_ = false;
    impl_->shouldStop = true;

    if (impl_->listenSocket != INVALID_SOCKET) {
        close(impl_->listenSocket);
        impl_->listenSocket = INVALID_SOCKET;
    }

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

    spdlog::info("RemoteControlServer stopped");
}

void RemoteControlServer::handleClient(SOCKET clientSocket) {
    char buffer[4096];
    std::string requestBuffer;

    while (running_ && !impl_->shouldStop) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;

        buffer[bytesRead] = '\0';
        requestBuffer.append(buffer);

        try {
            auto j = nlohmann::json::parse(requestBuffer);
            RemoteRequest req = RemoteRequest::fromJson(j);

            RemoteResponse resp = handleRequest(req);

            std::string respStr = resp.toJsonString();
            send(clientSocket, respStr.c_str(), respStr.length(), 0);

            requestBuffer.clear();
        } catch (const nlohmann::json::parse_error&) {
            continue;
        } catch (const std::exception& e) {
            spdlog::error("Error handling request: {}", e.what());
            RemoteResponse resp;
            resp.success = false;
            resp.error = e.what();
            std::string respStr = resp.toJsonString();
            send(clientSocket, respStr.c_str(), respStr.length(), 0);
            requestBuffer.clear();
        }
    }

    close(clientSocket);

    {
        std::lock_guard<std::mutex> lock(impl_->clientThreadsMutex);
        --connectionCount_;
    }
}

// ========== RemoteControlClient Platform Implementation ==========

class RemoteControlClient::Impl {
public:
    SOCKET socket = INVALID_SOCKET;
};

RemoteControlClient::RemoteControlClient()
    : impl_(std::make_unique<Impl>())
{
}

RemoteControlClient::~RemoteControlClient() {
    disconnect();
}

bool RemoteControlClient::connect(const std::string& host, int port) {
    host_ = host;
    port_ = port;

    impl_->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (impl_->socket == INVALID_SOCKET) {
        spdlog::error("socket creation failed");
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0) {
            serverAddr.sin_addr = ((sockaddr_in*)result->ai_addr)->sin_addr;
            freeaddrinfo(result);
        } else {
            spdlog::error("Invalid address: {}", host);
            close(impl_->socket);
            return false;
        }
    }

    if (::connect(impl_->socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        spdlog::error("Connection to {}:{} failed", host, port);
        close(impl_->socket);
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(impl_->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    connected_ = true;
    spdlog::info("Connected to {}:{}", host, port);
    return true;
}

void RemoteControlClient::disconnect() {
    if (!connected_) return;

    if (impl_->socket != INVALID_SOCKET) {
        close(impl_->socket);
        impl_->socket = INVALID_SOCKET;
    }

    connected_ = false;
    spdlog::info("Disconnected from {}:{}", host_, port_);
}

RemoteResponse RemoteControlClient::send(const RemoteRequest& req) {
    RemoteResponse resp;
    if (!connected_) {
        resp.success = false;
        resp.error = "Not connected";
        return resp;
    }

    std::string reqStr = req.toJson().dump();
    if (::send(impl_->socket, reqStr.c_str(), reqStr.length(), 0) == SOCKET_ERROR) {
        resp.success = false;
        resp.error = "Send failed";
        return resp;
    }

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

RemoteResponse RemoteControlClient::send(const std::string& action, const nlohmann::json& params) {
    RemoteRequest req;
    req.action = action;
    req.params = params;
    return send(req);
}

RemoteResponse RemoteControlClient::captureScreen(int x, int y, int width, int height) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    params["width"] = width;
    params["height"] = height;
    return send("capture_screen", params);
}

RemoteResponse RemoteControlClient::getPixel(int x, int y) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    return send("get_pixel", params);
}

RemoteResponse RemoteControlClient::findColor(uint32_t color, int x, int y, int width, int height, int tolerance) {
    nlohmann::json params;

    char colorHex[16];
    snprintf(colorHex, sizeof(colorHex), "0x%06X", color);
    params["color"] = colorHex;

    params["x"] = x;
    params["y"] = y;
    params["width"] = width;
    params["height"] = height;
    params["tolerance"] = tolerance;
    return send("find_color", params);
}

RemoteResponse RemoteControlClient::click(int x, int y, const std::string& button) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    params["button"] = button;
    return send("click", params);
}

RemoteResponse RemoteControlClient::move(int x, int y, int durationMs) {
    nlohmann::json params;
    params["x"] = x;
    params["y"] = y;
    params["duration"] = durationMs;
    return send("move", params);
}

RemoteResponse RemoteControlClient::key(int keyCode) {
    nlohmann::json params;
    params["key"] = keyCode;
    return send("key", params);
}

RemoteResponse RemoteControlClient::typeText(const std::string& text, int delayMs) {
    nlohmann::json params;
    params["text"] = text;
    params["delay"] = delayMs;
    return send("type_text", params);
}

RemoteResponse RemoteControlClient::addTrigger(const nlohmann::json& config) {
    nlohmann::json params;
    params["config"] = config;
    return send("add_trigger", params);
}

RemoteResponse RemoteControlClient::listTriggers() {
    return send("list_triggers");
}

RemoteResponse RemoteControlClient::enableTrigger(const std::string& id) {
    nlohmann::json params;
    params["id"] = id;
    return send("enable_trigger", params);
}

RemoteResponse RemoteControlClient::disableTrigger(const std::string& id) {
    nlohmann::json params;
    params["id"] = id;
    return send("disable_trigger", params);
}

RemoteResponse RemoteControlClient::ping() {
    return send("ping");
}

} // namespace wingman

#endif // __linux__ || __APPLE__
#include "wingman/platform/input_factory.hpp"
