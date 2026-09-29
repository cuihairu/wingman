// StandaloneMode（standalone_mode.cpp）覆盖率缺口补测。
// 基线：78%（51 行未覆盖）——此前覆盖几乎全部来自 agent_loopback_test 的间接
// 驱动，本文件对单机模式的编排面直接补测：start 幂等/建目录失败、autoStart
// 装配（含输出回调空串跳过）、脚本登记与状态映射（loaded/error/completed
// 现状钉）、unknown-id 防御、pause/resume/stopAll 对闲置脚本的计数契约、
// manager 已侧卸载后的 stop/unload 降级腿、getConfig。
//
// 结构性不可达登记（不写假用例，根因见 CHANGELOG 本轮条目）：
// ScriptManager::runScriptInternal 从不赋值 ScriptState::running（全文件唯一
// 赋值点在 resumeScript，而 resume 前置要求 paused、pause 前置要求 running
// ——死锁环），因此 pause/resume 的成功腿、pauseAll/resumeAll/stopAll 的
// 非零计数、StandaloneMode 对应的 state_changed 推送均不可达；stopScript
// 成功腿仅当脚本处于 starting（runScript 阻塞等待中）时可达，但该路径会并发
// engine->shutdown()（销毁执行线程正在使用的 lua_State，数据竞争）——同
// agent_loopback_test 注记 2 的延迟口径，此处不触发。
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

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

    // 闲置（loaded）脚本 pause/resume/stop 均失败 → 计数 0（现状钉：
    // 非零计数需 running/paused 态，见文件头结构性不可达登记）
    EXPECT_EQ(mode.pauseAllScripts(), 0u);
    EXPECT_EQ(mode.resumeAllScripts(), 0u);
    EXPECT_EQ(mode.stopAllScripts(), 0u);
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
