#include "wingman/server/server.hpp"
#include <spdlog/spdlog.h>
#include <mutex>

namespace wingman::server {

// Connection implementation
Connection::Connection(tcp::socket socket, RequestHandler handler)
    : socket_(std::move(socket)), handler_(std::move(handler)) {}

void Connection::start() {
    doRead();
}

void Connection::send(const Response& response) {
    auto self = shared_from_this();
    std::string data = Protocol::encode(response);
    doWrite(data);
}

void Connection::doRead() {
    auto self = shared_from_this();
    asio::async_read_until(socket_, buffer_, '\n',
        [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::error("Connection read error: {}", ec.message());
                return;
            }

            std::istream is(&buffer_);
            std::string line;
            std::getline(is, line);

            // Parse length
            size_t length = std::stoul(line, nullptr, 16);

            // Read JSON data
            std::string jsonStr(length, '\0');
            asio::read(socket_, asio::buffer(jsonStr, length + 1)); // +1 for newline

            // Parse request
            Request request = Request::fromJson(jsonStr);

            // Handle request
            Response response = handler_(request);
            response.requestId = request.id;

            send(response);
            doRead(); // Continue reading
        });
}

void Connection::doWrite(const std::string& data) {
    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(data),
        [this, self](const std::error_code& ec, std::size_t) {
            if (ec) {
                spdlog::error("Connection write error: {}", ec.message());
            }
        });
}

// Server implementation
Server::Server(asio::io_context& ioContext, unsigned short port)
    : ioContext_(ioContext),
      acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)) {}

Server::~Server() {
    stop();
}

void Server::start() {
    spdlog::info("Server started on port {}", acceptor_.local_endpoint().port());
    doAccept();
}

void Server::stop() {
    std::error_code ec;
    acceptor_.close(ec);
    if (ec) {
        spdlog::error("Server close error: {}", ec.message());
    }
}

void Server::setHandler(RequestType type, RequestHandler handler) {
    handlers_[type] = std::move(handler);
}

void Server::broadcast(const Response& response) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto& conn : connections_) {
        conn->send(response);
    }
}

void Server::doAccept() {
    acceptor_.async_accept(
        [this](const std::error_code& ec, tcp::socket socket) {
            if (!ec) {
                auto conn = std::make_shared<Connection>(
                    std::move(socket),
                    [this](const Request& req) { return defaultHandler(req); }
                );
                {
                    std::lock_guard<std::mutex> lock(connectionsMutex_);
                    connections_.push_back(conn);
                }
                spdlog::info("New connection accepted");
                conn->start();
            } else {
                spdlog::error("Accept error: {}", ec.message());
            }
            doAccept();
        });
}

void Server::removeConnection(Connection::ptr conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(
        std::remove(connections_.begin(), connections_.end(), conn),
        connections_.end()
    );
}

Response Server::defaultHandler(const Request& request) {
    Response response;
    response.requestId = request.id;

    auto it = handlers_.find(request.type);
    if (it != handlers_.end()) {
        return it->second(request);
    }

    response.status = ResponseStatus::kNotFound;
    response.message = "Unknown request type";
    return response;
}

} // namespace wingman::server
