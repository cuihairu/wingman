#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"
#include <functional>
#include <string>

namespace wingman::runtime {
class StandaloneMode;
}

namespace wingman::rpc {

// RuntimeStatusProviders 为 system.getStatus 提供运行模式上下文。
// 由 LocalIpcServer 的所有者（Agent）注入，避免 handler 直接依赖 Agent 类型；
// 未注入时各字段回退为"禁用/未知"，保持向后兼容。
struct RuntimeStatusProviders {
    std::function<bool()> remoteConnected;
    std::function<std::string()> remoteStateName;
    std::function<bool()> ipcClientConnected;
    std::function<int()> runMode;
};

void registerRuntimeSystemHandlers(RpcDispatcher& dispatcher,
                                   const std::string& version,
                                   runtime::StandaloneMode& standalone,
                                   const RuntimeStatusProviders& providers = {});

} // namespace wingman::rpc
