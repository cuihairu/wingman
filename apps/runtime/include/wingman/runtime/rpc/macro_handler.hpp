#pragma once

#include "wingman/rpc/rpc_dispatcher.hpp"
#include "wingman/recorder.hpp"

namespace wingman::rpc {

/// 注册宏录制相关 RPC 处理器。
///
/// 方法：
///   macro.start          开始录制
///   macro.stop           停止录制（返回事件数）
///   macro.play           回放 {speed?, repeat?}
///   macro.status         查询 {recording, paused, eventCount}
///   macro.save           保存为 JSON {path}
///   macro.load           从 JSON 载入 {path}
///   macro.clear          清空录制
void registerMacroHandlers(RpcDispatcher& dispatcher, MacroRecorder& recorder);

} // namespace wingman::rpc
