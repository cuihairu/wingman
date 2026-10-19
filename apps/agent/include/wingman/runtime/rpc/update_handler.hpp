#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"

namespace wingman {
class UpdatePoller;
}

namespace wingman::rpc {

// 注册 update.status / update.check / update.download（本地 IPC only，
// 见架构决策：GUI 只走本地 IPC 控制 runtime，runtime 只 outbound 连 Go server）。
//
// poller 为空 = 更新功能未启用（[update] 未配置或关闭），三个方法统一返回
// enabled=false 的可读状态，GUI 据此隐藏更新入口而不是报错。
//
// check/download 都是异步触发：真正执行在 poller 的 worker 线程上（HTTP 最坏
// 等传输超时，不能阻塞本地 IPC 的 handler 线程），结果通过 update.status_changed
// 事件回投给 GUI（runtime → EventBuffer → events.drain 拉取）。
void registerRuntimeUpdateHandlers(RpcDispatcher& dispatcher, UpdatePoller* poller);

} // namespace wingman::rpc
