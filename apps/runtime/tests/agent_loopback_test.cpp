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
// 缺陷回归钉：Agent::shutdown 原实现不摘除 EventBuffer 远程 sink（lambda 捕获
// this），Agent 析构后进程级单例仍持有悬空回调，后续 push 即 UB——已在
// shutdown 中补 setRemoteSink(nullptr)，本文件 ShutdownClearsRemoteEventSink 钉定。
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

namespace {

// 进程内唯一临时路径（配置文件/脚本/XDG 私有目录共用；同一二进制串行运行）
std::string tempPath(const std::string& tag) {
    static std::atomic<int> seq{0};
    static const auto base = fs::temp_directory_path();
    return (base / ("wm-agent-loop-" + tag + "-" +
                    std::to_string(seq.fetch_add(1)) + ".tmp"))
        .string();
}

// 选择一个空闲端口（占住后立即释放）；对「不可达」用例而言 RST 远快于
// 未路由等待，与 libs/agentcore 测试的 findFreePort 同一模式
int findFreePort() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    const auto port = acceptor.local_endpoint().port();
    acceptor.close();
    return static_cast<int>(port);
}

std::string writeScriptFile(const std::string& name, const std::string& body) {
    const auto path = fs::path(tempPath(name));
    std::ofstream file(path);
    file << body;
    return path.string();
}

// 纯 API 套件共用的能力配置：一律显式关闭 LocalIpc——默认端点会绑定真实
// XDG/TMP 路径下的 wingman.sock，与被测机器上可能运行的生产 runtime 冲突
AgentConfig makeApiConfig(bool remote, bool standalone) {
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

} // namespace

// ========== 生命周期 / applyRemoteConfig（全平台，无 socket 依赖） ==========

class AgentLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
    }

    void TearDown() override {
        // Agent 析构会走 shutdown（含摘除远程 sink）；此处兜底防用例内
        // 自建 Agent 泄漏影响后续用例
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
        std::error_code ec;
        for (const auto& path : files_) {
            fs::remove(path, ec);
        }
    }

    fs::path tempFile(const std::string& tag) {
        const auto path = fs::path(tempPath(tag));
        files_.push_back(path);
        return path;
    }

    std::vector<fs::path> files_;
};

TEST_F(AgentLifecycleTest, InitializeNoCapabilitiesYieldsUnknownModeAndNullComponents) {
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(false, false)));
    EXPECT_EQ(agent.getMode(), RunMode::Unknown);
    EXPECT_EQ(agent.getRemoteClient(), nullptr);
    EXPECT_EQ(agent.getStandaloneMode(), nullptr);
    EXPECT_FALSE(agent.isRunning());
    const auto& config = agent.getConfig();
    EXPECT_FALSE(config.enableRemote);
    EXPECT_FALSE(config.enableLocalIpc);
    EXPECT_FALSE(config.enableStandaloneScript);
}

TEST_F(AgentLifecycleTest, InitializeRemoteOnlyCreatesRemoteClientOnly) {
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(true, false)));
    EXPECT_EQ(agent.getMode(), RunMode::Remote);
    EXPECT_NE(agent.getRemoteClient(), nullptr);
    EXPECT_EQ(agent.getStandaloneMode(), nullptr);
}

TEST_F(AgentLifecycleTest, InitializeLocalIpcOnlyCreatesStandaloneOnly) {
    // LocalIpc 需要 StandaloneMode 实例承载 RPC handler（agent.cpp:163 注释），
    // 但能力组合仅 LocalIpc 时遗留 RunMode 没有对应值 → Unknown
    Agent agent;
    AgentConfig config = makeApiConfig(false, false);
    config.enableLocalIpc = true;
    // 套件内不 start()，仅验证 initialize 的组件派生（端点绑定归回环组）
    ASSERT_TRUE(agent.initialize(config));
    EXPECT_EQ(agent.getMode(), RunMode::Unknown);
    EXPECT_EQ(agent.getRemoteClient(), nullptr);
    EXPECT_NE(agent.getStandaloneMode(), nullptr);
}

TEST_F(AgentLifecycleTest, InitializeStandaloneOnlyYieldsStandaloneMode) {
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(false, true)));
    EXPECT_EQ(agent.getMode(), RunMode::Standalone);
    EXPECT_EQ(agent.getRemoteClient(), nullptr);
    EXPECT_NE(agent.getStandaloneMode(), nullptr);
}

TEST_F(AgentLifecycleTest, InitializeFromMissingPathWritesDefaultConfigFile) {
    // 首跑路径：文件不存在 → 默认配置生效并落盘，下次启动可读回
    Agent agent;
    const auto path = tempFile("first-run");
    ASSERT_TRUE(agent.initialize(path.string()));
    EXPECT_TRUE(fs::exists(path));

    const auto reloaded = AgentConfig::loadFromFile(path.string());
    const AgentConfig defaults;
    EXPECT_EQ(reloaded.enableRemote, defaults.enableRemote);
    EXPECT_EQ(reloaded.enableLocalIpc, defaults.enableLocalIpc);
    EXPECT_EQ(reloaded.remoteClient.serverPort, defaults.remoteClient.serverPort);
    EXPECT_EQ(reloaded.getRunMode(), defaults.getRunMode());
}

TEST_F(AgentLifecycleTest, InitializeFromExistingPathLoadsFileConfig) {
    AgentConfig seed = makeApiConfig(false, true);
    seed.enableLocalIpc = true;
    seed.remoteClient.serverIp = "10.2.3.4";
    seed.remoteClient.serverPort = 9527;
    const auto path = tempFile("seeded");
    ASSERT_TRUE(seed.saveToFile(path.string()));

    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string()));
    EXPECT_EQ(agent.getConfig().enableRemote, false);
    EXPECT_EQ(agent.getConfig().enableStandaloneScript, true);
    EXPECT_EQ(agent.getConfig().remoteClient.serverIp, "10.2.3.4");
    EXPECT_EQ(agent.getConfig().remoteClient.serverPort, 9527);
    EXPECT_EQ(agent.getMode(), RunMode::Standalone);
}

TEST_F(AgentLifecycleTest, ShutdownWithoutInitializeIsSafe) {
    Agent agent;
    agent.shutdown();
    agent.shutdown(); // 幂等
    EXPECT_FALSE(agent.isRunning());
}

TEST_F(AgentLifecycleTest, StartStopWithoutComponentsSucceeds) {
    // 无能力 agent：start 无组件可启仍返回 true，running 置位/复位
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(false, false)));
    EXPECT_TRUE(agent.start());
    EXPECT_TRUE(agent.isRunning());
    agent.stop();
    EXPECT_FALSE(agent.isRunning());
}

TEST_F(AgentLifecycleTest, StartWithStandaloneCapabilityRunsStandalone) {
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(false, true)));
    EXPECT_TRUE(agent.start());
    EXPECT_TRUE(agent.isRunning());
    ASSERT_NE(agent.getStandaloneMode(), nullptr);
    EXPECT_TRUE(agent.getStandaloneMode()->isRunning());
    agent.stop();
    EXPECT_FALSE(agent.getStandaloneMode()->isRunning());
    EXPECT_FALSE(agent.isRunning());
}

TEST_F(AgentLifecycleTest, StartWithUnreachableRemoteFailsStartButKeepsRunningContract) {
    // 契约钉：start() 先置 running 再启组件；远程启动失败只影响返回值，
    // agent 仍处于 running 态（GUI 可据此显示链路降级），stop 可正常回收
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(true, false)));
    EXPECT_FALSE(agent.start());
    EXPECT_TRUE(agent.isRunning());
    EXPECT_FALSE(agent.getRemoteClient()->isConnected());
    agent.stop();
    EXPECT_FALSE(agent.isRunning());
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigWithoutRemoteFails) {
    Agent agent; // 未 initialize：无 remoteClient
    AgentConfig seed = makeApiConfig(false, false);
    EXPECT_EQ(agent.applyRemoteConfig(seed.remoteClient),
        "远程出站未启用，无法应用远程配置");

    ASSERT_TRUE(agent.initialize(seed)); // LocalIpc-only 同样没有 remoteClient
    EXPECT_EQ(agent.applyRemoteConfig(seed.remoteClient),
        "远程出站未启用，无法应用远程配置");
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigPersistsViaConfigPath) {
    const auto path = tempFile("apply-file");
    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string())); // 默认配置含 RemoteOutbound
    ASSERT_TRUE(agent.getConfig().enableRemote);

    AgentConfig next;
    next.enableRemote = true;
    next.enableLocalIpc = false;
    next.remoteClient.serverIp = "10.9.9.9";
    next.remoteClient.serverPort = 4001;
    next.remoteClient.registerToken = "tok-1";
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient), "");

    EXPECT_EQ(agent.getConfig().remoteClient.serverIp, "10.9.9.9");
    EXPECT_EQ(agent.getConfig().remoteClient.serverPort, 4001);
    EXPECT_EQ(agent.getConfig().remoteClient.registerToken, "tok-1");

    // 落盘持久化：重启后读回
    const auto reloaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(reloaded.remoteClient.serverIp, "10.9.9.9");
    EXPECT_EQ(reloaded.remoteClient.serverPort, 4001);
    EXPECT_EQ(reloaded.remoteClient.registerToken, "tok-1");
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigWithoutConfigPathStillApplies) {
    // 经 initialize(AgentConfig) 构造（configPath 为空）：只改内存不落盘
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(true, false)));

    AgentConfig next;
    next.remoteClient.serverIp = "10.5.5.5";
    next.remoteClient.serverPort = 4002;
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient), "");
    EXPECT_EQ(agent.getConfig().remoteClient.serverIp, "10.5.5.5");
    EXPECT_EQ(agent.getConfig().remoteClient.serverPort, 4002);
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigReportsPersistenceFailure) {
    Agent agent;
    const auto dir = tempFile("dir"); // 不存在目录下的子路径
    const auto path = dir / "nested" / "config.toml";
    ASSERT_TRUE(agent.initialize(path.string()));

    AgentConfig next;
    next.remoteClient.serverIp = "10.6.6.6";
    next.remoteClient.serverPort = 4003;
    // 配置已应用到内存，但写回失败 → 明确的部分成功错误串
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient),
        "配置已应用（本次运行生效），但写入配置文件失败");
    EXPECT_EQ(agent.getConfig().remoteClient.serverIp, "10.6.6.6");
    EXPECT_EQ(agent.getConfig().remoteClient.serverPort, 4003);
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigWhileRunningReportsDeferredReconnect) {
    // 运行中换配置 + 新地址不可达（常态）：配置照常落盘，重连交给重启后重试
    const auto path = tempFile("apply-running");
    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string()));
    EXPECT_FALSE(agent.start()); // 初始地址即不可达，但 running 契约保持
    ASSERT_TRUE(agent.isRunning());

    AgentConfig next;
    next.remoteClient.serverIp = "10.7.7.7";
    next.remoteClient.serverPort = findFreePort();
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient),
        "远程客户端已按新配置写入，但连接启动失败（将在重启后重试）");
    EXPECT_EQ(agent.getConfig().remoteClient.serverIp, "10.7.7.7");

    // 尽管连接未启动，配置已持久化
    const auto reloaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(reloaded.remoteClient.serverIp, "10.7.7.7");
    agent.stop();
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigWhileRunningReportsReconnectAndPersistFailure) {
    // 双重失败：新地址不可达 + 配置文件路径不可写 → 最严重的错误串
    const auto dir = tempFile("dir2");
    const auto path = dir / "nested" / "config.toml";
    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string()));
    EXPECT_FALSE(agent.start());
    ASSERT_TRUE(agent.isRunning());

    AgentConfig next;
    next.remoteClient.serverIp = "10.8.8.8";
    next.remoteClient.serverPort = findFreePort();
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient),
        "远程客户端连接启动失败，且写入配置文件失败");
    EXPECT_EQ(agent.getConfig().remoteClient.serverIp, "10.8.8.8"); // 内存仍生效
    agent.stop();
}

TEST_F(AgentLifecycleTest, ShutdownStopsComponentsAndClearsState) {
    Agent agent;
    ASSERT_TRUE(agent.initialize(makeApiConfig(false, true)));
    EXPECT_TRUE(agent.start());
    ASSERT_NE(agent.getStandaloneMode(), nullptr);

    agent.shutdown();
    EXPECT_FALSE(agent.isRunning());
    EXPECT_EQ(agent.getStandaloneMode(), nullptr);
    EXPECT_EQ(agent.getRemoteClient(), nullptr);
    agent.shutdown(); // 幂等
}

// ========== Agent 装配回环（POSIX：本地 IPC + agentcore 协议） ==========

#ifndef _WIN32

namespace {

// Agent 装配的 LocalIpcServer 走默认端点（XDG_RUNTIME_DIR/wingman.sock）；
// 用例把 XDG/TMPDIR 重定向到私有目录后用极简客户端直连，驱动 GUI 请求全链
class LoopbackIpcClient {
public:
    explicit LoopbackIpcClient(const std::string& endpoint) : endpoint_(endpoint) {
        wingman::ipc::IpcConfig config;
        config.serverName = endpoint;
        channel_ = wingman::ipc::IpcFactory::createClient(config);
    }

    bool connect() {
        if (!channel_ || !channel_->connect(endpoint_)) {
            return false;
        }
        channel_->setMessageCallback([this](const wingman::ipc::IpcMessage& message) {
            if (message.type != wingman::ipc::IpcMessageType::Response) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                responses_.push_back(message);
            }
            cv_.notify_all();
        });
        channel_->startReceiving();
        return true;
    }

    uint64_t call(const std::string& method, const std::string& payload = "{}") {
        return channel_ ? channel_->sendRequest(method, payload) : 0;
    }

    std::optional<wingman::ipc::IpcMessage> awaitResponse(
        uint64_t id, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [&] {
                return std::any_of(responses_.begin(), responses_.end(),
                    [id](const auto& m) { return m.id == id; });
            })) {
            return std::nullopt;
        }
        for (auto it = responses_.begin(); it != responses_.end(); ++it) {
            if (it->id == id) {
                wingman::ipc::IpcMessage matched = *it;
                responses_.erase(it);
                return matched;
            }
        }
        return std::nullopt;
    }

    void disconnect() {
        if (channel_) {
            channel_->disconnect();
        }
    }

private:
    std::string endpoint_;
    std::unique_ptr<wingman::ipc::IIpcChannel> channel_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<wingman::ipc::IpcMessage> responses_;
};

} // namespace

class AgentLoopbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        wingman::lua::registerLuaEngine(); // 幂等：脚本用例需要真实 Lua 引擎
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();

        // XDG/TMPDIR 重定向：Agent::start() 空端点 → IpcFactory::getDefaultEndpoint()
        // 读环境变量 → 每用例私有目录，socket 不撞不残留
        for (const char* name : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
            const char* old = std::getenv(name);
            hadEnv_[name] = old != nullptr;
            savedEnv_[name] = old ? std::string(old) : std::string();
        }
        runtimeDir_ = tempPath("xdg");
        std::filesystem::create_directories(runtimeDir_);
        setenv("XDG_RUNTIME_DIR", runtimeDir_.c_str(), 1);
        // macOS 分支直接拼 TMPDIR + "wingman.sock"，惯例要求尾斜杠
        setenv("TMPDIR", (runtimeDir_.string() + "/").c_str(), 1);

        port_ = findFreePort();

        server_ = createTcpServer();
        server_->setMessageHandler([this](const MessagePtr& msg) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                serverMessages_.push_back(msg);
            }
            cv_.notify_all();
        });
        ASSERT_TRUE(server_->listen("127.0.0.1", port_));
        ASSERT_TRUE(server_->start());
    }

    void TearDown() override {
        ipcClient_.reset();
        if (agent_) {
            agent_->shutdown();
            agent_.reset();
        }
        server_->stop();
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
        for (const char* name : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
            if (hadEnv_[name]) {
                setenv(name, savedEnv_[name].c_str(), 1);
            } else {
                unsetenv(name);
            }
        }
        std::error_code ec;
        std::filesystem::remove_all(runtimeDir_, ec);
    }

    // 能力组合 + 回环 server 地址；remote=false 时不下发不可达端口干扰断言
    AgentConfig loopConfig(bool remote, bool standalone, bool ipc) {
        AgentConfig config;
        config.enableRemote = remote;
        config.enableLocalIpc = ipc;
        config.enableStandaloneScript = standalone;
        config.remoteClient.serverIp = "127.0.0.1";
        config.remoteClient.serverPort = port_;
        config.remoteClient.reconnectInterval = 1;
        config.remoteClient.maxReconnectInterval = 1;
        config.remoteClient.heartbeatInterval = 30;
        config.remoteClient.connectTimeout = 2;
        return config;
    }

    // 初始化 + 启动；remote 用例再走注册握手（server 收到 agent.register
    // 即回注册确认），local-only 用例只验证装配与 IPC 面
    bool startAgent(const AgentConfig& config, bool expectRegister) {
        agent_ = std::make_unique<Agent>();
        if (!agent_->initialize(config)) return false;
        if (!agent_->start()) return false;
        if (!expectRegister) return true;
        if (!waitForServerBodyType("agent.register", 1)) return false;
        ackAllRegisters();
        return waitRemoteState("connected");
    }

    bool waitRemoteState(const std::string& stateName,
                         std::chrono::milliseconds timeout = 3000ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto* client = agent_ ? agent_->getRemoteClient() : nullptr;
            if (client && client->connectionStateName() == stateName) return true;
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }

    bool waitForServerBodyType(const std::string& type, std::size_t count,
                               std::chrono::milliseconds timeout = 3000ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                std::size_t seen = 0;
                for (const auto& msg : serverMessages_) {
                    try {
                        if (json::parse(msg->body).value("type", "") == type) ++seen;
                    } catch (const std::exception&) {}
                }
                if (seen >= count) return true;
            }
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }

    // 对所有未确认的 agent.register 补发注册确认（重建客户端后再次握手同理）
    void ackAllRegisters() {
        std::vector<MessagePtr> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (std::size_t i = ackedRegisters_; i < serverMessages_.size(); ++i) {
                try {
                    if (json::parse(serverMessages_[i]->body).value("type", "") ==
                        "agent.register") {
                        pending.push_back(serverMessages_[i]);
                    }
                } catch (const std::exception&) {}
            }
            ackedRegisters_ = serverMessages_.size();
        }
        const json ack = {
            {"type", "agent.register_ack"},
            {"success", true},
            {"agentId", "srv-loop"},
        };
        for (std::size_t i = 0; i < pending.size(); ++i) {
            server_->send(server_->getSessionIds()[0],
                Message::create(MessageType::Notify, ack.dump()));
        }
    }

    // Agent 装配的 LocalIpcServer 固定落在重定向后的默认端点
    std::string ipcEndpoint() const { return (runtimeDir_ / "wingman.sock").string(); }

    // 向已注册会话下发远程命令并等待同序号 Response，返回响应 body JSON
    json sendCommand(const std::string& type, const json& fields = json::object(),
                     std::chrono::milliseconds timeout = 5000ms) {
        json body = {{"type", type}};
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            body[it.key()] = it.value();
        }
        auto request = Message::create(MessageType::Request, body.dump());
        const auto sequence = nextSequence_++;
        request->header.sequence = sequence;
        if (!server_->send(server_->getSessionIds()[0], request)) {
            return json(); // null
        }
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (const auto& msg : serverMessages_) {
                    if (msg->header.type == MessageType::Response &&
                        msg->header.sequence == sequence) {
                        try {
                            return json::parse(msg->body);
                        } catch (const std::exception&) {
                            return json();
                        }
                    }
                }
            }
            std::this_thread::sleep_for(10ms);
        }
        return json(); // 超时
    }

    // 经本地 IPC 发起 GUI 请求，返回响应信封 {type,id,data}
    std::optional<json> ipc(const std::string& method,
                            const json& params = json::object()) {
        const uint64_t id = ipcClient_->call(method, params.dump());
        EXPECT_NE(id, 0u);
        const auto message = ipcClient_->awaitResponse(id, std::chrono::seconds{5});
        if (!message) {
            return std::nullopt;
        }
        try {
            return json::parse(message->payload);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    int port_ = 0;
    std::unique_ptr<Agent> agent_;
    TcpServerPtr server_;
    std::unique_ptr<LoopbackIpcClient> ipcClient_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<MessagePtr> serverMessages_;
    std::size_t ackedRegisters_ = 0;
    uint64_t nextSequence_ = 100;

    std::filesystem::path runtimeDir_;
    std::map<std::string, std::string> savedEnv_;
    std::map<std::string, bool> hadEnv_;
};

// ----- Agent::start() 装配的本地 IPC 面（GUI 全链） -----

TEST_F(AgentLoopbackTest, LocalIpcServesVersionThroughAgentWiring) {
    ASSERT_TRUE(startAgent(loopConfig(false, true, true), false));
    ipcClient_ = std::make_unique<LoopbackIpcClient>(ipcEndpoint());
    ASSERT_TRUE(ipcClient_->connect());

    const auto response = ipc("system.getVersion");
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->at("type").get<std::string>(), "response");
    ASSERT_TRUE(response->at("data").at("success").get<bool>());
    EXPECT_EQ(response->at("data").at("result").at("server").get<std::string>(),
        "wingman");
}

TEST_F(AgentLoopbackTest, GetStatusProvidersReflectLocalOnlyAgent) {
    // LocalIpc-only agent：start() 注入的 providers 应如实反映「远程禁用、
    // IPC 客户端在线、遗留模式 Unknown」
    ASSERT_TRUE(startAgent(loopConfig(false, true, true), false));
    ipcClient_ = std::make_unique<LoopbackIpcClient>(ipcEndpoint());
    ASSERT_TRUE(ipcClient_->connect());

    const auto response = ipc("system.getStatus");
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>());
    const auto& result = data.at("result");
    EXPECT_EQ(result.at("mode").get<int>(),
        static_cast<int>(RunMode::Unknown));
    EXPECT_FALSE(result.at("remoteConnected").get<bool>());
    EXPECT_EQ(result.at("remoteState").get<std::string>(), "disabled");
    EXPECT_TRUE(result.at("ipcClientConnected").get<bool>());
}

TEST_F(AgentLoopbackTest, ConfigGetRemoteMirrorsAgentConfig) {
    AgentConfig config = loopConfig(false, true, true);
    config.remoteClient.serverIp = "10.0.0.42";
    config.remoteClient.serverPort = 4123;
    config.remoteClient.registerToken = "seed-tok";
    ASSERT_TRUE(startAgent(config, false));
    ipcClient_ = std::make_unique<LoopbackIpcClient>(ipcEndpoint());
    ASSERT_TRUE(ipcClient_->connect());

    const auto response = ipc("config.getRemote");
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("result").at("serverIp").get<std::string>(), "10.0.0.42");
    EXPECT_EQ(data.at("result").at("serverPort").get<int>(), 4123);
    EXPECT_EQ(data.at("result").at("registerToken").get<std::string>(), "seed-tok");
}

TEST_F(AgentLoopbackTest, ConfigSetRemoteValidationMatrixAndNoRemoteApply) {
    // 校验在 start() 注入的 lambda 内完成，先于 applyRemoteConfig：
    // 非法输入逐项拒绝且配置不被改动；合法输入落到「远程出站未启用」
    ASSERT_TRUE(startAgent(loopConfig(false, true, true), false));
    ipcClient_ = std::make_unique<LoopbackIpcClient>(ipcEndpoint());
    ASSERT_TRUE(ipcClient_->connect());

    const std::vector<std::pair<json, std::string>> invalid{
        {json{{"serverIp", 123}}, "serverIp 必须为非空字符串"},
        {json{{"serverIp", ""}}, "serverIp 必须为非空字符串"},
        {json{{"serverPort", "9527"}}, "serverPort 必须为整数"},
        {json{{"serverPort", 0}}, "serverPort 超出范围 (1-65535)"},
        {json{{"serverPort", 70000}}, "serverPort 超出范围 (1-65535)"},
        {json{{"registerToken", 5}}, "registerToken 必须为字符串"},
        {json{{"registerToken", std::string(257, 'x')}}, "registerToken 过长（上限 256 字符）"},
    };
    for (const auto& [params, expected] : invalid) {
        const auto response = ipc("config.setRemote", params);
        ASSERT_TRUE(response.has_value()) << expected;
        EXPECT_FALSE(response->at("data").at("success").get<bool>()) << expected;
        EXPECT_EQ(response->at("data").at("error").get<std::string>(), expected);
    }

    // 校验通过但 agent 无远程出站能力：apply 层拒绝
    const auto valid = ipc("config.setRemote",
        json{{"serverIp", "10.1.1.1"}, {"serverPort", 4950}});
    ASSERT_TRUE(valid.has_value());
    EXPECT_FALSE(valid->at("data").at("success").get<bool>());
    EXPECT_EQ(valid->at("data").at("error").get<std::string>(),
        "远程出站未启用，无法应用远程配置");

    // 拒绝路径不触碰配置
    const auto after = ipc("config.getRemote");
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->at("data").at("result").at("serverIp").get<std::string>(),
        AgentConfig{}.remoteClient.serverIp);
}

TEST_F(AgentLoopbackTest, ConfigSetRemoteAppliesAndPersists) {
    // 真实链路：GUI config.setRemote → start() 注入的校验/apply lambda →
    // Agent::applyRemoteConfig 热重建客户端 → 写回配置文件
    const auto path = fs::path(tempPath("setremote"));
    AgentConfig seed = loopConfig(true, true, true);
    ASSERT_TRUE(seed.saveToFile(path.string()));

    agent_ = std::make_unique<Agent>();
    ASSERT_TRUE(agent_->initialize(path.string()));
    ASSERT_TRUE(agent_->start());
    ASSERT_TRUE(waitForServerBodyType("agent.register", 1));
    ackAllRegisters();
    ASSERT_TRUE(waitRemoteState("connected"));

    ipcClient_ = std::make_unique<LoopbackIpcClient>(ipcEndpoint());
    ASSERT_TRUE(ipcClient_->connect());

    const auto response = ipc("config.setRemote",
        json{{"serverIp", "10.9.8.7"}, {"serverPort", 4950},
             {"registerToken", "tok-9"}});
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>()) << data.dump();
    EXPECT_EQ(data.at("result").at("serverIp").get<std::string>(), "10.9.8.7");
    EXPECT_EQ(data.at("result").at("serverPort").get<int>(), 4950);
    EXPECT_EQ(data.at("result").at("registerToken").get<std::string>(), "tok-9");

    // 内存生效 + 落盘持久化（runtime 重启后保持）
    EXPECT_EQ(agent_->getConfig().remoteClient.serverIp, "10.9.8.7");
    const auto reloaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(reloaded.remoteClient.serverIp, "10.9.8.7");
    EXPECT_EQ(reloaded.remoteClient.serverPort, 4950);
    EXPECT_EQ(reloaded.remoteClient.registerToken, "tok-9");
}

TEST_F(AgentLoopbackTest, ConfigSetRemoteWithUnwritableConfigPathReportsPartialApply) {
    const auto dir = fs::path(tempPath("dir3"));
    const auto path = dir / "nested" / "config.toml";
    agent_ = std::make_unique<Agent>();
    ASSERT_TRUE(agent_->initialize(path.string())); // 默认配置（含远程）
    ASSERT_TRUE(agent_->start());
    ASSERT_TRUE(waitForServerBodyType("agent.register", 1));
    ackAllRegisters();
    ASSERT_TRUE(waitRemoteState("connected"));

    ipcClient_ = std::make_unique<LoopbackIpcClient>(ipcEndpoint());
    ASSERT_TRUE(ipcClient_->connect());

    const auto response = ipc("config.setRemote",
        json{{"serverIp", "10.4.4.4"}, {"serverPort", 4951}});
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    // 新客户端对可达 server 重连成功，但配置文件写回失败 → 部分成功错误串
    EXPECT_FALSE(data.at("success").get<bool>()) << data.dump();
    EXPECT_EQ(data.at("error").get<std::string>(),
        "配置已应用（本次运行生效），但写入配置文件失败");
    EXPECT_EQ(agent_->getConfig().remoteClient.serverIp, "10.4.4.4");
}

TEST_F(AgentLoopbackTest, EventBufferForwardsSelectedEventsToServer) {
    // EventBuffer → 远程 sink 过滤转发：trigger.fired/script.state_changed/
    // script.output 三类上线（agent.event），log.line 留本地
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    EventBuffer::instance().push("trigger.fired", json{{"id", 7}});
    EventBuffer::instance().push("script.state_changed", json{{"id", "s1"}, {"state", "stopped"}});
    EventBuffer::instance().push("script.output", json{{"id", "s1"}, {"line", "hi"}});
    EventBuffer::instance().push("log.line", json{{"text", "local only"}});

    ASSERT_TRUE(waitForServerBodyType("agent.event", 3));

    std::vector<std::pair<std::string, json>> forwarded;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type != MessageType::Notify) continue;
            const auto body = json::parse(msg->body);
            if (body.value("type", "") != "agent.event") continue;
            forwarded.emplace_back(body.at("event").get<std::string>(),
                body.at("data"));
        }
    }
    ASSERT_EQ(forwarded.size(), 3u);
    std::sort(forwarded.begin(), forwarded.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    EXPECT_EQ(forwarded[0].first, "script_output");
    EXPECT_EQ(forwarded[0].second.at("line"), "hi");
    EXPECT_EQ(forwarded[1].first, "script_state");
    EXPECT_EQ(forwarded[1].second.at("state"), "stopped");
    EXPECT_EQ(forwarded[2].first, "trigger_fired");
    EXPECT_EQ(forwarded[2].second.at("id"), 7);
}

TEST_F(AgentLoopbackTest, ShutdownClearsRemoteEventSink) {
    // 缺陷回归钉：shutdown 必须摘除 EventBuffer 远程 sink（lambda 捕获 this）。
    // 回归场景：Agent 析构后进程级单例仍持悬空回调，后续 push 即 UB。
    // 断言：shutdown 后 push 不崩溃也不产生 agent.event。
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));
    agent_->shutdown();
    agent_.reset();

    EventBuffer::instance().push("trigger.fired", json{{"id", 8}});
    std::this_thread::sleep_for(300ms);

    std::size_t events = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            try {
                if (json::parse(msg->body).value("type", "") == "agent.event") ++events;
            } catch (const std::exception&) {}
        }
    }
    EXPECT_EQ(events, 0u);
}

// ----- 远程命令面（Go server → agent 经 agentcore 协议） -----

TEST_F(AgentLoopbackTest, GetStatusReportsScriptsAndLinkState) {
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    // 预置一个已加载（未启动）脚本：get_status 应列出且状态为 loaded
    const auto scriptPath = writeScriptFile("loaded.lua", "return 1\n");
    const auto scriptId = agent_->getStandaloneMode()->loadScript(scriptPath);
    ASSERT_FALSE(scriptId.empty());

    const auto body = sendCommand("get_status");
    ASSERT_TRUE(body.is_object()) << "timed out waiting for get_status response";
    ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();
    const auto& status = body.at("data");
    EXPECT_EQ(status.at("mode").get<int>(),
        static_cast<int>(agent_->getMode()));
    EXPECT_TRUE(status.at("remoteConnected").get<bool>());
    EXPECT_EQ(status.at("remoteState").get<std::string>(), "connected");
    EXPECT_TRUE(status.at("standaloneRunning").get<bool>());
    ASSERT_EQ(status.at("scripts").size(), 1u);
    EXPECT_EQ(status.at("scripts")[0].at("id"), scriptId);
    EXPECT_EQ(status.at("scripts")[0].at("path"), scriptPath);
    EXPECT_EQ(status.at("scripts")[0].at("state"), "loaded");
}

TEST_F(AgentLoopbackTest, GetStatusReportsErroredScript) {
    // 语法错误的脚本：启动受理后执行线程编译失败 → 状态轮询收敛到 error
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));
    const auto badPath = writeScriptFile("bad.lua", "this is not lua )(\n");

    const auto started = sendCommand("run_script", json{{"path", badPath}});
    ASSERT_TRUE(started.is_object()) << "timed out";
    ASSERT_TRUE(started.at("success").get<bool>()) << started.dump();

    for (int i = 0; i < 100; ++i) {
        const auto body = sendCommand("get_status");
        ASSERT_TRUE(body.is_object()) << "timed out";
        ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();
        const auto& scripts = body.at("data").at("scripts");
        if (!scripts.empty() && scripts[0].at("state") == "error") {
            EXPECT_FALSE(scripts[0].at("error").get<std::string>().empty());
            return;
        }
        std::this_thread::sleep_for(50ms);
    }
    FAIL() << "script did not reach error state within timeout";
}

TEST_F(AgentLoopbackTest, ListWindowsReturnsArrayEnvelope) {
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    const auto body = sendCommand("list_windows");
    ASSERT_TRUE(body.is_object()) << "timed out waiting for list_windows response";
    ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();
    ASSERT_TRUE(body.at("data").is_array());
    // 窗口数量随桌面环境不同；逐元素钉信封契约
    for (const auto& window : body.at("data")) {
        EXPECT_TRUE(window.contains("handle"));
        EXPECT_TRUE(window.contains("title"));
        EXPECT_TRUE(window.at("bounds").contains("width"));
    }
}

TEST_F(AgentLoopbackTest, RemoteOnlyAgentCommandLegs) {
    // 无 StandaloneMode 的 agent：run_script / stop_script 两条「不可用」腿
    // + get_status 空 scripts
    ASSERT_TRUE(startAgent(loopConfig(true, false, false), true));

    auto body = sendCommand("run_script", json{{"path", "/tmp/whatever.lua"}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "StandaloneMode not available");

    body = sendCommand("stop_script", json{{"script_id", "s1"}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "StandaloneMode not available");

    body = sendCommand("get_status");
    ASSERT_TRUE(body.is_object()) << "timed out";
    ASSERT_TRUE(body.at("success").get<bool>());
    EXPECT_TRUE(body.at("data").at("scripts").empty());
    EXPECT_FALSE(body.at("data").at("standaloneRunning").get<bool>());
}

TEST_F(AgentLoopbackTest, RunScriptParameterLegs) {
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    // 缺 path 参数
    auto body = sendCommand("run_script");
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "missing path parameter");

    // 空 path 同样拒绝
    body = sendCommand("run_script", json{{"path", ""}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_EQ(body.at("error"), "missing path parameter");

    // 不存在的文件：加载失败
    const auto missing = (fs::path(tempPath("missing")) += ".lua").string();
    body = sendCommand("run_script", json{{"path", missing}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "failed to load script: " + missing);

    // 正常文件：受理并启动
    const auto good = writeScriptFile("good.lua", "return 42\n");
    body = sendCommand("run_script", json{{"path", good}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();
    EXPECT_EQ(body.at("message").get<std::string>().rfind("script started: ", 0), 0);
}

TEST_F(AgentLoopbackTest, StopScriptErrorLegs) {
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    // 缺 script_id
    auto body = sendCommand("stop_script");
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "missing script_id parameter");

    // 未知 id（脚本存在但从未运行 → 协作停止返回失败，错误腿确定性）
    const auto scriptPath = writeScriptFile("idle.lua", "return 9\n");
    const auto scriptId = agent_->getStandaloneMode()->loadScript(scriptPath);
    ASSERT_FALSE(scriptId.empty());
    body = sendCommand("stop_script", json{{"script_id", scriptId}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "failed to stop script: " + scriptId);

    body = sendCommand("stop_script", json{{"script_id", "script_404"}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_EQ(body.at("error"), "failed to stop script: script_404");
}

TEST_F(AgentLoopbackTest, UnknownRemoteCommandFails) {
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    const auto body = sendCommand("no.such.command");
    ASSERT_TRUE(body.is_object()) << "timed out waiting for response";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "unknown command: no.such.command");
}

TEST_F(AgentLoopbackTest, TriggerCommandsReuseRpcDispatcher) {
    // Dispatcher Reuse：远程 trigger.* 与本地 IPC 共用 handler。
    // CommandData 字符串值按 JSON 解析（config 对象串 → 对象参数），
    // 非 JSON 值原样透传（普通字符串名），handler 错误信封回传远端。
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    // config 以 JSON 字符串下发 → dispatchViaRpcDispatcher 解析回对象
    auto body = sendCommand("trigger.add",
        json{{"config", R"({"name":"from-json","interval":250})"}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();
    ASSERT_TRUE(body.at("data").contains("id"));

    // 非 JSON 值回退为字符串参数
    body = sendCommand("trigger.add", json{{"name", "plain-name"}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();

    body = sendCommand("trigger.list");
    ASSERT_TRUE(body.is_object()) << "timed out";
    ASSERT_TRUE(body.at("success").get<bool>());
    const auto& triggers = body.at("data").at("triggers");
    ASSERT_TRUE(triggers.is_array());
    std::vector<std::string> names;
    for (const auto& trigger : triggers) {
        names.push_back(trigger.at("name").get<std::string>());
    }
    EXPECT_NE(std::find(names.begin(), names.end(), "from-json"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "plain-name"), names.end());

    // handler 错误信封 → success=false + error 原样回传（71-73 分支）
    body = sendCommand("trigger.update", json{{"id", "999999"}});
    ASSERT_TRUE(body.is_object()) << "timed out";
    EXPECT_FALSE(body.at("success").get<bool>());
    EXPECT_EQ(body.at("error"), "Trigger not found");
}

TEST_F(AgentLoopbackTest, SystemShutdownCommandStopsAgent) {
    ASSERT_TRUE(startAgent(loopConfig(true, true, false), true));

    const auto body = sendCommand("system.shutdown");
    ASSERT_TRUE(body.is_object()) << "timed out waiting for shutdown response";
    ASSERT_TRUE(body.at("success").get<bool>()) << body.dump();
    EXPECT_EQ(body.at("message"), "agent shutting down");
    EXPECT_FALSE(agent_->isRunning());
    EXPECT_FALSE(agent_->getRemoteClient()->isRunning());
}

#endif // !_WIN32
