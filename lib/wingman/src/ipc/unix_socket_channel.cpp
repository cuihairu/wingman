#include "wingman/ipc/unix_socket_channel.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <chrono>

#if defined(__unix__) || defined(__APPLE__)

namespace wingman::ipc {

UnixSocketChannel::UnixSocketChannel(bool serverMode, const std::string& socketPath)
    : serverMode_(serverMode)
    , socketPath_(socketPath)
    , state_(IpcState::Disconnected)
    , stopping_(false)
    , nextMessageId_(1)
{
}

UnixSocketChannel::~UnixSocketChannel() {
    disconnect();
}

bool UnixSocketChannel::connect(const std::string& endpoint) {
    if (state_ == IpcState::Connected) return true;

    if (!endpoint.empty()) {
        socketPath_ = endpoint;
    }

    setState(IpcState::Connecting);

    if (serverMode_) {
        return createServer();
    }
    return connectToServer();
}

void UnixSocketChannel::disconnect() {
    if (state_ == IpcState::Disconnected && dataFd_ == INVALID_SOCKET && listenFd_ == INVALID_SOCKET) return;

    setState(IpcState::Disconnecting);
    stopping_ = true;

    if (dataFd_ != INVALID_SOCKET) {
        shutdown(dataFd_, SHUT_RDWR);
        closesocket(dataFd_);
        dataFd_ = INVALID_SOCKET;
    }
    if (listenFd_ != INVALID_SOCKET) {
        shutdown(listenFd_, SHUT_RDWR);
        closesocket(listenFd_);
        listenFd_ = INVALID_SOCKET;
    }

    stopReceiving();

    // Remove socket file
    if (serverMode_) {
        unlink(socketPath_.c_str());
    }

    setState(IpcState::Disconnected);
    stopping_ = false;
}

bool UnixSocketChannel::isConnected() const {
    return state_ == IpcState::Connected;
}

IpcState UnixSocketChannel::getState() const {
    return state_;
}

bool UnixSocketChannel::send(const IpcMessage& message) {
    if (!isConnected()) return false;

    std::string json = serializeMessage(message);
    uint32_t length = static_cast<uint32_t>(json.size());

    std::lock_guard<std::mutex> lock(sendMutex_);
    if (!sendRaw(&length, sizeof(length))) return false;
    if (!sendRaw(json.data(), json.size())) return false;
    return true;
}

uint64_t UnixSocketChannel::sendRequest(const std::string& method, const std::string& payload) {
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = method;
    msg.payload = payload;
    msg.id = nextMessageId_++;
    msg.timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);

    return send(msg) ? msg.id : 0;
}

bool UnixSocketChannel::sendEvent(const std::string& method, const std::string& payload) {
    IpcMessage msg;
    msg.type = IpcMessageType::Event;
    msg.method = method;
    msg.payload = payload;
    msg.id = 0;
    msg.timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);

    return send(msg);
}

void UnixSocketChannel::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    messageCallback_ = std::move(callback);
}

void UnixSocketChannel::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    errorCallback_ = std::move(callback);
}

void UnixSocketChannel::startReceiving() {
    if (receiveThread_.joinable()) return;
    stopping_ = false;
    receiveThread_ = std::thread([this]() { receiveLoop(); });
}

void UnixSocketChannel::stopReceiving() {
    stopping_ = true;
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

IpcTransport UnixSocketChannel::getTransport() const {
    return IpcTransport::UnixSocket;
}

std::string UnixSocketChannel::getBackendName() const {
    return "UnixSocket";
}

std::string UnixSocketChannel::getEndpoint() const {
    return socketPath_;
}

void UnixSocketChannel::setState(IpcState state) {
    state_ = state;
    ErrorCallback callback;
    if (state == IpcState::Error) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callback = errorCallback_;
    }
    if (callback) {
        callback("UnixSocket channel error: " + socketPath_);
    }
}

bool UnixSocketChannel::createServer() {
    // Remove stale socket file
    unlink(socketPath_.c_str());

    listenFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd_ == INVALID_SOCKET) {
        spdlog::error("[UnixSocket] socket() failed: {}", strerror(errno));
        setState(IpcState::Error);
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        spdlog::error("[UnixSocket] bind() failed on {}: {}", socketPath_, strerror(errno));
        closesocket(listenFd_);
        listenFd_ = INVALID_SOCKET;
        setState(IpcState::Error);
        return false;
    }

    if (listen(listenFd_, 1) == SOCKET_ERROR) {
        spdlog::error("[UnixSocket] listen() failed: {}", strerror(errno));
        closesocket(listenFd_);
        listenFd_ = INVALID_SOCKET;
        unlink(socketPath_.c_str());
        setState(IpcState::Error);
        return false;
    }

    spdlog::info("[UnixSocket] Server listening on {}", socketPath_);

    // Accept one connection
    dataFd_ = accept(listenFd_, nullptr, nullptr);
    if (dataFd_ == INVALID_SOCKET) {
        spdlog::error("[UnixSocket] accept() failed: {}", strerror(errno));
        closesocket(listenFd_);
        listenFd_ = INVALID_SOCKET;
        unlink(socketPath_.c_str());
        setState(IpcState::Error);
        return false;
    }

    // Close listen socket after accepting (single client)
    closesocket(listenFd_);
    listenFd_ = INVALID_SOCKET;

    setState(IpcState::Connected);
    spdlog::info("[UnixSocket] Server connected");
    return true;
}

bool UnixSocketChannel::connectToServer() {
    dataFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dataFd_ == INVALID_SOCKET) {
        spdlog::error("[UnixSocket] socket() failed: {}", strerror(errno));
        setState(IpcState::Error);
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    // Retry connection for up to 5 seconds
    for (int i = 0; i < 50; ++i) {
        if (::connect(dataFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
            setState(IpcState::Connected);
            spdlog::info("[UnixSocket] Client connected to {}", socketPath_);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::error("[UnixSocket] Failed to connect to {} after retries", socketPath_);
    closesocket(dataFd_);
    dataFd_ = INVALID_SOCKET;
    setState(IpcState::Error);
    return false;
}

void UnixSocketChannel::receiveLoop() {
    while (!stopping_ && isConnected()) {
        uint32_t messageLength = 0;
        if (!recvRaw(&messageLength, sizeof(messageLength))) break;

        if (messageLength == 0 || messageLength > 10 * 1024 * 1024) {
            spdlog::error("[UnixSocket] Invalid message length: {}", messageLength);
            break;
        }

        std::vector<uint8_t> buffer(messageLength);
        if (!recvRaw(buffer.data(), messageLength)) break;

        std::string json(buffer.begin(), buffer.end());
        IpcMessage message = deserializeMessage(json);

        MessageCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callback = messageCallback_;
        }
        if (callback) {
            callback(message);
        }
    }

    if (!stopping_) {
        setState(IpcState::Disconnected);
    }
}

bool UnixSocketChannel::sendRaw(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t sent = ::send(dataFd_, ptr, remaining, 0);
        if (sent <= 0) {
            if (!stopping_) spdlog::error("[UnixSocket] send() failed");
            setState(IpcState::Error);
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

bool UnixSocketChannel::recvRaw(void* data, size_t len) {
    char* ptr = static_cast<char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t received = recv(dataFd_, ptr, remaining, 0);
        if (received <= 0) {
            if (!stopping_) spdlog::info("[UnixSocket] Connection closed");
            return false;
        }
        ptr += received;
        remaining -= received;
    }
    return true;
}

std::string UnixSocketChannel::serializeMessage(const IpcMessage& msg) {
    nlohmann::json j;
    j["type"] = static_cast<int>(msg.type);
    j["method"] = msg.method;
    j["id"] = msg.id;
    j["timestamp"] = msg.timestamp;

    if (msg.payload.empty()) {
        j["payload"] = nlohmann::json::object();
    } else {
        try {
            j["payload"] = nlohmann::json::parse(msg.payload);
        } catch (...) {
            j["payload"] = msg.payload;
        }
    }

    return j.dump();
}

IpcMessage UnixSocketChannel::deserializeMessage(const std::string& json) {
    IpcMessage msg;
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(json);
    } catch (const std::exception& e) {
        msg.type = IpcMessageType::Error;
        msg.payload = nlohmann::json{{"error", e.what()}}.dump();
        return msg;
    }

    msg.type = static_cast<IpcMessageType>(parsed.value("type", static_cast<int>(IpcMessageType::Request)));
    msg.method = parsed.value("method", "");
    msg.id = parsed.value("id", uint64_t{0});
    msg.timestamp = parsed.value("timestamp", uint64_t{0});

    if (parsed.contains("payload")) {
        msg.payload = parsed["payload"].is_string()
            ? parsed["payload"].get<std::string>()
            : parsed["payload"].dump();
    }
    return msg;
}

} // namespace wingman::ipc

#endif // __unix__ || __APPLE__
