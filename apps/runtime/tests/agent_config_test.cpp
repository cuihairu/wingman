// AgentConfig 单测：能力/模式派生 + TOML 解析节域 + 文件加载/保存往返。
//
// 覆盖率缺口背景（gcovr：agent_config.cpp 45%、78 行未覆盖）：此前仅有
// cli_test.cpp 的一个 loadFromString 用例，getCapabilities/getRunMode/
// loadFromFile/saveToFile 整段为零覆盖，而它们是 agent.cpp 的真实生产路径
// （Agent::initialize 派生 RunMode、applyRemoteConfig 写回配置文件）。
//
// 缺陷回归钉：saveToFile 原实现不写 [performance] 节，而 loadFromString
// 支持该节——用户手调的性能配置会在 runtime 首次配置写回
// （Agent::applyRemoteConfig → saveToFile）时被静默抹掉。
//
// 平台说明：纯 std::filesystem/fstream，Windows CI 全量参与（无 loopback
// 跳过、无真机观察项）。解析器的行为断言一律钉「当前实现的真实契约」，
// 不写理想化用例：未知键/节忽略、类型不匹配忽略、未配对引号不剥。

#include "wingman/runtime/config.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

using wingman::runtime::AgentConfig;
using wingman::runtime::hasCapability;
using wingman::runtime::RunCapability;
using wingman::runtime::RunMode;

class AgentConfigTest : public ::testing::Test {
protected:
    // 每用例独立的临时文件路径（进程内原子序号避免同套件并发撞名；
    // temp_directory_path 在 Windows/macOS/Linux 语义一致）。
    std::filesystem::path tempFile(const std::string& tag) {
        const auto base = std::filesystem::temp_directory_path();
        const auto path = base / ("wm-agent-config-" + tag + "-" +
            std::to_string(++seq_) + ".tmp");
        files_.push_back(path);
        return path;
    }

    void TearDown() override {
        std::error_code ec;
        for (const auto& path : files_) {
            std::filesystem::remove(path, ec);
        }
    }

    int seq_ = 0;
    std::vector<std::filesystem::path> files_;
};

// ========== 能力与模式派生（agent.cpp 的真实分支依据） ==========

TEST_F(AgentConfigTest, DefaultsAreRemotePlusLocalIpcYieldingHybrid) {
    const AgentConfig config;
    EXPECT_TRUE(hasCapability(config.getCapabilities(), RunCapability::RemoteOutbound));
    EXPECT_TRUE(hasCapability(config.getCapabilities(), RunCapability::LocalIpc));
    EXPECT_FALSE(hasCapability(config.getCapabilities(), RunCapability::StandaloneScript));
    EXPECT_EQ(config.getRunMode(), RunMode::Hybrid);
}

TEST_F(AgentConfigTest, RemoteOnlyDerivesRemoteMode) {
    AgentConfig config;
    config.enableLocalIpc = false;
    EXPECT_EQ(config.getRunMode(), RunMode::Remote);
}

TEST_F(AgentConfigTest, StandaloneOnlyDerivesStandaloneMode) {
    AgentConfig config;
    config.enableRemote = false;
    config.enableLocalIpc = false;
    config.enableStandaloneScript = true;
    EXPECT_EQ(config.getCapabilities(), RunCapability::StandaloneScript);
    EXPECT_EQ(config.getRunMode(), RunMode::Standalone);
}

TEST_F(AgentConfigTest, LocalIpcOnlyIsUnknownMode) {
    // 仅 LocalIpc 是 GUI 受控 runtime 的合法能力组合，遗留 RunMode 没有对应值
    AgentConfig config;
    config.enableRemote = false;
    EXPECT_EQ(config.getRunMode(), RunMode::Unknown);
}

TEST_F(AgentConfigTest, NothingEnabledIsUnknownMode) {
    AgentConfig config;
    config.enableRemote = false;
    config.enableLocalIpc = false;
    config.enableStandaloneScript = false;
    EXPECT_EQ(config.getCapabilities(), RunCapability::None);
    EXPECT_EQ(config.getRunMode(), RunMode::Unknown);
}

TEST_F(AgentConfigTest, HybridTakesPrecedenceOverStandalone) {
    // getRunMode 的派生顺序：Hybrid（远端+本地IPC）优先于 Standalone 判定
    AgentConfig config;
    config.enableStandaloneScript = true;
    EXPECT_EQ(config.getRunMode(), RunMode::Hybrid);
}

// ========== loadFromString：节域与类型分支 ==========

TEST_F(AgentConfigTest, EmptyStringYieldsAllDefaults) {
    const auto config = AgentConfig::loadFromString("");
    const AgentConfig defaults;
    EXPECT_EQ(config.enableRemote, defaults.enableRemote);
    EXPECT_EQ(config.enableLocalIpc, defaults.enableLocalIpc);
    EXPECT_EQ(config.enableStandaloneScript, defaults.enableStandaloneScript);
    EXPECT_EQ(config.remoteClient.serverIp, defaults.remoteClient.serverIp);
    EXPECT_EQ(config.remoteClient.serverPort, defaults.remoteClient.serverPort);
    EXPECT_EQ(config.debugger.listenPort, defaults.debugger.listenPort);
    EXPECT_EQ(config.logging.level, defaults.logging.level);
    EXPECT_EQ(config.performance.memoryLimitMb, defaults.performance.memoryLimitMb);
}

TEST_F(AgentConfigTest, GlobalCapabilityKeysParseTrueAndFalse) {
    const auto config = AgentConfig::loadFromString(R"(
        enable_remote = true
        enable_local_ipc = false
        enable_standalone_script = true
    )");
    EXPECT_TRUE(config.enableRemote);
    EXPECT_FALSE(config.enableLocalIpc);
    EXPECT_TRUE(config.enableStandaloneScript);
}

TEST_F(AgentConfigTest, GlobalSectionIsAliasForTopLevel) {
    const auto config = AgentConfig::loadFromString(R"(
        [global]
        enable_remote = false
        enable_local_ipc = false
        enable_standalone_script = true
    )");
    EXPECT_FALSE(config.enableRemote);
    EXPECT_FALSE(config.enableLocalIpc);
    EXPECT_TRUE(config.enableStandaloneScript);
}

TEST_F(AgentConfigTest, RemoteSectionParsesIntsAndStrings) {
    const auto config = AgentConfig::loadFromString(R"(
        [remote]
        server_ip = "10.1.2.3"
        server_port = 9527
        reconnect_interval = 7
        max_reconnect_interval = 120
        heartbeat_interval = 15
        connect_timeout = 3
        register_token = "tok-123"
    )");
    EXPECT_EQ(config.remoteClient.serverIp, "10.1.2.3");
    EXPECT_EQ(config.remoteClient.serverPort, 9527);
    EXPECT_EQ(config.remoteClient.reconnectInterval, 7);
    EXPECT_EQ(config.remoteClient.maxReconnectInterval, 120);
    EXPECT_EQ(config.remoteClient.heartbeatInterval, 15);
    EXPECT_EQ(config.remoteClient.connectTimeout, 3);
    EXPECT_EQ(config.remoteClient.registerToken, "tok-123");
}

TEST_F(AgentConfigTest, DebuggerSectionParsesBoolsAndInt) {
    const auto config = AgentConfig::loadFromString(R"(
        [debugger]
        enable = false
        wait_for_ide = true
        listen_port = 7777
    )");
    EXPECT_FALSE(config.debugger.enable);
    EXPECT_TRUE(config.debugger.waitForIde);
    EXPECT_EQ(config.debugger.listenPort, 7777);
}

TEST_F(AgentConfigTest, LoggingSectionParsesBoolAndStrings) {
    const auto config = AgentConfig::loadFromString(R"(
        [logging]
        console = false
        level = "debug"
        file = "agent.log"
    )");
    EXPECT_FALSE(config.logging.console);
    EXPECT_EQ(config.logging.level, "debug");
    EXPECT_EQ(config.logging.file, "agent.log");
}

TEST_F(AgentConfigTest, PerformanceSectionParsesInts) {
    const auto config = AgentConfig::loadFromString(R"(
        [performance]
        screenshot_cache_size = 32
        match_thread_pool_size = 8
        memory_limit_mb = 1024
    )");
    EXPECT_EQ(config.performance.screenshotCacheSize, 32);
    EXPECT_EQ(config.performance.matchThreadPoolSize, 8);
    EXPECT_EQ(config.performance.memoryLimitMb, 1024);
}

TEST_F(AgentConfigTest, StandaloneSectionParsesScriptDir) {
    const auto config = AgentConfig::loadFromString(R"(
        [standalone]
        script_dir = "scripts/custom"
    )");
    EXPECT_EQ(config.standalone.scriptDir, "scripts/custom");
}

TEST_F(AgentConfigTest, InlineCommentInsideQuotesIsPreserved) {
    // stripInlineComment 只在引号外切 '#'；引号内的 '#' 是值的一部分
    const auto config = AgentConfig::loadFromString(R"(
        [remote]
        register_token = "tok#value"
    )");
    EXPECT_EQ(config.remoteClient.registerToken, "tok#value");
}

TEST_F(AgentConfigTest, UnquoteOnlyStripsPairedQuotes) {
    const auto config = AgentConfig::loadFromString(R"(
        [logging]
        level = "debug"
        file = agent-unquoted.log
    )");
    EXPECT_EQ(config.logging.level, "debug");
    // 值本身不带引号：原样保留（不报错）
    EXPECT_EQ(config.logging.file, "agent-unquoted.log");
}

TEST_F(AgentConfigTest, UnknownKeysAndSectionsAreIgnored) {
    // 当前解析器契约：未识别的节/键静默忽略（不报错、不落任何字段）
    const auto config = AgentConfig::loadFromString(R"(
        [unknown_section]
        mystery_key = 42
        [remote]
        unknown_remote_key = "x"
        server_port = 9001
    )");
    EXPECT_EQ(config.remoteClient.serverPort, 9001);
    EXPECT_EQ(config.remoteClient.serverIp, AgentConfig{}.remoteClient.serverIp);
}

TEST_F(AgentConfigTest, WrongTypeValueKeepsDefault) {
    // 类型不匹配（int 键给了非数字串）走字符串分支，而该分支没有此键名
    // → 静默忽略并保留默认值。这是当前实现的契约（容错而非报错）。
    const auto config = AgentConfig::loadFromString(R"(
        [remote]
        server_port = not-a-number
    )");
    EXPECT_EQ(config.remoteClient.serverPort, AgentConfig{}.remoteClient.serverPort);
}

TEST_F(AgentConfigTest, OutOfRangeIntegerThrows) {
    // std::stoi 对超范围数字串抛 std::out_of_range 且解析器不捕获——
    // 生产侧 Agent::initialize 的 catch 兜底为默认配置，此处钉解析层契约
    EXPECT_THROW(
        AgentConfig::loadFromString("[remote]\nserver_port = 99999999999999999999"),
        std::exception);
}

// ========== loadFromFile / saveToFile ==========

TEST_F(AgentConfigTest, LoadFromNonexistentFileThrowsWithPathInMessage) {
    const auto path = tempFile("missing");
    try {
        AgentConfig::loadFromFile(path.string());
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find(path.string()), std::string::npos)
            << "error message should contain the path: " << e.what();
    }
}

TEST_F(AgentConfigTest, SaveToUnwritableDirectoryReturnsFalse) {
    AgentConfig config;
    const auto dir = tempFile("dir"); // 一个不存在的目录下的子路径
    EXPECT_FALSE(config.saveToFile((dir / "nested" / "config.toml").string()));
}

TEST_F(AgentConfigTest, SaveThenLoadRoundTripsAllPersistedSections) {
    AgentConfig config;
    config.enableRemote = false;
    config.enableLocalIpc = true;
    config.enableStandaloneScript = true;
    config.remoteClient.serverIp = "192.168.7.7";
    config.remoteClient.serverPort = 9443;
    config.remoteClient.reconnectInterval = 11;
    config.remoteClient.maxReconnectInterval = 300;
    config.remoteClient.heartbeatInterval = 10;
    config.remoteClient.connectTimeout = 4;
    config.remoteClient.registerToken = "secret#token";
    config.standalone.scriptDir = "scripts/rt";
    config.debugger.enable = false;
    config.debugger.listenPort = 7654;
    config.debugger.waitForIde = true;
    config.logging.console = false;
    config.logging.level = "warn";
    config.logging.file = "rt-agent.log";
    config.performance.screenshotCacheSize = 64;
    config.performance.matchThreadPoolSize = 9;
    config.performance.memoryLimitMb = 2048;

    const auto path = tempFile("roundtrip");
    ASSERT_TRUE(config.saveToFile(path.string()));

    const auto loaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(loaded.enableRemote, config.enableRemote);
    EXPECT_EQ(loaded.enableLocalIpc, config.enableLocalIpc);
    EXPECT_EQ(loaded.enableStandaloneScript, config.enableStandaloneScript);
    EXPECT_EQ(loaded.remoteClient.serverIp, config.remoteClient.serverIp);
    EXPECT_EQ(loaded.remoteClient.serverPort, config.remoteClient.serverPort);
    EXPECT_EQ(loaded.remoteClient.reconnectInterval, config.remoteClient.reconnectInterval);
    EXPECT_EQ(loaded.remoteClient.maxReconnectInterval, config.remoteClient.maxReconnectInterval);
    EXPECT_EQ(loaded.remoteClient.heartbeatInterval, config.remoteClient.heartbeatInterval);
    EXPECT_EQ(loaded.remoteClient.connectTimeout, config.remoteClient.connectTimeout);
    EXPECT_EQ(loaded.remoteClient.registerToken, config.remoteClient.registerToken);
    EXPECT_EQ(loaded.standalone.scriptDir, config.standalone.scriptDir);
    EXPECT_EQ(loaded.debugger.enable, config.debugger.enable);
    EXPECT_EQ(loaded.debugger.listenPort, config.debugger.listenPort);
    EXPECT_EQ(loaded.debugger.waitForIde, config.debugger.waitForIde);
    EXPECT_EQ(loaded.logging.console, config.logging.console);
    EXPECT_EQ(loaded.logging.level, config.logging.level);
    EXPECT_EQ(loaded.logging.file, config.logging.file);
    EXPECT_EQ(loaded.performance.screenshotCacheSize, config.performance.screenshotCacheSize);
    EXPECT_EQ(loaded.performance.matchThreadPoolSize, config.performance.matchThreadPoolSize);
    EXPECT_EQ(loaded.performance.memoryLimitMb, config.performance.memoryLimitMb);
}

TEST_F(AgentConfigTest, SaveToFilePersistsPerformanceSection) {
    // 缺陷回归钉：saveToFile 曾漏写 [performance]，用户手调值在 runtime
    // 配置写回（Agent::applyRemoteConfig）时被静默抹掉
    AgentConfig config;
    config.performance.screenshotCacheSize = 48;
    config.performance.matchThreadPoolSize = 6;
    config.performance.memoryLimitMb = 4096;

    const auto path = tempFile("perf");
    ASSERT_TRUE(config.saveToFile(path.string()));

    std::ifstream in(path);
    const std::string content((std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("[performance]"), std::string::npos)
        << "saved config must contain [performance] section:\n" << content;

    const auto loaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(loaded.performance.screenshotCacheSize, 48);
    EXPECT_EQ(loaded.performance.matchThreadPoolSize, 6);
    EXPECT_EQ(loaded.performance.memoryLimitMb, 4096);
}

TEST_F(AgentConfigTest, DefaultConfigSavesAndReloadsIdentical) {
    // Agent::initialize 首跑路径：文件不存在 → 写默认配置 → 下次启动读回
    const AgentConfig defaults;
    const auto path = tempFile("default");
    ASSERT_TRUE(defaults.saveToFile(path.string()));

    const auto loaded = AgentConfig::loadFromFile(path.string());
    EXPECT_EQ(loaded.enableRemote, defaults.enableRemote);
    EXPECT_EQ(loaded.enableLocalIpc, defaults.enableLocalIpc);
    EXPECT_EQ(loaded.enableStandaloneScript, defaults.enableStandaloneScript);
    EXPECT_EQ(loaded.remoteClient.serverIp, defaults.remoteClient.serverIp);
    EXPECT_EQ(loaded.remoteClient.serverPort, defaults.remoteClient.serverPort);
    EXPECT_EQ(loaded.debugger.listenPort, defaults.debugger.listenPort);
    EXPECT_EQ(loaded.logging.level, defaults.logging.level);
    EXPECT_EQ(loaded.performance.screenshotCacheSize, defaults.performance.screenshotCacheSize);
    EXPECT_EQ(loaded.performance.memoryLimitMb, defaults.performance.memoryLimitMb);
    EXPECT_EQ(loaded.getRunMode(), defaults.getRunMode());
}

} // namespace
