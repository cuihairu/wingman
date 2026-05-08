#pragma once

#include "wingman/client/config.hpp"
#include <memory>
#include <atomic>
#include <string>
#include <vector>

namespace wingman::client {

// ========== 前向声明 ==========

class ActiveMode;
class PassiveMode;
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
    ActiveMode* getActiveMode();
    PassiveMode* getPassiveMode();
    StandaloneMode* getStandaloneMode();

    // TCP 消息处理（供 PassiveMode 回调使用）
    std::vector<uint8_t> handleMessage(const std::string& sessionId, const std::vector<uint8_t>& data);

private:
    // 初始化各模式
    bool initActiveMode();
    bool initPassiveMode();
    bool initStandaloneMode();

    // P-Impl
    std::unique_ptr<Impl> impl_;

    // 运行状态
    std::atomic<bool> running_{false};
};

} // namespace wingman::client
