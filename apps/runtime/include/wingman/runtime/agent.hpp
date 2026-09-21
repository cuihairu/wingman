#pragma once

#include "wingman/runtime/config.hpp"
#include "wingman/runtime/remote_client.hpp"
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

    // 应用新的远程配置（config.setRemote 经本地 IPC 调入）：更新内存配置 +
    // 写回配置文件 + 热重建远程客户端（无需重启 runtime）。运行中重建后
    // 自动重连；校验在调用方（config handler 装配层）完成。
    // 返回错误串，空串 = 成功。
    std::string applyRemoteConfig(RemoteClientConfig next);

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

    // 远程命令处理
    CommandResult handleRemoteCommand(const std::string& command, const CommandData& data);

    // P-Impl
    std::unique_ptr<Impl> impl_;

    // 运行状态
    std::atomic<bool> running_{false};
};

} // namespace wingman::runtime
