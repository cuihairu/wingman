#pragma once

#include "wingman/runtime/rpc/system_handler.hpp"
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

    // 本地 IPC 客户端（GUI）当前是否在线；仅 start() 后有意义
    bool isClientConnected() const { return clientConnected_.load(); }

    // 注入 system.getStatus 的运行模式上下文（Agent 在 start() 前调用）
    void setStatusProviders(rpc::RuntimeStatusProviders providers);

private:
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> running_{false};
    std::atomic<bool> clientConnected_{false};
    std::mutex startMutex_;
    std::condition_variable startCV_;
    bool startFailed_ = false;
};

} // namespace wingman::runtime
