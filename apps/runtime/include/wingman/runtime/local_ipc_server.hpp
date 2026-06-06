#pragma once

#include <atomic>
#include <memory>
#include <string>

namespace wingman::runtime {

class StandaloneMode;

class LocalIpcServer {
    class Impl;

public:
    explicit LocalIpcServer(StandaloneMode& standalone, std::string endpoint = {});
    ~LocalIpcServer();

    LocalIpcServer(const LocalIpcServer&) = delete;
    LocalIpcServer& operator=(const LocalIpcServer&) = delete;

    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

private:
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
};

} // namespace wingman::runtime
