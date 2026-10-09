// StandaloneMode（standalone_mode.cpp）覆盖率缺口补测。
// 基线：78%（51 行未覆盖）——此前覆盖几乎全部来自 agent_loopback_test 的间接
// 驱动，本文件对单机模式的编排面直接补测：start 幂等/建目录失败、autoStart
// 装配（含输出回调空串跳过）、脚本登记与状态映射（loaded/error/completed
// 现状钉）、unknown-id 防御、pause/resume/stopAll 对闲置脚本的计数契约、
// manager 已侧卸载后的 stop/unload 降级腿、getConfig。
//
// 2026-10-01 ScriptManager 状态机修复后补入全链回归钉：running 此前从不可达
// （唯一赋值点在 resumeScript，resume 前置 paused、pause 前置 running——死锁
// 环），pause/resume 成功腿、批量操作非零计数、running/paused 状态映射与对应
// state_changed 推送全部不可达；stop 成功腿仅 starting 窗口并发 stop 可达且
// 伴生 engine->shutdown() 与执行线程的数据竞争（不触发）。修复后：执行线程
// 起动即迁 running，stop 为协作停止（stopRequested，终态 loaded）——全链与
// 批量计数增量由本文件 FullChain/Bulk 两条用例钉住（根因与修复见 CHANGELOG
// 本轮条目）。
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/lua/lua_script_engine.hpp"
#include "wingman/runtime/runtime_context.hpp"
#include "wingman/runtime/standalone_mode.hpp"
#include "wingman/script_manager.hpp"

namespace fs = std::filesystem;

using wingman::runtime::EventBuffer;
using wingman::runtime::getScriptManager;
using wingman::runtime::ScriptState;
using wingman::runtime::StandaloneMode;
using wingman::runtime::StandaloneModeConfig;

namespace {

// 每用例独立临时目录；EventBuffer 为进程级单例，动作前先 drain 清残留，
// 动作后再 drain 断言（按 method + payload.id 过滤本用例的事件）。
class StandaloneModeCoverageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Lua 引擎经 registerLuaEngine() 惰性注册（函数内 static 登记器），
        // 与 rpc_ipc_test 同款接线；幂等，重复调用无副作用
        wingman::lua::registerLuaEngine();
        static std::atomic<uint64_t> seq{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        dir_ = fs::temp_directory_path() /
               ("wm_standalone_" + std::to_string(stamp) + "_" +
                std::to_string(seq.fetch_add(1)));
        fs::create_directories(dir_);
        EventBuffer::instance().drain();
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

    fs::path writeScript(const std::string& name, const std::string& content) {
        const auto path = dir_ / name;
        std::ofstream f(path);
        f << content;
        return path;
    }

    static std::vector<nlohmann::json> drainedEvents(const std::string& method,
                                                      const std::string& scriptId) {
        std::vector<nlohmann::json> found;
        for (const auto& evt : EventBuffer::instance().drain()) {
            if (evt.method != method) continue;
            if (scriptId.empty() || evt.payload.value("id", "") == scriptId) {
                found.push_back(evt.payload);
            }
        }
        return found;
    }

    // 轮询等待脚本进入期望的运行态（manager 等待循环以 50ms 为周期收尾，
    // 超时上界只作失败时限）
    static bool waitForRuntimeState(const StandaloneMode& mode, const std::string& id,
                                    ScriptState expected, int timeoutMs = 5000) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (mode.getScript(id).state == expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    fs::path dir_;
};

} // namespace

// ========== start()：幂等 / 建目录失败 / autoStart 装配 ==========

TEST_F(StandaloneModeCoverageTest, StartTwiceReturnsTrue) {
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    ASSERT_TRUE(mode.start());
    EXPECT_TRUE(mode.start());  // 已在运行：warn 后幂等 true
    mode.stop();
}

TEST_F(StandaloneModeCoverageTest, StartFailsWhenScriptDirOccupiedByRegularFile) {
    const auto blocker = dir_ / "blocker";
    { std::ofstream f(blocker); }
    StandaloneModeConfig config;
    config.scriptDir = blocker.string();  // 路径被普通文件占据 → create_directories 抛异常
    StandaloneMode mode(config);
    EXPECT_FALSE(mode.start());
}

TEST_F(StandaloneModeCoverageTest, AutoStartLoadsAndRunsQuickScript) {
    // 空 print() 经引擎输出回调送出空串（lua_script_engine_test 已钉），
    // StandaloneMode 的输出回调必须跳过空串、只转发非空输出。
    const auto script = writeScript("quick.lua",
                                    "print('hello standalone')\nprint()\n");
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    config.autoStart = {script.string()};
    StandaloneMode mode(config);
    ASSERT_TRUE(mode.start());

    const auto scripts = mode.listScripts();
    ASSERT_EQ(scripts.size(), 1u);
    const auto& info = scripts[0];
    EXPECT_NE(info.id.find("script_"), std::string::npos);
    EXPECT_EQ(info.path, script.string());

    const auto outputs = drainedEvents("script.output", info.id);
    ASSERT_EQ(outputs.size(), 1u) << "空串输出必须被跳过，非空输出恰好一条";
    EXPECT_EQ(outputs[0].value("output", ""), "hello standalone");

    // startScript 阻塞至脚本完成：completed 在 toRuntimeState 无映射 →
    // 现状钉 Unknown（arguably 应为 Stopped，登记为观察项不改动）
    EXPECT_EQ(mode.getScript(info.id).state, ScriptState::Unknown);
    mode.stop();
}

TEST_F(StandaloneModeCoverageTest, AutoStartSkipsMissingScript) {
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    config.autoStart = {(dir_ / "missing.lua").string()};
    StandaloneMode mode(config);
    ASSERT_TRUE(mode.start());
    EXPECT_TRUE(mode.listScripts().empty());
    mode.stop();
}

// ========== loadScript / unloadScript / startScript ==========

TEST_F(StandaloneModeCoverageTest, LoadScriptReportsStoppedUntilReload) {
    // 现状钉：manager.loadScript 登记条目但不置 loaded 态（ScriptInfo 默认
    // unloaded）→ 映射 Stopped；loaded 态仅在 reloadScript 后出现
    const auto script = writeScript("idle.lua", "x = 1\n");
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    const auto id = mode.loadScript(script.string());
    ASSERT_FALSE(id.empty());

    auto info = mode.getScript(id);
    EXPECT_EQ(info.id, id);
    EXPECT_EQ(info.path, script.string());
    EXPECT_EQ(info.state, ScriptState::Stopped);  // unloaded → Stopped（现状钉）
    EXPECT_TRUE(info.error.empty());

    ASSERT_TRUE(getScriptManager().reloadScript(id));  // reload 置 loaded
    info = mode.getScript(id);
    EXPECT_EQ(info.state, ScriptState::Loaded);  // loaded → Loaded
    mode.stop();
}

TEST_F(StandaloneModeCoverageTest, FailedScriptReportsErrorStateAndEvent) {
    const auto script = writeScript("boom.lua", "error('boom-stmt')\n");
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    ASSERT_TRUE(mode.start());  // 注册 error 事件回调（GUI 感知失败态的链路）
    const auto id = mode.loadScript(script.string());
    ASSERT_FALSE(id.empty());

    EXPECT_FALSE(mode.startScript(id));  // 执行失败：started=false，不推送 running
    const auto info = mode.getScript(id);
    EXPECT_EQ(info.state, ScriptState::Error);
    EXPECT_NE(info.error.find("boom-stmt"), std::string::npos);

    // 脚本级 error 经 start() 注册的事件回调推送（GUI 感知失败态的链路）
    const auto events = drainedEvents("script.state_changed", id);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].value("state", ""), "error");
    EXPECT_NE(events[0].value("error", "").find("boom-stmt"), std::string::npos);
    mode.stop();
}

TEST_F(StandaloneModeCoverageTest, StartScriptUnknownIdReturnsFalse) {
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    EXPECT_FALSE(mode.startScript("no-such-script"));
}

TEST_F(StandaloneModeCoverageTest, UnloadScriptFailsWhenManagerEntryAlreadyGone) {
    // 登记 map 与进程级 ScriptManager 可能失同步（此处直接卸掉 manager 条目
    // 模拟）：stopScript/unloadScript 双双落空 → 返回 false
    const auto script = writeScript("gone.lua", "x = 1\n");
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    const auto id = mode.loadScript(script.string());
    ASSERT_FALSE(id.empty());
    ASSERT_TRUE(getScriptManager().unloadScript(id));

    // 登记 map 命中但 manager 条目已失：getScript 回落默认 ScriptInfo
    EXPECT_EQ(mode.getScript(id).state, ScriptState::Unknown);

    EXPECT_FALSE(mode.unloadScript(id));
}

// ========== 批量操作对闲置脚本的契约：计数为 0 ==========

TEST_F(StandaloneModeCoverageTest, BulkOperationsCountZeroForIdleScripts) {
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    ASSERT_FALSE(mode.loadScript(writeScript("a.lua", "x = 1\n").string()).empty());
    ASSERT_FALSE(mode.loadScript(writeScript("b.lua", "x = 2\n").string()).empty());
    ASSERT_EQ(mode.listScripts().size(), 2u);

    // 闲置（loaded）脚本 pause/resume/stop 均失败 → 计数 0（对照用例：
    // 运行中脚本的批量计数增量见 BulkOperationsCountRunningScripts）
    EXPECT_EQ(mode.pauseAllScripts(), 0u);
    EXPECT_EQ(mode.resumeAllScripts(), 0u);
    EXPECT_EQ(mode.stopAllScripts(), 0u);
    mode.stop();
}

// ========== 状态机全链（2026-10-01 死锁环修复回归钉）==========

TEST_F(StandaloneModeCoverageTest, FullChainRunningPauseResumeStopReachable) {
    // 全链 start→running→pause→paused→resume→running→stop 此前不可达
    // （running 从不赋值——死锁环，见文件头登记）；修复后经 StandaloneMode
    // 编排面逐态驱动，running/paused 状态映射（toRuntimeState）与对应
    // state_changed 推送一并钉住。脚本用 wingman.timer.sleep 驻留（沙箱下
    // wingman 全局表可用），被停后执行线程睡完剩余时长自然退出。
    const auto script = writeScript("chain.lua",
                                    "for i = 1, 150 do wingman.timer.sleep(20) end\n");
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    ASSERT_TRUE(mode.start());  // 注册 manager error 事件回调
    const auto id = mode.loadScript(script.string());
    ASSERT_FALSE(id.empty());

    std::atomic<bool> runResult{true};
    std::thread runner([&] { runResult = mode.startScript(id); });

    ASSERT_TRUE(waitForRuntimeState(mode, id, ScriptState::Running));
    EXPECT_TRUE(mode.pauseScript(id));
    EXPECT_EQ(mode.getScript(id).state, ScriptState::Paused);
    EXPECT_TRUE(mode.resumeScript(id));
    EXPECT_EQ(mode.getScript(id).state, ScriptState::Running);
    EXPECT_TRUE(mode.stopScript(id));
    ASSERT_TRUE(waitForRuntimeState(mode, id, ScriptState::Loaded, 2000));

    runner.join();
    EXPECT_FALSE(runResult.load());  // 被停止的运行不报成功（也不推 running）

    // 事件序列：paused → running(resume) → stopped；startScript 因被停止
    // 返回 false 不推 running
    const auto events = drainedEvents("script.state_changed", id);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].value("state", ""), "paused");
    EXPECT_EQ(events[1].value("state", ""), "running");
    EXPECT_EQ(events[2].value("state", ""), "stopped");
    mode.stop();
}

TEST_F(StandaloneModeCoverageTest, BulkOperationsCountRunningScripts) {
    // 批量计数增量回归钉：此前 pauseAll/resumeAll/stopAll 的非零计数不可达
    // （running 不可达），任何脚本永远闲置计数 0。
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    const auto idA = mode.loadScript(
        writeScript("long_a.lua", "for i = 1, 100 do wingman.timer.sleep(20) end\n").string());
    const auto idB = mode.loadScript(
        writeScript("long_b.lua", "for i = 1, 100 do wingman.timer.sleep(20) end\n").string());
    ASSERT_FALSE(idA.empty());
    ASSERT_FALSE(idB.empty());

    std::atomic<bool> runA{true}, runB{true};
    std::thread runnerA([&] { runA = mode.startScript(idA); });
    std::thread runnerB([&] { runB = mode.startScript(idB); });

    ASSERT_TRUE(waitForRuntimeState(mode, idA, ScriptState::Running));
    ASSERT_TRUE(waitForRuntimeState(mode, idB, ScriptState::Running));

    EXPECT_EQ(mode.pauseAllScripts(), 2u);
    EXPECT_EQ(mode.getScript(idA).state, ScriptState::Paused);
    EXPECT_EQ(mode.getScript(idB).state, ScriptState::Paused);

    EXPECT_EQ(mode.resumeAllScripts(), 2u);
    EXPECT_EQ(mode.getScript(idA).state, ScriptState::Running);
    EXPECT_EQ(mode.getScript(idB).state, ScriptState::Running);

    EXPECT_EQ(mode.stopAllScripts(), 2u);
    ASSERT_TRUE(waitForRuntimeState(mode, idA, ScriptState::Loaded, 2000));
    ASSERT_TRUE(waitForRuntimeState(mode, idB, ScriptState::Loaded, 2000));

    runnerA.join();
    runnerB.join();
    EXPECT_FALSE(runA.load());
    EXPECT_FALSE(runB.load());
    mode.stop();
}

// ========== stop()：清理降级腿 ==========

TEST_F(StandaloneModeCoverageTest, StopToleratesScriptVanishedFromManager) {
    const auto script = writeScript("vanish.lua", "x = 1\n");
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    const auto id = mode.loadScript(script.string());
    ASSERT_FALSE(id.empty());
    ASSERT_TRUE(getScriptManager().unloadScript(id));

    mode.stop();  // stopScript/unloadScript 双失败只打日志，不抛不崩
    EXPECT_TRUE(mode.listScripts().empty());
}

// ========== 查询防御 / 配置镜像 ==========

TEST_F(StandaloneModeCoverageTest, GetScriptUnknownIdReturnsDefaultInfo) {
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    StandaloneMode mode(config);
    const auto info = mode.getScript("never-loaded");
    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.path.empty());
    EXPECT_EQ(info.state, ScriptState::Unknown);
}

TEST_F(StandaloneModeCoverageTest, GetConfigMirrorsConstructionConfig) {
    StandaloneModeConfig config;
    config.scriptDir = dir_.string();
    config.autoStart = {"/tmp/a.lua", "/tmp/b.lua"};
    StandaloneMode mode(config);
    const auto& mirrored = mode.getConfig();
    EXPECT_EQ(mirrored.scriptDir, dir_.string());
    ASSERT_EQ(mirrored.autoStart.size(), 2u);
    EXPECT_EQ(mirrored.autoStart[0], "/tmp/a.lua");
    EXPECT_EQ(mirrored.autoStart[1], "/tmp/b.lua");
}
