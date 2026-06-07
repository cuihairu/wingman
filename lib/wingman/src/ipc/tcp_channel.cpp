#include "wingman/ipc/tcp_channel.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <chrono>

#ifdef _WIN32
// winsock2.h already included via header
#else
#include <cerrno>
#include <fcntl.h>
#endif

namespace wingman::ipc {

// Winsock initialization (Windows only)
#ifdef _WIN32
static struct WsaInit {
    WsaInit() {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
    ~WsaInit() { WSACleanup(); }
} wsaInit;
#endif

TcpChannel::TcpChannel(bool serverMode, const std::string& host, int port)
    : serverMode_(serverMode)
    , host_(host)
    , port_(port)
    , state_(IpcState::Disconnected)
    , stopping_(false)
    , nextMessageId_(1)
{
}

TcpChannel::~TcpChannel() {
    disconnect();
}

bool TcpChannel::connect(const std::string& endpoint) {
    if (state_ == IpcState::Connected) {
        return true;
    }

    if (!endpoint.empty()) {
        // Parse "host:port" format
        auto colon = endpoint.rfind(':');
        if (colon != std::string::npos) {
            host_ = endpoint.substr(0, colon);
            port_ = std::stoi(endpoint.substr(colon + 1));
        } else {
            host_ = endpoint;
        }
        endpoint_ = endpoint;
    }

    setState(IpcState::Connecting);

    if (serverMode_) {
        return createServer();
    }
    return connectToServer();
}

void TcpChannel::disconnect() {
    if (state_ == IpcState::Disconnected && dataSocket_ == INVALID_SOCKET && listenSocket_ == INVALID_SOCKET) {
        return;
    }

    setState(IpcState::Disconnecting);
    stopping_ = true;

    if (dataSocket_ != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(dataSocket_, SD_BOTH);
#else
        shutdown(dataSocket_, SHUT_RDWR);
#endif
        closesocket(dataSocket_);
        dataSocket_ = INVALID_SOCKET;
    }
    if (listenSocket_ != INVALID_SOCKET) {
#ifdef _WIN32
        shutdown(listenSocket_, SD_BOTH);
#else
        shutdown(listenSocket_, SHUT_RDWR);
#endif
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }

    stopReceiving();

    setState(IpcState::Disconnected);
    stopping_ = false;
}

bool TcpChannel::isConnected() const {
    return state_ == IpcState::Connected;
}

IpcState TcpChannel::getState() const {
    return state_;
}

bool TcpChannel::send(const IpcMessage& message) {
    if (!isConnected()) {
        return false;
    }

    std::string json = serializeMessage(message);
    uint32_t length = static_cast<uint32_t>(json.size());

    std::lock_guard<std::mutex> lock(sendMutex_);
    if (!sendRaw(&length, sizeof(length))) {
        return false;
    }
    if (!sendRaw(json.data(), json.size())) {
        return false;
    }
    return true;
}

uint64_t TcpChannel::sendRequest(const std::string& method, const std::string& payload) {
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = method;
    msg.payload = payload;
    msg.id = nextMessageId_++;
    msg.timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);

    if (send(msg)) {
        return msg.id;
    }
    return 0;
}

bool TcpChannel::sendEvent(const std::string& method, const std::string& payload) {
    IpcMessage msg;
    msg.type = IpcMessageType::Event;
    msg.method = method;
    msg.payload = payload;
    msg.id = 0;
    msg.timestamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000000);

    return send(msg);
}

void TcpChannel::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    messageCallback_ = std::move(callback);
}

void TcpChannel::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callbacksMutex_);
    errorCallback_ = std::move(callback);
}

void TcpChannel::startReceiving() {
    if (receiveThread_.joinable()) {
        return;
    }
    stopping_ = false;
    receiveThread_ = std::thread([this]() { receiveLoop(); });
}

void TcpChannel::stopReceiving() {
    stopping_ = true;
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

IpcTransport TcpChannel::getTransport() const {
    return IpcTransport::TcpPipe;
}

std::string TcpChannel::getBackendName() const {
    return "TCP";
}

std::string TcpChannel::getEndpoint() const {
    return host_ + ":" + std::to_string(port_);
}

void TcpChannel::setState(IpcState state) {
    state_ = state;
    ErrorCallback callback;
    if (state == IpcState::Error) {
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callback = errorCallback_;
    }
    if (callback) {
        callback("TCP channel error");
    }
}

bool TcpChannel::createServer() {
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        spdlog::error("[TCP] socket() failed");
        setState(IpcState::Error);
        return false;
    }

    // Allow address reuse
    int optval = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port_));

    // Bind to configured host, not INADDR_ANY, for security
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        spdlog::error("[TCP] Invalid address {}: {}", host_, strerror(errno));
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        setState(IpcState::Error);
        return false;
    }

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        spdlog::error("[TCP] bind() failed on {}:{}", host_, port_);
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        setState(IpcState::Error);
        return false;
    }

    if (listen(listenSocket_, 1) == SOCKET_ERROR) {
        spdlog::error("[TCP] listen() failed");
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        setState(IpcState::Error);
        return false;
    }

    spdlog::info("[TCP] Server listening on port {}", port_);

    // Accept one connection
    dataSocket_ = accept(listenSocket_, nullptr, nullptr);
    if (dataSocket_ == INVALID_SOCKET) {
        spdlog::error("[TCP] accept() failed");
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
        setState(IpcState::Error);
        return false;
    }

    // Close listen socket after accepting (single client)
    closesocket(listenSocket_);
    listenSocket_ = INVALID_SOCKET;

    setState(IpcState::Connected);
    spdlog::info("[TCP] Server connected");
    return true;
}

bool TcpChannel::connectToServer() {
    dataSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (dataSocket_ == INVALID_SOCKET) {
        spdlog::error("[TCP] socket() failed");
        setState(IpcState::Error);
        return false;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        // Try localhost if host is empty or invalid
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    // Retry connection for up to 5 seconds
    for (int i = 0; i < 50; ++i) {
        if (::connect(dataSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR) {
            setState(IpcState::Connected);
            spdlog::info("[TCP] Client connected to {}:{}",
                         host_.empty() ? "127.0.0.1" : host_, port_);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::error("[TCP] Failed to connect to {}:{} after retries", host_, port_);
    closesocket(dataSocket_);
    dataSocket_ = INVALID_SOCKET;
    setState(IpcState::Error);
    return false;
}

void TcpChannel::receiveLoop() {
    while (!stopping_ && isConnected()) {
        uint32_t messageLength = 0;
        if (!recvRaw(&messageLength, sizeof(messageLength))) {
            break;
        }

        if (messageLength == 0 || messageLength > 10 * 1024 * 1024) {
            spdlog::error("[TCP] Invalid message length: {}", messageLength);
            break;
        }

        std::vector<uint8_t> buffer(messageLength);
        if (!recvRaw(buffer.data(), messageLength)) {
            break;
        }

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

bool TcpChannel::sendRaw(const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        int sent = ::send(dataSocket_, ptr, static_cast<int>(remaining), 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            spdlog::error("[TCP] send() failed");
            setState(IpcState::Error);
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

bool TcpChannel::recvRaw(void* data, size_t len) {
    char* ptr = static_cast<char*>(data);
    size_t remaining = len;

    while (remaining > 0) {
        int received = recv(dataSocket_, ptr, static_cast<int>(remaining), 0);
        if (received == SOCKET_ERROR || received == 0) {
            if (!stopping_) {
                spdlog::info("[TCP] Connection closed");
            }
            return false;
        }
        ptr += received;
        remaining -= received;
    }
    return true;
}

std::string TcpChannel::serializeMessage(const IpcMessage& msg) {
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

IpcMessage TcpChannel::deserializeMessage(const std::string& json) {
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
