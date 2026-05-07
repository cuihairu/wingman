#include "wingman/transport/transport.hpp"
#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <thread>

namespace wingman::transport {

// ========== TcpClient 实现 ==========

class TcpClient::Impl {
public:
    asio::io_context ioContext;
    std::unique_ptr<asio::ip::tcp::socket> socket;
    std::thread ioThread;
    bool connected = false;
    bool reconnectEnabled = false;
    int reconnectIntervalMs = 5000;

    void startIoThread() {
        if (ioThread.joinable()) return;
        ioThread = std::thread([this]() {
            ioContext.run();
        });
    }

    void stopIoThread() {
        ioContext.stop();
        if (ioThread.joinable()) {
            ioThread.join();
        }
    }
};

TcpClient::TcpClient() : impl_(std::make_unique<Impl>()) {}

TcpClient::~TcpClient() {
    disconnect();
}

bool TcpClient::connect(const std::string& host, int port) {
    try {
        impl_->socket = std::make_unique<asio::ip::tcp::socket>(impl_->ioContext);
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host), port);
        asio::error_code ec;
        impl_->socket->connect(endpoint, ec);

        if (ec) {
            spdlog::error("TCP connect failed: {}", ec.message());
            return false;
        }

        impl_->connected = true;
        impl_->startIoThread();

        if (eventCallback_) {
            eventCallback_("connected");
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("TCP connect exception: {}", e.what());
        return false;
    }
}

void TcpClient::disconnect() {
    if (impl_->socket) {
        asio::error_code ec;
        impl_->socket->close(ec);
        impl_->socket.reset();
    }
    impl_->connected = false;
    impl_->stopIoThread();
}

bool TcpClient::isConnected() const {
    return impl_->connected && impl_->socket && impl_->socket->is_open();
}

bool TcpClient::send(const ByteBuffer& data) {
    if (!isConnected()) return false;

    try {
        asio::error_code ec;
        asio::write(*impl_->socket, asio::buffer(data), ec);
        return !ec;
    } catch (const std::exception& e) {
        spdlog::error("TCP send exception: {}", e.what());
        return false;
    }
}

bool TcpClient::send(const std::string& data) {
    return send(ByteBuffer(data.begin(), data.end()));
}

void TcpClient::connectAsync(const std::string& host, int port,
                             std::function<void(bool)> callback) {
    // TODO: 实现异步连接
    callback(connect(host, port));
}

void TcpClient::setReconnect(bool enable, int intervalMs) {
    impl_->reconnectEnabled = enable;
    impl_->reconnectIntervalMs = intervalMs;
}

// ========== TcpServer 实现 ==========

class TcpServer::Impl {
public:
    asio::io_context ioContext;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
    std::thread ioThread;
    bool running = false;
    std::map<std::string, std::shared_ptr<asio::ip::tcp::socket>> clients;
};

TcpServer::TcpServer() : impl_(std::make_unique<Impl>()) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::listen(const std::string& host, int port) {
    try {
        impl_->acceptor = std::make_unique<asio::ip::tcp::acceptor>(
            impl_->ioContext,
            asio::ip::tcp::endpoint(asio::ip::make_address(host), port)
        );

        impl_->running = true;
        impl_->ioThread = std::thread([this]() {
            impl_->ioContext.run();
        });

        // 开始接受连接
        acceptNext();

        spdlog::info("TCP Server listening on {}:{}", host, port);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("TCP listen failed: {}", e.what());
        return false;
    }
}

void TcpServer::stop() {
    impl_->running = false;

    if (impl_->acceptor) {
        asio::error_code ec;
        impl_->acceptor->close(ec);
        impl_->acceptor.reset();
    }

    // 关闭所有客户端
    for (auto& [id, socket] : impl_->clients) {
        asio::error_code ec;
        socket->close(ec);
    }
    impl_->clients.clear();

    impl_->ioContext.stop();
    if (impl_->ioThread.joinable()) {
        impl_->ioThread.join();
    }
}

bool TcpServer::isRunning() const {
    return impl_->running;
}

void TcpServer::acceptNext() {
    if (!impl_->acceptor) return;

    auto socket = std::make_shared<asio::ip::tcp::socket>(impl_->ioContext);
    impl_->acceptor->async_accept(*socket, [this, socket](asio::error_code ec) {
        if (!ec && impl_->running) {
            std::string clientId = socket->remote_endpoint().address().to_string() + ":" +
                                   std::to_string(socket->remote_endpoint().port());
            impl_->clients[clientId] = socket;

            if (clientCallback_) {
                clientCallback_(clientId, true);
            }

            // 继续接受下一个连接
            acceptNext();
        }
    });
}

void TcpServer::broadcast(const ByteBuffer& data) {
    for (auto& [id, socket] : impl_->clients) {
        asio::error_code ec;
        asio::write(*socket, asio::buffer(data), ec);
    }
}

std::vector<std::string> TcpServer::getClients() const {
    std::vector<std::string> result;
    for (const auto& [id, _] : impl_->clients) {
        result.push_back(id);
    }
    return result;
}

} // namespace wingman::transport
