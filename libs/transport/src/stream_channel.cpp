#include "wingman/transport/stream_channel.hpp"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <sys/types.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace wingman::transport {

// ========== StreamChannel 实现 ==========

class StreamChannel::Impl {
public:
    Impl() = default;
    ~Impl() = default;
};

StreamChannel::StreamChannel(StreamType type, const StreamParams& params)
    : impl_(std::make_unique<Impl>())
    , type_(type)
    , params_(params)
    , socket_(INVALID_SOCKET_VALUE) {
}

StreamChannel::~StreamChannel() {
    disconnect();
}

bool StreamChannel::connect(const std::string& host, int port) {
    if (state_.load() != StreamState::Disconnected) {
        return false;
    }

    setState(StreamState::Connecting);

#ifdef _WIN32
    // 初始化 Winsock（仅第一次）
    static bool winsockInitialized = false;
    if (!winsockInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            winsockInitialized = true;
        }
    }
#endif

    // 创建 Socket
    socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET_VALUE) {
        setState(StreamState::Error);
        return false;
    }

    // 应用 Socket 选项
    if (!applySocketOptions()) {
        disconnect();
        setState(StreamState::Error);
        return false;
    }

    // 连接到服务器
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        // 尝试解析域名
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* result = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) == 0) {
            std::memcpy(&addr.sin_addr, &result->ai_addr->sa_data[2], 4);
            freeaddrinfo(result);
        } else {
            disconnect();
            setState(StreamState::Error);
            return false;
        }
    }

    if (::connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        disconnect();
        setState(StreamState::Error);
        return false;
    }

    // 获取本地端口
    sockaddr_in localAddr{};
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(socket_, reinterpret_cast<sockaddr*>(&localAddr), &addrLen) == 0) {
        localPort_ = ntohs(localAddr.sin_port);
    }

    remoteEndpoint_ = host + ":" + std::to_string(port);
    setState(StreamState::Connected);
    return true;
}

bool StreamChannel::listen(const std::string& host, int port) {
    if (state_.load() != StreamState::Disconnected) {
        return false;
    }

    setState(StreamState::Connecting);

#ifdef _WIN32
    static bool winsockInitialized = false;
    if (!winsockInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            winsockInitialized = true;
        }
    }
#endif

    // 创建 Socket
    socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET_VALUE) {
        setState(StreamState::Error);
        return false;
    }

    // 设置 SO_REUSEADDR
    int optval = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    // 绑定
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (!host.empty() && host != "0.0.0.0") {
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    }

    if (::bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        disconnect();
        setState(StreamState::Error);
        return false;
    }

    // 监听
    if (::listen(socket_, SOMAXCONN) != 0) {
        disconnect();
        setState(StreamState::Error);
        return false;
    }

    localPort_ = port;
    setState(StreamState::Connected);  // 监听状态视为 Connected
    return true;
}

std::unique_ptr<StreamChannel> StreamChannel::accept() {
    if (state_.load() != StreamState::Connected) {
        return nullptr;
    }

    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    Protocol::SocketType clientSocket = ::accept(
        socket_,
        reinterpret_cast<sockaddr*>(&clientAddr),
        &addrLen
    );

    if (clientSocket == INVALID_SOCKET_VALUE) {
        return nullptr;
    }

    // 创建新的流通道
    auto clientChannel = std::make_unique<StreamChannel>(type_, params_);
    clientChannel->socket_ = clientSocket;
    clientChannel->setState(StreamState::Connected);

    // 获取客户端信息
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
    clientChannel->remoteEndpoint_ = std::string(clientIP) + ":" +
                                     std::to_string(ntohs(clientAddr.sin_port));

    return clientChannel;
}

void StreamChannel::disconnect() {
    setState(StreamState::Disconnecting);

    // 停止接收线程
    stopReceiving();

    // 关闭 Socket
    if (socket_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = INVALID_SOCKET_VALUE;
    }

    remoteEndpoint_.clear();
    localPort_ = 0;
    setState(StreamState::Disconnected);
}

bool StreamChannel::send(const uint8_t* data, size_t size) {
    if (!isConnected()) {
        return false;
    }

    auto msg = SimpleMessage::create(std::vector<uint8_t>(data, data + size));
    if (!msg) {
        return false;
    }

    std::error_code ec;
    return Protocol::sendMessage(socket_, *msg, ec);
}

bool StreamChannel::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

bool StreamChannel::send(const std::string& data) {
    return send(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void StreamChannel::startReceiving(StreamDataCallback dataCallback,
                                   StreamErrorCallback errorCallback) {
    if (receiving_.load()) {
        return;  // 已经在接收
    }

    dataCallback_ = std::move(dataCallback);
    errorCallback_ = std::move(errorCallback);

    receiving_.store(true);
    receiveThread_ = std::thread(&StreamChannel::receiveLoop, this);
}

void StreamChannel::stopReceiving() {
    receiving_.store(false);

    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

void StreamChannel::setState(StreamState state) {
    state_.store(state);
}

bool StreamChannel::applySocketOptions() {
    if (socket_ == INVALID_SOCKET_VALUE) {
        return false;
    }

    // TCP_NODELAY
    if (params_.tcpNoDelay) {
        int flag = 1;
        setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&flag), sizeof(flag));
    }

    // SO_KEEPALIVE
    if (params_.keepAlive) {
#ifdef _WIN32
        tcp_keepalive keepalive{};
        keepalive.onoff = 1;
        keepalive.keepalivetime = params_.keepAliveIdle * 1000;
        keepalive.keepaliveinterval = params_.keepAliveInterval * 1000;
        DWORD bytesReturned = 0;
        WSAIoctl(socket_, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive),
                 nullptr, 0, &bytesReturned, nullptr, nullptr);
#else
        int flag = 1;
        setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<const char*>(&flag), sizeof(flag));

        #ifdef TCP_KEEPIDLE
        setsockopt(socket_, IPPROTO_TCP, TCP_KEEPIDLE,
                   reinterpret_cast<const char*>(&params_.keepAliveIdle), sizeof(int));
        #endif
        #ifdef TCP_KEEPINTVL
        setsockopt(socket_, IPPROTO_TCP, TCP_KEEPINTVL,
                   reinterpret_cast<const char*>(&params_.keepAliveInterval), sizeof(int));
        #endif
        #ifdef TCP_KEEPCNT
        setsockopt(socket_, IPPROTO_TCP, TCP_KEEPCNT,
                   reinterpret_cast<const char*>(&params_.keepAliveCount), sizeof(int));
        #endif
#endif
    }

    // SO_SNDBUF
    if (params_.sendBufferSize > 0) {
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<const char*>(&params_.sendBufferSize), sizeof(int));
    }

    // SO_RCVBUF
    if (params_.recvBufferSize > 0) {
        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&params_.recvBufferSize), sizeof(int));
    }

    return true;
}

void StreamChannel::receiveLoop() {
    MessageReceiver receiver;

    while (receiving_.load() && isConnected()) {
        uint8_t buffer[4096];
#ifdef _WIN32
        int n = ::recv(socket_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
        if (n == SOCKET_ERROR_VALUE) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                if (errorCallback_) {
                    errorCallback_(std::error_code(error, std::system_category()));
                }
            }
            break;
        }
        if (n == 0) {
            // 连接关闭
            break;
        }
#else
        ssize_t n = ::recv(socket_, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            if (n < 0 && errorCallback_) {
                errorCallback_(std::error_code(errno, std::system_category()));
            }
            break;
        }
#endif

        // 解析消息
        auto messages = receiver.receive(buffer, n);
        for (const auto& msg : messages) {
            if (dataCallback_) {
                dataCallback_(msg->getPayload().data(), msg->getPayload().size());
            }
        }
    }

    receiving_.store(false);
}

// ========== StreamChannelPair 实现 ==========

StreamChannelPair::StreamChannelPair(const StreamParams& requestParams,
                                     const StreamParams& responseParams)
    : requestChannel_(std::make_unique<StreamChannel>(
          requestParams.type, requestParams))
    , responseChannel_(std::make_unique<StreamChannel>(
          responseParams.type, responseParams)) {
}

bool StreamChannelPair::connect(const std::string& host,
                                int requestPort, int responsePort) {
    if (!requestChannel_->connect(host, requestPort)) {
        return false;
    }

    if (!responseChannel_->connect(host, responsePort)) {
        requestChannel_->disconnect();
        return false;
    }

    return true;
}

void StreamChannelPair::disconnect() {
    if (requestChannel_) {
        requestChannel_->disconnect();
    }
    if (responseChannel_) {
        responseChannel_->disconnect();
    }
}

bool StreamChannelPair::isConnected() const {
    return requestChannel_ && requestChannel_->isConnected() &&
           responseChannel_ && responseChannel_->isConnected();
}

} // namespace wingman::transport
