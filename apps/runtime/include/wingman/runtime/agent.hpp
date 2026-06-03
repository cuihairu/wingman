#pragma once

#include "wingman/runtime/config.hpp"
#include <memory>
#include <atomic>
#include <string>

namespace wingman::runtime {

// ========== 前向声明 ==========

class RemoteClient;
class StandaloneMode;

// ========== Agent 主类 ==========

class Agent {
    // P-Impl 前向声明
    class Impl;

public:
    Agent();
    ~Agent();

    // 禁止拷贝
    Agent(const Agent&) = delete;
    Agent& operator=(const Agent&) = delete;

    // 初始化
    bool initialize(const std::string& configPath);
    bool initialize(const AgentConfig& config);
    void shutdown();

    // 运行控制
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // 状态查询
    RunMode getMode() const;
    const AgentConfig& getConfig() const;

    // 获取各模式实例（用于高级控制）
    RemoteClient* getRemoteClient();
    StandaloneMode* getStandaloneMode();

private:
    // 初始化各模式
    bool initRemoteClient();
    bool initStandaloneMode();

    // P-Impl
    std::unique_ptr<Impl> impl_;

    // 运行状态
    std::atomic<bool> running_{false};
};

} // namespace wingman::runtime
