#pragma once
// runtime 本地控制面（GUI ⇄ runtime：local IPC + RPC handler 层）覆盖率缺口收口。
// 覆盖率基线：script_handler 95 行 0%、macro_handler 44 行 0%、config_handler
// 15 行 0%、local_ipc_server 168 行 0%、event_log_sink 18 行 0%——源文件早已
// 编译进 runtime_tests 却无任何测试驱动。本文件补齐：
//   * script.*：真实 StandaloneMode + 真实 Lua 脚本生命周期（含错误信封）
//   * macro.*：speed=0 除零 / 负 speed 无符号下溢挂死两个真实缺陷的回归守卫
//   * config.* / events.drain / system.*（providers 注入与 toggle/pauseAll 分支）
//   * EventLogSink：级别过滤 + 4096 截断
//   * LocalIpcServer：真实 UnixSocket 回环（start→请求/响应→断开→重连→stop）
// 平台/环境注记（假设）：
//   1) LocalIpcServer 回环仅编入 Unix（Linux/macOS）：Windows NamedPipe 通道的
//      端点命名/accept 语义无法在 Linux 上验证，_WIN32 下不编译该组；dispatcher
//      级 handler 测试全平台运行。
//   2) 运行中脚本的 pause/resume 成功路径依赖脚本引擎协作暂停（真机观察范畴，
//      todo.md 既有条目），此处只覆盖确定性错误信封路径。
//   3) macro.start 依赖平台钩子（X11/Win32/Cocoa），录制是否生效随环境不同，
//      只断言信封成功与状态一致性，不断言 recording 必为 true。
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "wingman/lua/lua_script_engine.hpp"

#include "wingman/runtime/standalone_mode.hpp"
#include "wingman/runtime/runtime_context.hpp"
#include "wingman/runtime/event_log_sink.hpp"
#include "wingman/runtime/rpc/system_handler.hpp"
#include "wingman/runtime/rpc/config_handler.hpp"
#include "wingman/runtime/rpc/macro_handler.hpp"
#include "wingman/runtime/rpc/event_handler.hpp"
#include "wingman/runtime/local_ipc_server.hpp"
#include "wingman/rpc/script_handler.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/ipc/ipc_factory.hpp"

#include <spdlog/logger.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

using json = nlohmann::json;
using wingman::runtime::StandaloneMode;
using wingman::runtime::StandaloneModeConfig;
using wingman::rpc::RpcDispatcher;

namespace fs = std::filesystem;

// ctest -j 下每用例独立进程并发：进程内 static 序号跨进程重合，确定性目录名
// 会撞车——先收尾进程的 remove_all 连树拔掉在用目录，后者 bind socket 报
// ENOENT。掺 pid 保证并发进程的临时目录互不相撞。
inline int processId() {
#if defined(_WIN32)
    return _getpid();
#else
    return getpid();
#endif
}

// 进程内唯一临时路径（socket/脚本/宏文件共用；pid 掺名 + 进程内序号递增）
inline std::string tempPath(const std::string& tag) {
    static std::atomic<int> seq{0};
    static const auto base = fs::temp_directory_path();
    static const auto pid = std::to_string(processId());
    return (base / ("wm-rpcipc-" + tag + "-" + pid + "-" +
                    std::to_string(seq.fetch_add(1)) + ".tmp"))
        .string();
}

// 分发一条 RPC 请求并解析响应信封 {type, id, data}
inline json dispatchCall(RpcDispatcher& dispatcher, const std::string& method,
                  const json& params = json::object()) {
    const json request = {
        {"type", "call"}, {"id", "t"}, {"method", method}, {"params", params}};
    return json::parse(dispatcher.dispatch(request.dump()));
}

inline void clearEventBuffer() { wingman::runtime::EventBuffer::instance().clear(); }

inline std::vector<wingman::runtime::IpcEvent> drainEventBuffer() {
    return wingman::runtime::EventBuffer::instance().drain(5000);
}
// 构造一个可被 playback 消费且无副作用的事件宏：type=Delay(8) 走 switch 的
// default 分支不注入输入；timestamp 0/1 相邻使得修复后 speed 钳到 1 时单轮
// 仅 100ms 延迟，而修复前 speed=0 触发除零、负 speed 触发无符号下溢挂死。
inline std::string writeTwoTickMacro(const std::string& path) {
    std::ofstream file(path);
    file << "{\"events\":["
         << "{\"type\":8,\"timestamp\":0,\"delay\":0},"
         << "{\"type\":8,\"timestamp\":1,\"delay\":0}]}";
    return path;
}


