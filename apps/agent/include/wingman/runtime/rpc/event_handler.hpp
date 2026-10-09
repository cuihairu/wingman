#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

namespace wingman::rpc {

/// 注册事件相关 RPC 处理器（events.drain）。
///
/// GUI 通过轮询 `events.drain` 拉取 runtime 缓冲的日志/触发器/截图事件。
void registerEventHandlers(RpcDispatcher& dispatcher);

} // namespace wingman::rpc
