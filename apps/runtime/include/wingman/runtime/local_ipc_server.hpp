#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace wingman {
class TriggerManager;
}

namespace wingman::runtime {

class StandaloneMode;

class LocalIpcServer {
    class Impl;

public:
    // sharedTriggerManager 非空时复用外部触发器管理器（如 Agent 持有的实例，
    // 使本地 IPC 与远程 agent 通道看到同一份触发器），为空时自建并独占生命周期。
    explicit LocalIpcServer(StandaloneMode& standalone, std::string endpoint = {},
        TriggerManager* sharedTriggerManager = nullptr);
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
