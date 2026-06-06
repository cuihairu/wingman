#pragma once

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

namespace wingman::rpc {

class WsServer {
public:
    WsServer(int port = 8080);
    ~WsServer();

    void start();
    void stop();
    bool isRunning() const;

    using MessageHandler = std::function<std::string(const std::string&)>;
    void onMessage(MessageHandler handler);

private:
    int port_;
    std::atomic<bool> running_{false};
    MessageHandler handler_;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wingman::rpc
