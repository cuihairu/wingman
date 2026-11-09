#pragma once
// Agent 主类（agent.cpp）覆盖率缺口收口。
// 覆盖率基线：agent.cpp 297 行 0%——runtime 的编排核心（initialize 能力分支 /
// start 装配 / applyRemoteConfig 热重建 / handleRemoteCommand 全命令面）此前只被
// 间接编译、无任何测试驱动。本文件三块补齐：
//   * AgentLifecycleTest（全平台）：纯 API——能力派生组件矩阵、配置文件首跑写
//     默认值、applyRemoteConfig 全错误串矩阵（含运行中重建的两条失败腿）、
//     start/stop/shutdown 生命周期
//   * AgentLoopbackTest（POSIX）：Agent::start() 真实装配下的本地 IPC 回环
//     （XDG_RUNTIME_DIR 重定向到用例私有目录，system.*/config.* 全链）与
//     agentcore 协议回环（TcpServer 注册握手 + 远程命令往返 + EventBuffer
//     远程转发过滤）
// 缺陷回归钉（两处，均由本文件用例暴露后根治）：
//   1) Agent::shutdown 原实现不摘除 EventBuffer 远程 sink（lambda 捕获
//      this），Agent 析构后进程级单例仍持有悬空回调，后续 push 即 UB——
//      已在 shutdown 中补 setRemoteSink(nullptr)，
//      ShutdownClearsRemoteEventSink 钉定。
//   2) system.shutdown 远程命令原实现同步 stop()，而命令回调内联运行在
//      RemoteClient 消息处理线程上 → 自我互等（EDEADLK），ack 永远发不出
//      ——已改为先回 ack、stop 移交独立线程，SystemShutdownCommandStopsAgent
//      钉定。
//
// 平台/环境注记（假设）：
//   1) 回环组仅编入 Unix：XDG_RUNTIME_DIR 端点重定向与 UnixSocket 通道为 POSIX
//      原语；Windows NamedPipe 默认端点（"wingman" 全局名）语义无法在 Linux
//      验证，_WIN32 下不编译该组。纯 API 组全平台运行。
//   2) stop_script 成功腿（agent.cpp stopScript==true 分支）要求脚本处于
//      running/paused/starting 态且依赖引擎协作停止——与 rpc_ipc_test 的
//      pause/resume 同属真机观察范畴（todo.md 既有条目），此处只覆盖
//      missing 参数 / 未知 id / StandaloneMode 缺失三条确定性错误腿。
//   3) screenshot.capture 的 dispatcher 空指针防御分支经公共 API 不可达
//      （initRemoteClient 必建 dispatcher，applyRemoteConfig 为整体替换），
//      with-dispatcher 腿由 trigger.* 等价驱动（同一 dispatchViaRpcDispatcher）；
//      dispatchViaRpcDispatcher 的 catch 分支同理由不可达（RpcDispatcher::
//      dispatch 内部捕获 handler 异常，正常路径只返回错误信封）——余量按
//      不可归因/防御分支登记。
//   4) scriptStateToString 的 running/paused/stopped 分支需长驻运行脚本配合
//      协作停止（同注记 2）不造假用例；loaded/error 由确定性用例覆盖。
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <asio.hpp>

#include "wingman/runtime/agent.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/agentcore/remote_client_config.hpp"
#include "wingman/transport/transport.hpp"
#include "wingman/lua/lua_script_engine.hpp"
#include "wingman/ipc/ipc_factory.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

using json = nlohmann::json;
using Agent = wingman::runtime::Agent;
using wingman::runtime::AgentConfig;
using wingman::runtime::EventBuffer;
using wingman::runtime::RunCapability;
using wingman::runtime::RunMode;
using wingman::transport::Message;
using wingman::transport::MessagePtr;
using wingman::transport::MessageType;
using wingman::transport::TcpServerPtr;
using wingman::transport::createTcpServer;
using namespace std::chrono_literals;

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

// 进程内唯一临时路径（配置文件/脚本/XDG 私有目录共用；pid 掺名 + 进程内序号递增）
inline std::string tempPath(const std::string& tag) {
    static std::atomic<int> seq{0};
    static const auto base = fs::temp_directory_path();
    static const auto pid = std::to_string(processId());
    return (base / ("wm-agent-loop-" + tag + "-" + pid + "-" +
                    std::to_string(seq.fetch_add(1)) + ".tmp"))
        .string();
}

// 选择一个空闲端口（占住后立即释放）；对「不可达」用例而言 RST 远快于
// 未路由等待，与 libs/agentcore 测试的 findFreePort 同一模式
inline int findFreePort() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    const auto port = acceptor.local_endpoint().port();
    acceptor.close();
    return static_cast<int>(port);
}

inline std::string writeScriptFile(const std::string& name, const std::string& body) {
    const auto path = fs::path(tempPath(name));
    std::ofstream file(path);
    file << body;
    return path.string();
}

// 纯 API 套件共用的能力配置：一律显式关闭 LocalIpc——默认端点会绑定真实
// XDG/TMP 路径下的 wingman.sock，与被测机器上可能运行的生产 runtime 冲突
inline AgentConfig makeApiConfig(bool remote, bool standalone) {
    AgentConfig config;
    config.enableRemote = remote;
    config.enableLocalIpc = false;
    config.enableStandaloneScript = standalone;
    if (remote) {
        config.remoteClient.serverIp = "127.0.0.1";
        config.remoteClient.serverPort = findFreePort();
        config.remoteClient.reconnectInterval = 1;
        config.remoteClient.maxReconnectInterval = 1;
    }
    return config;
}


