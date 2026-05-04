#pragma once

#include <asio.hpp>
#include <memory>
#include <functional>
#include <string>
#include <unordered_map>
#include "wingman/server/protocol.hpp"

namespace wingman::server {

using asio::ip::tcp;

// Request handler callback
using RequestHandler = std::function<Response(const Request&)>;

// Connection class
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using ptr = std::shared_ptr<Connection>;

    explicit Connection(tcp::socket socket, RequestHandler handler);

    void start();
    void send(const Response& response);

private:
    tcp::socket socket_;
    RequestHandler handler_;
    asio::streambuf buffer_;

    void doRead();
    void doWrite(const std::string& data);
};

// TCP Server
class Server {
public:
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Server(asio::io_context& ioContext, unsigned short port);
    ~Server();

    void start();
    void stop();

    // Register request handler
    void setHandler(RequestType type, RequestHandler handler);

    // Broadcast to all clients
    void broadcast(const Response& response);

private:
    asio::io_context& ioContext_;
    tcp::acceptor acceptor_;
    std::unordered_map<RequestType, RequestHandler> handlers_;
    std::vector<Connection::ptr> connections_;
    std::mutex connectionsMutex_;

    void doAccept();
    void removeConnection(Connection::ptr conn);

    Response defaultHandler(const Request& request);
};

} // namespace wingman::server
