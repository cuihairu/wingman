#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
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
    std::mutex startMutex_;
    std::condition_variable startCV_;
    bool startFailed_ = false;
};

} // namespace wingman::runtime
