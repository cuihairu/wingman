#pragma once

#include <asio.hpp>
#include <string>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include "wingman/server/protocol.hpp"

namespace wingman::server {

using asio::ip::tcp;

// Response callback
using ResponseCallback = std::function<void(const Response&)>;

// TCP Client
class Client : public std::enable_shared_from_this<Client> {
public:
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    explicit Client(asio::io_context& ioContext);
    ~Client();

    // Connect to server
    std::future<bool> connect(const std::string& host, unsigned short port);
    void disconnect();

    // Send request and get response (async callback)
    void send(const Request& request, ResponseCallback callback);

    // Send request and wait for response (sync)
    std::future<Response> sendSync(const Request& request);

    // Check if connected
    bool isConnected() const;

private:
    asio::io_context& ioContext_;
    std::unique_ptr<tcp::socket> socket_;
    bool connected_ = false;
    std::mutex socketMutex_;
    asio::streambuf buffer_;

    void doRead(ResponseCallback callback);
};

} // namespace wingman::server
