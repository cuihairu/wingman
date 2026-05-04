#include "wingman/server/client.hpp"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace wingman::server {

Client::Client(asio::io_context& ioContext)
    : ioContext_(ioContext),
      socket_(std::make_unique<tcp::socket>(ioContext)) {}

Client::~Client() {
    disconnect();
}

std::future<bool> Client::connect(const std::string& host, unsigned short port) {
    auto promise = std::make_shared<std::promise<bool>>();

    tcp::resolver resolver(ioContext_);
    auto endpoints = resolver.resolve(host, std::to_string(port));

    asio::async_connect(*socket_, endpoints,
        [this, promise](const std::error_code& ec, const tcp::endpoint&) {
            if (!ec) {
                connected_ = true;
                spdlog::info("Connected to server");
                promise->set_value(true);
            } else {
                spdlog::error("Connection failed: {}", ec.message());
                promise->set_value(false);
            }
        });

    return promise->get_future();
}

void Client::disconnect() {
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (socket_ && socket_->is_open()) {
        std::error_code ec;
        socket_->close(ec);
        connected_ = false;
    }
}

void Client::send(const Request& request, ResponseCallback callback) {
    if (!isConnected()) {
        Response response;
        response.status = ResponseStatus::kError;
        response.message = "Not connected";
        callback(response);
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
                Response response;
                response.status = ResponseStatus::kError;
                response.message = ec.message();
                callback(response);
                return;
            }
            doRead(callback);
        });
}

std::future<Response> Client::sendSync(const Request& request) {
    auto promise = std::make_shared<std::promise<Response>>();

    send(request, [promise](const Response& response) {
        promise->set_value(response);
    });

    return promise->get_future();
}

bool Client:: isConnected() const {
    return connected_ && socket_ && socket_->is_open();
}

void Client::doRead(ResponseCallback callback) {
    asio::async_read_until(*socket_, buffer_, '\n',
        [this, callback](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::error("Read error: {}", ec.message());
                Response response;
                response.status = ResponseStatus::kError;
                response.message = ec.message();
                callback(response);
                return;
            }

            std::istream is(&buffer_);
            std::string line;
            std::getline(is, line);

            size_t length = std::stoul(line, nullptr, 16);

            std::string jsonStr(length, '\0');
            asio::read(*socket_, asio::buffer(jsonStr, length + 1));

            Response response = Response::fromJson(jsonStr);
            callback(response);
        });
}

} // namespace wingman::server
