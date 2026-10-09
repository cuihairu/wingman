#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

#include <nlohmann/json.hpp>
#include <functional>

namespace wingman::rpc {

// RemoteConfigAccess 为 config.getRemote / config.setRemote 提供读写通道。
// 由 LocalIpcServer 的所有者（Agent）注入，handler 不依赖 Agent 类型：
// - get   返回当前生效的远程配置（serverIp/serverPort/registerToken）
// - apply 应用新值（更新内存 + 写配置文件 + 热重建远程客户端），
//         返回错误串（空串 = 成功）
struct RemoteConfigAccess {
    std::function<nlohmann::json()> get;
    std::function<std::string(const nlohmann::json&)> apply;
};

// 注册 config.getRemote / config.setRemote（本地 IPC only，见架构文档）。
// GUI 通过 local IPC 读写 runtime 的远程注册配置（含 A3-P1 的注册令牌），
// 不存在对应的远程 agent 命令——远程配置不允许从 Go server 侧改写。
void registerRuntimeConfigHandlers(RpcDispatcher& dispatcher,
                                   const RemoteConfigAccess& access);

} // namespace wingman::rpc
