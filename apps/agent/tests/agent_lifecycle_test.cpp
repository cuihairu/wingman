#include "agent_loopback_test_common.hpp"

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
    // 预写关闭 LocalIpc 的种子配置：initialize(path) 缺文件时会落默认配置
    // （默认含 LocalIpc），后续 start() 会绑定真实 XDG 生产端点
    const AgentConfig seed = makeApiConfig(true, false);
    ASSERT_TRUE(seed.saveToFile(path.string()));
    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string()));
    ASSERT_TRUE(agent.getConfig().enableRemote);
    ASSERT_FALSE(agent.getConfig().enableLocalIpc);

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
    // 运行中换配置 + 新地址不可达（常态）：配置照常落盘，重连交给重启后重试。
    // 不可达一律用 127.0.0.1 + 释放端口（立即 ECONNREFUSED）；不可路由 IP 的
    // SYN 重试会让同步 connect 长时间挂起（已实测）
    const auto path = tempFile("apply-running");
    const AgentConfig seed = makeApiConfig(true, false);
    ASSERT_TRUE(seed.saveToFile(path.string()));
    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string()));
    EXPECT_FALSE(agent.start()); // 初始地址即不可达，但 running 契约保持
    ASSERT_TRUE(agent.isRunning());

    AgentConfig next;
    next.remoteClient.serverIp = "127.0.0.1";
    const auto newPort = static_cast<int>(findFreePort());
    next.remoteClient.serverPort = newPort;
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient),
        "远程客户端已按新配置写入，但连接启动失败（将在重启后重试）");
    EXPECT_EQ(agent.getConfig().remoteClient.serverPort, newPort);

    // 尽管连接未启动，配置已持久化
    const auto reloaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(reloaded.remoteClient.serverPort, newPort);
    agent.stop();
}

TEST_F(AgentLifecycleTest, ApplyRemoteConfigWhileRunningReportsReconnectAndPersistFailure) {
    // 双重失败：新地址不可达 + 配置文件路径不可写 → 最严重的错误串
    const auto dir = tempFile("dir2");
    const auto path = dir / "nested" / "config.toml";
    Agent agent;
    ASSERT_TRUE(agent.initialize(path.string())); // 默认配置（远程开、LocalIpc 开）
    // LocalIpc 能力下 start() 会绑定默认端点；显式关掉避免碰真实生产 socket
    AgentConfig disabled = agent.getConfig();
    // 默认端点 8888 在部分机器上被外部服务占用——connect 成功会让 start()
    // 走成功分支，摸不到「初始地址不可达」前提。同上例口径：127.0.0.1 +
    // 已释放端口构造注定不可达
    disabled.remoteClient.serverPort = static_cast<int>(findFreePort());
    disabled.enableLocalIpc = false;
    ASSERT_TRUE(agent.initialize(disabled));
    EXPECT_FALSE(agent.start());
    ASSERT_TRUE(agent.isRunning());

    AgentConfig next;
    next.remoteClient.serverIp = "127.0.0.1";
    const auto newPort = static_cast<int>(findFreePort());
    next.remoteClient.serverPort = newPort;
    EXPECT_EQ(agent.applyRemoteConfig(next.remoteClient),
        "远程客户端连接启动失败，且写入配置文件失败");
    EXPECT_EQ(agent.getConfig().remoteClient.serverPort, newPort); // 内存仍生效
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
