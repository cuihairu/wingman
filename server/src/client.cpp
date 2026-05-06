#include "wingman/server/client.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace wingman::server {

Client::Client(asio::io_context& ioContext)
    : ioContext_(ioContext),
      socket_(std::make_unique<tcp::socket>(ioContext)),
      reconnectTimer_(ioContext),
      heartbeatTimer_(ioContext) {}

Client::~Client() {
    disconnect();
}

// ========== 连接管理 ==========

std::future<bool> Client::connect(const std::string& host, unsigned short port) {
    host_ = host;
    port_ = port;

    auto promise = std::make_shared<std::promise<bool>>();

    tcp::resolver resolver(ioContext_);
    auto endpoints = resolver.resolve(host, std::to_string(port));

    asio::async_connect(*socket_, endpoints,
        [this, promise](const std::error_code& ec, const tcp::endpoint&) {
            if (!ec) {
                connected_ = true;
                spdlog::info("Connected to {}:{}", host_, port_);
                notifyStateChange(true);

                // 启动读取
                doRead();

                // 发送注册消息（如果有 agentId）
                if (!agentId_.empty()) {
                    Request req;
                    req.type = RequestType::kRegister;
                    req.id = Request::generateId();
                    req.timestamp = Protocol::now();
                    req.agentId = agentId_;
                    req.data = {
                        {"agent_id", agentId_},
                        {"hostname", asio::ip::host_name()}
                    };

                    send(req, [](const Response& resp) {
                        if (resp.code != ErrorCode::OK) {
                            spdlog::error("Registration failed: {}", resp.message);
                        }
                    });
                }

                promise->set_value(true);
            } else {
                spdlog::error("Connection failed: {}", ec.message());
                promise->set_value(false);

                // 自动重连
                if (autoReconnect_) {
                    scheduleReconnect();
                }
            }
        });

    return promise->get_future();
}

void Client::disconnect() {
    stopHeartbeat();

    std::lock_guard<std::mutex> lock(socketMutex_);
    if (socket_ && socket_->is_open()) {
        std::error_code ec;
        socket_->close(ec);
        connected_ = false;
        notifyStateChange(false);
    }
}

bool Client::isConnected() const {
    return connected_ && socket_ && socket_->is_open();
}

// ========== 配置 ==========

void Client::enableAutoReconnect(bool enable, std::chrono::seconds interval) {
    autoReconnect_ = enable;
    reconnectInterval_ = interval;
}

// ========== 消息发送 ==========

void Client::send(const Request& request, MessageCallback callback) {
    if (!isConnected()) {
        Response response;
        response.code = ErrorCode::DISCONNECTED;
        response.message = "Not connected";
        if (callback) callback(response);
        return;
    }

    std::string json = request.toJson();
    std::ostringstream oss;
    oss << std::hex << json.length() << "\n" << json << "\n";
    std::string data = oss.str();

    asio::async_write(*socket_, asio::buffer(data),
        [this, callback](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::error("Send error: {}", ec.message());
                if (callback) {
                    Response response;
                    response.code = ErrorCode::DISCONNECTED;
                    response.message = ec.message();
                    callback(response);
                }

                // 自动重连
                if (autoReconnect_) {
                    scheduleReconnect();
                }
                return;
            }
            // 继续读取响应
        });
}

std::future<Response> Client::sendSync(const Request& request) {
    auto promise = std::make_shared<std::promise<Response>>();

    send(request, [promise](const Response& response) {
        promise->set_value(response);
    });

    return promise->get_future();
}

// ========== 心跳 ==========

void Client::startHeartbeat(std::chrono::seconds interval) {
    heartbeatInterval_ = interval;
    heartbeatRunning_ = true;
    scheduleHeartbeat();
}

void Client::stopHeartbeat() {
    heartbeatRunning_ = false;
    std::error_code ec;
    heartbeatTimer_.cancel(ec);
}

// ========== 私有方法 ==========

void Client::doRead() {
    asio::async_read_until(*socket_, buffer_, '\n',
        [this](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::error("Read error: {}", ec.message());
                connected_ = false;
                notifyStateChange(false);

                if (autoReconnect_) {
                    scheduleReconnect();
                }
                return;
            }

            std::istream is(&buffer_);
            std::string line;
            std::getline(is, line);

            size_t length = 0;
            try {
                length = std::stoul(line, nullptr, 16);
            } catch (...) {
                spdlog::error("Invalid message length");
                doRead();
                return;
            }

            std::string jsonStr(length, '\0');
            std::error_code read_ec;
            std::size_t n = asio::read(*socket_, asio::buffer(jsonStr, length + 1), read_ec);

            if (read_ec || n != length + 1) {
                spdlog::error("Read data error");
                doRead();
                return;
            }

            Response response = Response::fromJson(jsonStr);

            // 触发回调
            if (messageCallback_) {
                messageCallback_(response);
            }

            // 继续读取
            doRead();
        });
}

void Client::notifyStateChange(bool connected) {
    if (stateCallback_) {
        stateCallback_(connected);
    }
}

void Client::scheduleReconnect() {
    if (!autoReconnect_ || connected_) {
        return;
    }

    reconnectTimer_.expires_after(reconnectInterval_);
    reconnectTimer_.async_wait([this](const std::error_code& ec) {
        if (ec) {
            return;  // Timer 被取消
        }
        doAutoReconnect();
    });
}

void Client::doAutoReconnect() {
    if (connected_) {
        return;
    }

    spdlog::info("Attempting to reconnect to {}:{}", host_, port_);

    // 关闭旧 socket
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        if (socket_) {
            std::error_code ec;
            socket_->close(ec);
        }
        socket_ = std::make_unique<tcp::socket>(ioContext_);
    }

    connect(host_, port_);
}

void Client::scheduleHeartbeat() {
    if (!heartbeatRunning_ || !connected_) {
        return;
    }

    heartbeatTimer_.expires_after(heartbeatInterval_);
    heartbeatTimer_.async_wait([this](const std::error_code& ec) {
        if (ec) {
            return;  // Timer 被取消
        }
        doHeartbeat();
    });
}

void Client::doHeartbeat() {
    if (!connected_) {
        return;
    }

    Request req;
    req.type = RequestType::kHeartbeat;
    req.id = Request::generateId();
    req.timestamp = Protocol::now();
    req.agentId = agentId_;
    req.data = heartbeatData_;

    send(req, [this](const Response& response) {
        if (response.code != ErrorCode::OK) {
            spdlog::warn("Heartbeat failed: {}", response.message);
        }
    });

    // 调度下一次心跳
    scheduleHeartbeat();
}

} // namespace wingman::server
