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

namespace {

// ctest -j 下每用例独立进程并发：进程内 static 序号跨进程重合，确定性目录名
// 会撞车——先收尾进程的 remove_all 连树拔掉在用目录，后者 bind socket 报
// ENOENT。掺 pid 保证并发进程的临时目录互不相撞。
int processId() {
#if defined(_WIN32)
    return _getpid();
#else
    return getpid();
#endif
}

// 进程内唯一临时路径（socket/脚本/宏文件共用；pid 掺名 + 进程内序号递增）
std::string tempPath(const std::string& tag) {
    static std::atomic<int> seq{0};
    static const auto base = fs::temp_directory_path();
    static const auto pid = std::to_string(processId());
    return (base / ("wm-rpcipc-" + tag + "-" + pid + "-" +
                    std::to_string(seq.fetch_add(1)) + ".tmp"))
        .string();
}

// 分发一条 RPC 请求并解析响应信封 {type, id, data}
json dispatchCall(RpcDispatcher& dispatcher, const std::string& method,
                  const json& params = json::object()) {
    const json request = {
        {"type", "call"}, {"id", "t"}, {"method", method}, {"params", params}};
    return json::parse(dispatcher.dispatch(request.dump()));
}

void clearEventBuffer() { wingman::runtime::EventBuffer::instance().clear(); }

std::vector<wingman::runtime::IpcEvent> drainEventBuffer() {
    return wingman::runtime::EventBuffer::instance().drain(5000);
}

// LocalIpcServer 回环用的极简客户端：connect 成功后再启动接收线程
// （通道的 receiveLoop 以 isConnected 为循环条件，先收后连会立即退出）。
class TestIpcClient {
public:
    explicit TestIpcClient(const std::string& endpoint) : endpoint_(endpoint) {
        wingman::ipc::IpcConfig config;
        config.serverName = endpoint;
        channel_ = wingman::ipc::IpcFactory::createClient(config);
    }

    bool connect() {
        if (!channel_) {
            return false;
        }
        if (!channel_->connect(endpoint_)) {
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

    bool send(const wingman::ipc::IpcMessage& message) {
        return channel_ && channel_->send(message);
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

// 构造一个可被 playback 消费且无副作用的事件宏：type=Delay(8) 走 switch 的
// default 分支不注入输入；timestamp 0/1 相邻使得修复后 speed 钳到 1 时单轮
// 仅 100ms 延迟，而修复前 speed=0 触发除零、负 speed 触发无符号下溢挂死。
std::string writeTwoTickMacro(const std::string& path) {
    std::ofstream file(path);
    file << "{\"events\":["
         << "{\"type\":8,\"timestamp\":0,\"delay\":0},"
         << "{\"type\":8,\"timestamp\":1,\"delay\":0}]}";
    return path;
}

} // namespace

// ========== script.*（真实 StandaloneMode + 真实 Lua） ==========

class ScriptHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        clearEventBuffer();
        // 幂等：static 注册器只在首次调用时把 Lua 引擎注册进 Factory
        wingman::lua::registerLuaEngine();
        config_.scriptDir = tempPath("scripts");
        standalone_ = std::make_unique<StandaloneMode>(config_);
        wingman::rpc::registerScriptHandlers(dispatcher_, *standalone_);
    }

    void TearDown() override {
        standalone_.reset();
        std::error_code ec;
        fs::remove_all(config_.scriptDir, ec);
    }

    std::string writeScript(const std::string& name, const std::string& body) {
        fs::create_directories(config_.scriptDir);
        const auto path = fs::path(config_.scriptDir) / name;
        std::ofstream file(path);
        file << body;
        return path.string();
    }

    StandaloneModeConfig config_;
    std::unique_ptr<StandaloneMode> standalone_;
    RpcDispatcher dispatcher_;
};

TEST_F(ScriptHandlerTest, ListEmptyInitially) {
    const auto response = dispatchCall(dispatcher_, "script.list");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    EXPECT_TRUE(response["data"]["result"]["scripts"].empty());
}

TEST_F(ScriptHandlerTest, ListReflectsLoadedScript) {
    const auto path = writeScript("simple.lua", "return 1\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(dispatcher_, "script.list");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& scripts = response["data"]["result"]["scripts"];
    ASSERT_EQ(scripts.size(), 1u);
    EXPECT_EQ(scripts[0]["id"].get<std::string>(), id);
    EXPECT_EQ(scripts[0]["path"].get<std::string>(), path);
    // 产品语义：ScriptManager 加载后的初始状态为 unloaded，runtime 层映射为
    // "stopped"（script_handler 的 scriptStateToString）
    EXPECT_EQ(scripts[0]["state"].get<std::string>(), "stopped");
    EXPECT_FALSE(scripts[0]["isRunning"].get<bool>());
    EXPECT_TRUE(scripts[0].contains("loadedAt"));
}

TEST_F(ScriptHandlerTest, StartWithoutPathFails) {
    const auto response = dispatchCall(dispatcher_, "script.start");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing path");
}

TEST_F(ScriptHandlerTest, StartWithNonexistentFileFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.start", json{{"path", tempPath("missing") + ".lua"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to load script");
}

TEST_F(ScriptHandlerTest, StartBlocksThenStopReportsNotRunning) {
    // 产品契约：ScriptManager 为同步执行模型，script.start 阻塞至脚本执行
    // 完成才返回。顺序 RPC 因此永远无法命中 running 窗口——对已完成脚本
    // stop 返回失败信封是确定性契约（「GUI 停止运行中脚本」在该模型下不可
    // 达，属真机观察范畴，见 todo.md）。
    const auto path = writeScript("life.lua", "return 7\n");
    const auto started = dispatchCall(
        dispatcher_, "script.start", json{{"path", path}});
    ASSERT_TRUE(started["data"]["success"].get<bool>()) << started.dump();
    const auto scriptId = started["data"]["result"]["scriptId"].get<std::string>();
    EXPECT_FALSE(scriptId.empty());
    EXPECT_FALSE(started["data"]["result"]["reused"].get<bool>());
    EXPECT_EQ(started["data"]["result"]["status"].get<std::string>(), "running");

    const auto stopped = dispatchCall(
        dispatcher_, "script.stop", json{{"scriptId", scriptId}});
    EXPECT_FALSE(stopped["data"]["success"].get<bool>());
    EXPECT_EQ(stopped["data"]["error"].get<std::string>(), "Failed to stop script");

    const auto unloaded = dispatchCall(
        dispatcher_, "script.unload", json{{"scriptId", scriptId}});
    EXPECT_TRUE(unloaded["data"]["success"].get<bool>()) << unloaded.dump();
}

TEST_F(ScriptHandlerTest, StartReusesLoadedScriptById) {
    const auto path = writeScript("reuse.lua", "return 2\n");
    const auto first = dispatchCall(dispatcher_, "script.start", json{{"path", path}});
    ASSERT_TRUE(first["data"]["success"].get<bool>()) << first.dump();
    const auto firstId = first["data"]["result"]["scriptId"].get<std::string>();

    const auto second = dispatchCall(dispatcher_, "script.start", json{{"path", path}});
    ASSERT_TRUE(second["data"]["success"].get<bool>()) << second.dump();
    EXPECT_TRUE(second["data"]["result"]["reused"].get<bool>());
    EXPECT_EQ(second["data"]["result"]["scriptId"].get<std::string>(), firstId);
}

TEST_F(ScriptHandlerTest, StopWithoutScriptIdFails) {
    const auto response = dispatchCall(dispatcher_, "script.stop");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing scriptId");
}

TEST_F(ScriptHandlerTest, StopUnknownScriptFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.stop", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to stop script");
}

TEST_F(ScriptHandlerTest, PauseAndResumeUnknownScriptFail) {
    const auto paused = dispatchCall(
        dispatcher_, "script.pause", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(paused["data"]["success"].get<bool>());
    EXPECT_EQ(paused["data"]["error"].get<std::string>(), "Failed to pause script");

    const auto resumed = dispatchCall(
        dispatcher_, "script.resume", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(resumed["data"]["success"].get<bool>());
    EXPECT_EQ(resumed["data"]["error"].get<std::string>(), "Failed to resume script");
}

TEST_F(ScriptHandlerTest, PauseLoadedButNotRunningScriptFails) {
    // 运行中脚本的协作暂停属真机观察范畴；此处锁定确定性路径：
    // loaded（未运行）脚本 pause 必须 false 而非伪成功
    const auto path = writeScript("idle.lua", "return 3\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(
        dispatcher_, "script.pause", json{{"scriptId", id}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to pause script");
}

TEST_F(ScriptHandlerTest, RestartUnknownScriptFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.restart", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Script not found");
}

TEST_F(ScriptHandlerTest, RestartLoadedScriptSucceeds) {
    const auto path = writeScript("restart.lua", "return 4\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(
        dispatcher_, "script.restart", json{{"scriptId", id}});
    EXPECT_TRUE(response["data"]["success"].get<bool>()) << response.dump();
}

TEST_F(ScriptHandlerTest, UnloadUnknownScriptFails) {
    const auto response = dispatchCall(
        dispatcher_, "script.unload", json{{"scriptId", "script_404"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(),
        "Failed to unload script (stop it first)");
}

TEST_F(ScriptHandlerTest, UnloadLoadedScriptSucceeds) {
    const auto path = writeScript("unload.lua", "return 5\n");
    const auto id = standalone_->loadScript(path);
    ASSERT_FALSE(id.empty());

    const auto response = dispatchCall(
        dispatcher_, "script.unload", json{{"scriptId", id}});
    EXPECT_TRUE(response["data"]["success"].get<bool>()) << response.dump();

    const auto listed = dispatchCall(dispatcher_, "script.list");
    EXPECT_TRUE(listed["data"]["result"]["scripts"].empty());
}

// 缺陷回归守卫（本轮修复）：仅 LocalIpc 能力的 runtime 不会 start()
// StandaloneMode，GUI 仍可经 script.start 加载脚本；析构时 stop() 原实现的
// running_ 早退会跳过清理，把脚本连同引擎永久留在进程级全局 ScriptManager。
TEST_F(ScriptHandlerTest, DestructorReleasesNeverStartedLoadedScripts) {
    const auto path = writeScript("leak.lua", "return 6\n");
    std::string id;
    {
        StandaloneMode transient(config_);
        id = transient.loadScript(path);
        ASSERT_FALSE(id.empty());
        ASSERT_NE(wingman::runtime::getScriptManager().getScriptInfo(id), nullptr);
    } // 析构：未 start() 也必须清理登记
    EXPECT_EQ(wingman::runtime::getScriptManager().getScriptInfo(id), nullptr);
}

// ========== events.drain ==========

TEST(EventHandlerTest, DrainRespectsLimitAndReportsRemaining) {
    clearEventBuffer();
    auto& buffer = wingman::runtime::EventBuffer::instance();
    buffer.push("t.one", {{"k", 1}});
    buffer.push("t.two", {{"k", 2}});
    buffer.push("t.three", {{"k", 3}});

    RpcDispatcher dispatcher;
    wingman::rpc::registerEventHandlers(dispatcher);

    const auto response = dispatchCall(dispatcher, "events.drain", json{{"max", 2}});
    const auto& result = response.at("data").at("result");
    ASSERT_EQ(result.at("events").size(), 2u);
    EXPECT_EQ(result.at("events")[0].at("method").get<std::string>(), "t.one");
    EXPECT_EQ(result.at("events")[1].at("method").get<std::string>(), "t.two");
    EXPECT_EQ(result.at("remaining").get<std::size_t>(), 1u);
    EXPECT_TRUE(result.contains("dropped"));

    const auto rest = dispatchCall(dispatcher, "events.drain");
    const auto& restResult = rest.at("data").at("result");
    ASSERT_EQ(restResult.at("events").size(), 1u);
    EXPECT_EQ(restResult.at("events")[0].at("method").get<std::string>(), "t.three");
    EXPECT_EQ(restResult.at("remaining").get<std::size_t>(), 0u);
}

TEST(EventHandlerTest, DrainOnEmptyBufferYieldsEmptyWithCounters) {
    clearEventBuffer();
    RpcDispatcher dispatcher;
    wingman::rpc::registerEventHandlers(dispatcher);

    const auto response = dispatchCall(dispatcher, "events.drain");
    const auto& result = response.at("data").at("result");
    EXPECT_TRUE(result.at("events").empty());
    EXPECT_EQ(result.at("remaining").get<std::size_t>(), 0u);
}

// ========== config.getRemote / config.setRemote ==========

TEST(ConfigHandlerTest, GetRemoteWithoutAccessReturnsError) {
    RpcDispatcher dispatcher;
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, {});

    const auto response = dispatchCall(dispatcher, "config.getRemote");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(),
        "remote config not available");
}

TEST(ConfigHandlerTest, SetRemoteWithoutApplyReturnsError) {
    RpcDispatcher dispatcher;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [] { return json{{"serverIp", "10.0.0.1"}}; };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.setRemote", json{{}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(),
        "remote config not writable");
}

TEST(ConfigHandlerTest, GetRemoteReturnsProvidedConfig) {
    RpcDispatcher dispatcher;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [] { return json{{"serverIp", "10.0.0.1"}, {"serverPort", 9000}}; };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.getRemote");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["serverIp"].get<std::string>(), "10.0.0.1");
    EXPECT_EQ(result["serverPort"].get<int>(), 9000);
}

TEST(ConfigHandlerTest, SetRemoteAppliesAndReturnsFreshConfig) {
    RpcDispatcher dispatcher;
    json applied;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [&applied] { return applied; };
    access.apply = [&applied](const json& request) -> std::string {
        applied = request.value("config", json::object());
        return {};
    };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.setRemote",
        json{{"config", {{"serverIp", "9.9.9.9"}}}});
    ASSERT_TRUE(response["data"]["success"].get<bool>()) << response.dump();
    EXPECT_EQ(response["data"]["result"]["serverIp"].get<std::string>(), "9.9.9.9");
}

TEST(ConfigHandlerTest, SetRemotePropagatesApplyError) {
    RpcDispatcher dispatcher;
    wingman::rpc::RemoteConfigAccess access;
    access.get = [] { return json::object(); };
    access.apply = [](const json&) -> std::string { return "bad token"; };
    wingman::rpc::registerRuntimeConfigHandlers(dispatcher, access);

    const auto response = dispatchCall(dispatcher, "config.setRemote", json{{}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "bad token");
}

// ========== macro.* ==========

class MacroHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        wingman::rpc::registerMacroHandlers(dispatcher_, recorder_);
    }

    wingman::MacroRecorder recorder_;
    RpcDispatcher dispatcher_;
};

TEST_F(MacroHandlerTest, StatusInitiallyIdle) {
    const auto response = dispatchCall(dispatcher_, "macro.status");
    const auto& result = response["data"]["result"];
    EXPECT_FALSE(result["recording"].get<bool>());
    EXPECT_FALSE(result["paused"].get<bool>());
    EXPECT_EQ(result["eventCount"].get<std::size_t>(), 0u);
}

TEST_F(MacroHandlerTest, ClearSucceedsAndResetsEventCount) {
    const auto cleared = dispatchCall(dispatcher_, "macro.clear");
    EXPECT_TRUE(cleared["data"]["success"].get<bool>());

    const auto status = dispatchCall(dispatcher_, "macro.status");
    EXPECT_EQ(status["data"]["result"]["eventCount"].get<std::size_t>(), 0u);
}

TEST_F(MacroHandlerTest, SaveWithoutPathFails) {
    const auto response = dispatchCall(dispatcher_, "macro.save");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing path");
}

TEST_F(MacroHandlerTest, SaveToUnwritableDirectoryFails) {
    const auto response = dispatchCall(dispatcher_, "macro.save",
        json{{"path", tempPath("no-such-dir") + "/macro.json"}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to save macro");
}

TEST_F(MacroHandlerTest, LoadWithoutPathFails) {
    const auto response = dispatchCall(dispatcher_, "macro.load");
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Missing path");
}

TEST_F(MacroHandlerTest, LoadNonexistentFileFails) {
    const auto response = dispatchCall(
        dispatcher_, "macro.load", json{{"path", tempPath("missing-macro")}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to load macro");
}

TEST_F(MacroHandlerTest, LoadGarbageFileFails) {
    const auto path = tempPath("garbage-macro");
    {
        std::ofstream file(path);
        file << "not-json{{{";
    }
    const auto response = dispatchCall(dispatcher_, "macro.load", json{{"path", path}});
    EXPECT_FALSE(response["data"]["success"].get<bool>());
    EXPECT_EQ(response["data"]["error"].get<std::string>(), "Failed to load macro");
    std::error_code ec;
    fs::remove(path, ec);
}

TEST_F(MacroHandlerTest, SaveLoadRoundTripPreservesEventCount) {
    const auto macroPath = writeTwoTickMacro(tempPath("roundtrip-macro"));

    const auto loaded = dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}});
    ASSERT_TRUE(loaded["data"]["success"].get<bool>()) << loaded.dump();
    // 带 success 键的 handler 返回体不包 result 层，直接作为 data
    EXPECT_EQ(loaded["data"]["eventCount"].get<std::size_t>(), 2u);

    const auto status = dispatchCall(dispatcher_, "macro.status");
    EXPECT_EQ(status["data"]["result"]["eventCount"].get<std::size_t>(), 2u);

    const auto savedPath = tempPath("saved-macro");
    const auto saved = dispatchCall(dispatcher_, "macro.save", json{{"path", savedPath}});
    ASSERT_TRUE(saved["data"]["success"].get<bool>()) << saved.dump();
    EXPECT_EQ(saved["data"]["eventCount"].get<std::size_t>(), 2u);

    std::error_code ec;
    fs::remove(savedPath, ec);
    fs::remove(macroPath, ec);
}

TEST_F(MacroHandlerTest, PlayWithEmptyQueueSucceeds) {
    const auto response = dispatchCall(dispatcher_, "macro.play");
    EXPECT_TRUE(response["data"]["success"].get<bool>());
}

TEST_F(MacroHandlerTest, PlayWithNonPositiveRepeatClampsToOne) {
    const auto macroPath = writeTwoTickMacro(tempPath("repeat-macro"));
    ASSERT_TRUE(dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}})
                    ["data"]["success"]
                    .get<bool>());

    // repeat<=0 原语义为「循环 0 次」，钳到 1 后保持有回放语义；无副作用事件
    //（type=Delay）保证该用例瞬时完成
    const auto response = dispatchCall(
        dispatcher_, "macro.play", json{{"speed", 100}, {"repeat", 0}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    std::error_code ec;
    fs::remove(macroPath, ec);
}

// 缺陷回归守卫（本轮修复）：speed=0 直达 playback 的整数除法触发 SIGFPE，
// 进程级崩溃；修复在 RPC 边界钳制 speed>=1。
TEST_F(MacroHandlerTest, PlayWithZeroSpeedIsClampedInsteadOfCrashing) {
    const auto macroPath = writeTwoTickMacro(tempPath("zerofpe-macro"));
    ASSERT_TRUE(dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}})
                    ["data"]["success"]
                    .get<bool>());

    const auto response = dispatchCall(
        dispatcher_, "macro.play", json{{"speed", 0}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    std::error_code ec;
    fs::remove(macroPath, ec);
}

// 缺陷回归守卫（本轮修复）：负 speed 使 playback 内 (delay*100)/speed 得负值，
// 赋给 unsigned long 下溢为巨量毫秒 → sleepMs 挂死 runtime。钳制后 speed=1，
// 相邻 timestamp 仅产生 100ms 延迟；修复前该用例会挂死直至测试超时。
TEST_F(MacroHandlerTest, PlayWithNegativeSpeedIsClampedInsteadOfHanging) {
    const auto macroPath = writeTwoTickMacro(tempPath("negspeed-macro"));
    ASSERT_TRUE(dispatchCall(dispatcher_, "macro.load", json{{"path", macroPath}})
                    ["data"]["success"]
                    .get<bool>());

    const auto response = dispatchCall(
        dispatcher_, "macro.play", json{{"speed", -7}});
    EXPECT_TRUE(response["data"]["success"].get<bool>());

    std::error_code ec;
    fs::remove(macroPath, ec);
}

#if defined(__linux__)
// 平台钩子录制（X11/XRecord）依赖显示环境：断言信封与状态一致性，
// 不断言 recording 必为 true（无 X 环境优雅失败）
TEST_F(MacroHandlerTest, StartStopEnvelopesAreConsistent) {
    const auto started = dispatchCall(dispatcher_, "macro.start");
    EXPECT_TRUE(started["data"]["success"].get<bool>());
    EXPECT_EQ(started["data"]["recording"].get<bool>(), recorder_.isRecording());

    const auto stopped = dispatchCall(dispatcher_, "macro.stop");
    EXPECT_TRUE(stopped["data"]["success"].get<bool>());
    EXPECT_FALSE(stopped["data"]["recording"].get<bool>());
    EXPECT_EQ(stopped["data"]["eventCount"].get<std::size_t>(),
        recorder_.getEventCount());
    EXPECT_FALSE(recorder_.isRecording());
}
#endif

// ========== system.*（providers 注入与批量操作分支） ==========

class SystemProvidersHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        standalone_ = std::make_unique<StandaloneMode>(StandaloneModeConfig{});
        providers_.remoteConnected = [] { return true; };
        providers_.remoteStateName = [] { return std::string("connected"); };
        providers_.ipcClientConnected = [] { return true; };
        providers_.runMode = [] { return 3; };
        wingman::rpc::registerRuntimeSystemHandlers(
            dispatcher_, "prov-version", *standalone_, providers_);
    }

    std::unique_ptr<StandaloneMode> standalone_;
    wingman::rpc::RuntimeStatusProviders providers_;
    RpcDispatcher dispatcher_;
};

TEST_F(SystemProvidersHandlerTest, GetStatusReflectsInjectedProviders) {
    const auto response = dispatchCall(dispatcher_, "system.getStatus");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_EQ(result["version"].get<std::string>(), "prov-version");
    EXPECT_TRUE(result["remoteConnected"].get<bool>());
    EXPECT_EQ(result["remoteState"].get<std::string>(), "connected");
    EXPECT_TRUE(result["ipcClientConnected"].get<bool>());
    EXPECT_EQ(result["mode"].get<int>(), 3);
}

TEST_F(SystemProvidersHandlerTest, TogglePauseWithoutScriptsFlipsToPaused) {
    const auto response = dispatchCall(dispatcher_, "system.togglePause");
    ASSERT_TRUE(response["data"]["success"].get<bool>());
    const auto& result = response["data"]["result"];
    EXPECT_TRUE(result["paused"].get<bool>());
    EXPECT_EQ(result["changedScripts"].get<std::size_t>(), 0u);
}

TEST_F(SystemProvidersHandlerTest, BatchPauseResumeStopWithoutScriptsAreNoops) {
    const auto paused = dispatchCall(dispatcher_, "system.pauseAll");
    ASSERT_TRUE(paused["data"]["success"].get<bool>());
    EXPECT_EQ(paused["data"]["result"]["changedScripts"].get<std::size_t>(), 0u);

    const auto resumed = dispatchCall(dispatcher_, "system.resumeAll");
    ASSERT_TRUE(resumed["data"]["success"].get<bool>());
    EXPECT_EQ(resumed["data"]["result"]["changedScripts"].get<std::size_t>(), 0u);

    const auto stopped = dispatchCall(dispatcher_, "system.stopAll");
    ASSERT_TRUE(stopped["data"]["success"].get<bool>());
    EXPECT_EQ(stopped["data"]["result"]["stoppedScripts"].get<std::size_t>(), 0u);
}

// ========== EventLogSink ==========

TEST(EventLogSinkTest, ForwardsInfoWarnErrorAndFiltersDebug) {
    clearEventBuffer();
    auto logger = std::make_shared<spdlog::logger>(
        "wm-rpcipc-evlog", wingman::runtime::createEventLogSink());
    logger->debug("wm-rpcipc-debug-filtered");
    logger->info("wm-rpcipc-info-{}", 42);
    logger->warn("wm-rpcipc-warn-passed");
    logger->error("wm-rpcipc-error-passed");
    logger.reset();

    // token 扫描断言（进程内可能存在其他异步生产者，不做事件总量断言）
    bool sawInfo = false, sawWarn = false, sawError = false, sawDebug = false;
    for (const auto& event : drainEventBuffer()) {
        if (event.method != "log.line") {
            continue;
        }
        const auto& payload = event.payload;
        const auto message = payload.at("message").get<std::string>();
        const auto level = payload.at("level").get<std::string>();
        if (message == "wm-rpcipc-info-42") {
            sawInfo = (level == "info");
        } else if (message == "wm-rpcipc-warn-passed") {
            // spdlog 的 warn 级别名在不同小版本间为 warning/warn
            sawWarn = (level == "warning" || level == "warn");
        } else if (message == "wm-rpcipc-error-passed") {
            sawError = (level == "error");
        } else if (message == "wm-rpcipc-debug-filtered") {
            sawDebug = true; // debug 低于 info 必须被过滤
        }
    }
    EXPECT_TRUE(sawInfo);
    EXPECT_TRUE(sawWarn);
    EXPECT_TRUE(sawError);
    EXPECT_FALSE(sawDebug);
}

TEST(EventLogSinkTest, TruncatesOversizedLines) {
    clearEventBuffer();
    auto logger = std::make_shared<spdlog::logger>(
        "wm-rpcipc-evlog-trunc", wingman::runtime::createEventLogSink());
    logger->error(std::string(5000, 'x'));
    logger.reset();

    bool found = false;
    for (const auto& event : drainEventBuffer()) {
        if (event.method != "log.line") {
            continue;
        }
        const auto message = event.payload.at("message").get<std::string>();
        if (message.size() > 4000) {
            found = true;
            EXPECT_EQ(message.size(), 4096u + std::string("...[truncated]").size());
            EXPECT_TRUE(message.ends_with("...[truncated]"));
        }
    }
    EXPECT_TRUE(found);
}

TEST(EventLogSinkTest, ConstructorArgRaisesFilterFloor) {
    clearEventBuffer();
    auto logger = std::make_shared<spdlog::logger>("wm-rpcipc-evlog-floor",
        wingman::runtime::createEventLogSink(
            static_cast<std::size_t>(spdlog::level::err)));
    logger->warn("wm-rpcipc-warn-filtered");
    logger->error("wm-rpcipc-error-passed");
    logger.reset();

    bool sawError = false, sawWarn = false;
    for (const auto& event : drainEventBuffer()) {
        if (event.method != "log.line") {
            continue;
        }
        const auto message = event.payload.at("message").get<std::string>();
        if (message == "wm-rpcipc-error-passed") {
            sawError = true;
        } else if (message == "wm-rpcipc-warn-filtered") {
            sawWarn = true; // err 下限之上 warn 必须被过滤
        }
    }
    EXPECT_TRUE(sawError);
    EXPECT_FALSE(sawWarn);
}

// ========== LocalIpcServer 回环（Unix：Linux/macOS） ==========

#ifndef _WIN32

class LocalIpcServerLoopbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        clearEventBuffer();
        wingman::lua::registerLuaEngine();
        endpoint_ = tempPath("ipc");
        standalone_ = std::make_unique<StandaloneMode>(StandaloneModeConfig{});
    }

    void TearDown() override {
        client_.reset(); // 先客户端后服务端：模拟 GUI 先行退出的顺序
        server_.reset();
        standalone_.reset();
        std::error_code ec;
        fs::remove(endpoint_, ec);
    }

    void startServer() {
        server_ = std::make_unique<wingman::runtime::LocalIpcServer>(
            *standalone_, endpoint_, nullptr);

        // access/providers 的 std::function 只捕获 this，成员生命周期覆盖整测
        wingman::rpc::RemoteConfigAccess access;
        access.get = [this] { return remoteConfig_; };
        access.apply = [this](const json& request) -> std::string {
            remoteConfig_ = request.value("config", json::object());
            return {};
        };
        server_->setRemoteConfigAccess(access);

        wingman::rpc::RuntimeStatusProviders providers;
        providers.remoteConnected = [] { return true; };
        providers.remoteStateName = [] { return std::string("connected"); };
        server_->setStatusProviders(providers);

        ASSERT_TRUE(server_->start());
    }

    void connectClient() {
        client_ = std::make_unique<TestIpcClient>(endpoint_);
        ASSERT_TRUE(client_->connect());
    }

    std::optional<json> rpc(const std::string& method,
                            const json& params = json::object()) {
        const uint64_t id = client_->call(method, params.dump());
        EXPECT_NE(id, 0u);
        const auto message = client_->awaitResponse(id, std::chrono::seconds{5});
        if (!message) {
            return std::nullopt;
        }
        try {
            return json::parse(message->payload);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // 轮询等待出现指定 state 的 connection.ipc_client 事件
    bool waitForIpcConnectionEvent(const std::string& state) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
        while (std::chrono::steady_clock::now() < deadline) {
            for (const auto& event : drainEventBuffer()) {
                if (event.method == "connection.ipc_client" &&
                    event.payload.value("state", "") == state) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
        return false;
    }

    std::string endpoint_;
    json remoteConfig_ = json{{"serverIp", "10.0.0.1"}};
    std::unique_ptr<StandaloneMode> standalone_;
    std::unique_ptr<wingman::runtime::LocalIpcServer> server_;
    std::unique_ptr<TestIpcClient> client_;
};

TEST_F(LocalIpcServerLoopbackTest, StartStopLifecycleIsIdempotent) {
    startServer();
    EXPECT_TRUE(server_->start()); // 已运行时立即返回 true
    client_.reset();
    server_->stop();
    EXPECT_NO_THROW(server_->stop()); // 二次 stop 幂等
}

TEST_F(LocalIpcServerLoopbackTest, RoundTripSystemGetVersion) {
    startServer();
    connectClient();

    const auto response = rpc("system.getVersion");
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->at("type").get<std::string>(), "response");
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("result").at("server").get<std::string>(), "wingman");
}

TEST_F(LocalIpcServerLoopbackTest, GetStatusExposesInjectedProviders) {
    startServer();
    connectClient();

    const auto response = rpc("system.getStatus");
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>());
    EXPECT_TRUE(data.at("result").at("remoteConnected").get<bool>());
    EXPECT_EQ(data.at("result").at("remoteState").get<std::string>(), "connected");
}

TEST_F(LocalIpcServerLoopbackTest, UnknownMethodReturnsErrorEnvelope) {
    startServer();
    connectClient();

    const auto response = rpc("no.such.method");
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    EXPECT_FALSE(data.at("success").get<bool>());
    EXPECT_NE(data.at("error").get<std::string>().find("Unknown method"),
        std::string::npos);
}

TEST_F(LocalIpcServerLoopbackTest, MalformedPayloadReturnsParseErrorEnvelope) {
    startServer();
    connectClient();

    wingman::ipc::IpcMessage message;
    message.type = wingman::ipc::IpcMessageType::Request;
    message.method = "system.getVersion";
    message.payload = "{definitely-not-json";
    message.id = 77;
    ASSERT_TRUE(client_->send(message));

    const auto raw = client_->awaitResponse(77, std::chrono::seconds{5});
    ASSERT_TRUE(raw.has_value());
    const auto response = json::parse(raw->payload);
    const auto& data = response.at("data");
    EXPECT_FALSE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("error").get<std::string>().substr(0, 21),
        "Invalid params JSON: ");
}

TEST_F(LocalIpcServerLoopbackTest, MissingMethodIsRejected) {
    startServer();
    connectClient();

    wingman::ipc::IpcMessage message;
    message.type = wingman::ipc::IpcMessageType::Request;
    message.method = "";
    message.payload = "{}";
    message.id = 78;
    ASSERT_TRUE(client_->send(message));

    const auto raw = client_->awaitResponse(78, std::chrono::seconds{5});
    ASSERT_TRUE(raw.has_value());
    const auto response = json::parse(raw->payload);
    EXPECT_FALSE(response.at("data").at("success").get<bool>());
    EXPECT_EQ(response.at("data").at("error").get<std::string>(),
        "Missing IPC method");
}

TEST_F(LocalIpcServerLoopbackTest, ErrorTypeMessageProducesErrorEnvelope) {
    startServer();
    connectClient();

    wingman::ipc::IpcMessage message;
    message.type = wingman::ipc::IpcMessageType::Error;
    message.method = "";
    message.payload = R"({"error":"boom"})";
    message.id = 79;
    ASSERT_TRUE(client_->send(message));

    const auto raw = client_->awaitResponse(79, std::chrono::seconds{5});
    ASSERT_TRUE(raw.has_value());
    const auto response = json::parse(raw->payload);
    const auto& data = response.at("data");
    EXPECT_FALSE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("error").get<std::string>(), "boom");
}

TEST_F(LocalIpcServerLoopbackTest, ConfigRoundTripThroughLoopback) {
    startServer();
    connectClient();

    const auto fetched = rpc("config.getRemote");
    ASSERT_TRUE(fetched.has_value());
    ASSERT_TRUE(fetched->at("data").at("success").get<bool>());
    EXPECT_EQ(fetched->at("data").at("result").at("serverIp").get<std::string>(),
        "10.0.0.1");

    const auto applied = rpc("config.setRemote",
        json{{"config", {{"serverIp", "9.9.9.9"}}}});
    ASSERT_TRUE(applied.has_value());
    ASSERT_TRUE(applied->at("data").at("success").get<bool>()) << applied->dump();
    EXPECT_EQ(applied->at("data").at("result").at("serverIp").get<std::string>(),
        "9.9.9.9");

    // 二次 dispatch 必须重新调用 get（handler 持有的是 access 拷贝而非配置快照）
    const auto refetched = rpc("config.getRemote");
    ASSERT_TRUE(refetched.has_value());
    EXPECT_EQ(refetched->at("data").at("result").at("serverIp").get<std::string>(),
        "9.9.9.9");
}

TEST_F(LocalIpcServerLoopbackTest, EventsDrainThroughLoopback) {
    startServer();
    connectClient();

    wingman::runtime::EventBuffer::instance().push("t.loopback", {{"k", true}});
    const auto response = rpc("events.drain", json{{"max", 10}});
    ASSERT_TRUE(response.has_value());
    const auto& result = response->at("data").at("result");
    bool found = false;
    for (const auto& event : result.at("events")) {
        if (event.at("method").get<std::string>() == "t.loopback") {
            found = true;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_EQ(result.at("remaining").get<std::size_t>(), 0u);
}

TEST_F(LocalIpcServerLoopbackTest, ClientDisconnectTriggersEventAndReconnect) {
    startServer();
    connectClient();

    const auto first = rpc("system.getVersion");
    ASSERT_TRUE(first.has_value());

    client_->disconnect();
    EXPECT_TRUE(waitForIpcConnectionEvent("disconnected"));

    connectClient(); // 服务端重听后第二个客户端可用
    const auto second = rpc("system.getVersion");
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(second->at("data").at("success").get<bool>());
}

TEST_F(LocalIpcServerLoopbackTest, StopWhileClientConnectedJoinsCleanly) {
    startServer();
    connectClient();

    const auto response = rpc("system.getVersion");
    ASSERT_TRUE(response.has_value());

    server_->stop(); // 客户端仍在线：服务端必须能收尾 join 而不悬挂
    client_->disconnect();
}

#endif // _WIN32
