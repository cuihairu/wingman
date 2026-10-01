#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script_manager.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <thread>

#ifdef WINGMAN_HAS_LUA
#include "wingman/lua/lua_script_engine.hpp"
#endif

using namespace wingman;

// script_manager.cpp 真实执行路径补测（2026-09-22 覆盖率第三批）：现有
// script_manager_test.cpp 全部为"不存在脚本"防御分支，runScriptInternal 的
// 引擎创建/线程执行/超时/错误路径与事件回调分发此前零覆盖。
// 本文件以临时 Lua 脚本文件走完整生命周期。引擎注册隐式契约：Lua 引擎由
// registerLuaEngine() 注册进 ScriptEngineFactory（runtime 在 main 里调用），
// 测试进程须同样注册——见 tests/CMakeLists.txt 对 wingman::lua 的可选链接。
// 2026-10-01 状态机修复后：pause/resume 的成功腿与 running/paused 态已可达
// （runScriptInternal 在执行线程起动后迁 running，此前唯一赋值点在
// resumeScript——死锁环）；仍不测 callFunction 的 running 路径：同步模型下
// running 窗口引擎正被执行线程独占，跨线程调用即数据竞争（无生产调用方，
// 见 script_manager.cpp 注释与 todo 登记）。

namespace {

std::string writeScript(const std::string& name, const std::string& content) {
    std::string path = std::string(::testing::TempDir()) + name;
    std::ofstream f(path);
    f << content;
    return path;
}

class ScriptManagerExecTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef WINGMAN_HAS_LUA
        // 幂等（内部 static registrar），与 runtime main 的调用等价
        wingman::lua::registerLuaEngine();
#endif
        mgr_.setOutputCallback([](const std::string&, const std::string&) {});
    }

    ScriptManager mgr_;
};

// ========== runScript 完整执行路径 ==========

TEST_F(ScriptManagerExecTest, RunSimpleScriptCompletes) {
    std::string path = writeScript("cov_ok.lua", "local x = 40\nreturn x + 2\n");
    ASSERT_TRUE(mgr_.loadScript("cov_ok", path));

    EXPECT_TRUE(mgr_.runScript("cov_ok"));

    auto info = mgr_.getScriptInfo("cov_ok");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::completed);
    EXPECT_TRUE(info->lastError.empty());

    // 完成后再次运行：engine 已存在则复用，不重建
    EXPECT_TRUE(mgr_.runScript("cov_ok"));
}

TEST_F(ScriptManagerExecTest, RunScriptRoutesOutputAndEnv) {
    std::string path = writeScript("cov_env.lua", "print(GREETING)\nprint(\"second line\")\n");
    ScriptConfig config;
    config.env["GREETING"] = "hello-cov";
    ASSERT_TRUE(mgr_.loadScript("cov_env", path, config));

    std::mutex mu;
    std::string collected;
    mgr_.setOutputCallback([&](const std::string& scriptName, const std::string& output) {
        std::lock_guard lock(mu);
        EXPECT_EQ(scriptName, "cov_env");
        collected += output;
    });

    ASSERT_TRUE(mgr_.runScript("cov_env"));
    std::lock_guard lock(mu);
    // env 变量经 setGlobal 注入为字符串，print 路由到 outputCallback
    EXPECT_NE(collected.find("hello-cov"), std::string::npos);
    EXPECT_NE(collected.find("second line"), std::string::npos);
}

TEST_F(ScriptManagerExecTest, RunScriptSyntaxErrorCapturesError) {
    std::string path = writeScript("cov_bad.lua", "this is not valid lua )(\n");
    ASSERT_TRUE(mgr_.loadScript("cov_bad", path));

    EXPECT_FALSE(mgr_.runScript("cov_bad"));

    auto info = mgr_.getScriptInfo("cov_bad");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::error);
    EXPECT_FALSE(info->lastError.empty());
}

TEST_F(ScriptManagerExecTest, RunScriptTimeoutMarksErrorAndReturnsFalse) {
    // 有限长循环（约数秒）+ 100ms 超时 → 超时路径触达且 detached 线程自然结束，
    // 不用 while true do end（detached 忙等线程会拖累全量测试）
    std::string path = writeScript("cov_slow.lua",
        "local n = 0\nfor i = 1, 500000000 do n = n + i end\nreturn n\n");
    ScriptConfig config;
    config.timeoutMs = 100;
    ASSERT_TRUE(mgr_.loadScript("cov_slow", path, config));

    EXPECT_FALSE(mgr_.runScript("cov_slow"));

    auto info = mgr_.getScriptInfo("cov_slow");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::error);
    EXPECT_NE(info->lastError.find("timeout"), std::string::npos);
}

TEST_F(ScriptManagerExecTest, EventCallbackFiresOnRunResults) {
    std::string okPath = writeScript("cov_ev_ok.lua", "return 1\n");
    std::string badPath = writeScript("cov_ev_bad.lua", ")(\n");
    ASSERT_TRUE(mgr_.loadScript("cov_ev_ok", okPath));
    ASSERT_TRUE(mgr_.loadScript("cov_ev_bad", badPath));

    std::mutex mu;
    std::vector<std::pair<std::string, ScriptEvent>> events;
    mgr_.setEventCallback([&](const std::string& name, ScriptEvent event, const std::string&) {
        std::lock_guard lock(mu);
        events.emplace_back(name, event);
    });

    ASSERT_TRUE(mgr_.runScript("cov_ev_ok"));
    ASSERT_FALSE(mgr_.runScript("cov_ev_bad"));

    std::lock_guard lock(mu);
    bool sawStarted = false, sawError = false;
    for (auto& [name, ev] : events) {
        if (name == "cov_ev_ok" && ev == ScriptEvent::started) sawStarted = true;
        if (name == "cov_ev_bad" && ev == ScriptEvent::error) sawError = true;
    }
    EXPECT_TRUE(sawStarted);
    EXPECT_TRUE(sawError);
}

// ========== reload 与生命周期 ==========

TEST_F(ScriptManagerExecTest, ReloadScriptResetsStateAndReruns) {
    std::string path = writeScript("cov_reload.lua", "return 7\n");
    ASSERT_TRUE(mgr_.loadScript("cov_reload", path));
    ASSERT_TRUE(mgr_.runScript("cov_reload"));

    EXPECT_TRUE(mgr_.reloadScript("cov_reload"));
    EXPECT_TRUE(mgr_.runScript("cov_reload"));

    auto info = mgr_.getScriptInfo("cov_reload");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::completed);

    EXPECT_TRUE(mgr_.unloadScript("cov_reload"));
    EXPECT_FALSE(mgr_.runScript("cov_reload")); // 卸载后运行拒绝
}

// ========== 状态机全链（2026-10-01 死锁环修复回归钉）==========

namespace {

// 轮询等待脚本进入期望状态（等待循环以 50ms 为周期收尾，超时上界只作失败时限）
bool waitForState(ScriptManager& mgr, const std::string& name, ScriptState expected,
                  int timeoutMs = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        auto info = mgr.getScriptInfo(name);
        if (info && info->state == expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

} // namespace

// wingman.timer.sleep 全平台可用（wingman 全局表不受沙箱 strip 影响），让脚本
// 驻留足够久以观察/操作中间态；被 stop 后执行线程睡完剩余时长自然退出，不烧 CPU。
TEST_F(ScriptManagerExecTest, PauseResumeStopFullChainIsReachable) {
    // 2026-09-29 登记的结构性缺陷回归钉：running 从不可达（唯一赋值点在
    // resumeScript，resume 前置 paused、pause 前置 running——死锁环），全链
    // start→running→pause→paused→resume→running→stop 不可达。修复：执行
    // 线程起动后迁 running；stop 改协作停止（stopRequested，不再 shutdown
    // 执行线程正在使用的引擎），终态 loaded。
    std::string path = writeScript("cov_chain.lua",
        "for i = 1, 150 do wingman.timer.sleep(20) end\n");
    ASSERT_TRUE(mgr_.loadScript("cov_chain", path));

    std::atomic<bool> runResult{true};
    std::thread runner([&] { runResult = mgr_.runScript("cov_chain"); });

    ASSERT_TRUE(waitForState(mgr_, "cov_chain", ScriptState::running));
    EXPECT_EQ(mgr_.getRunningScripts(), std::vector<std::string>({"cov_chain"}));

    // 重入防护：running 期间再次 run 拒绝（旧实现先 stop 再重启，会与执行
    // 线程并发复用同一引擎）
    EXPECT_FALSE(mgr_.runScript("cov_chain"));
    EXPECT_NE(mgr_.getScriptInfo("cov_chain")->lastError.find("already running"),
              std::string::npos);

    // pause：running → paused（簿记态；底层执行继续）
    EXPECT_TRUE(mgr_.pauseScript("cov_chain"));
    EXPECT_EQ(mgr_.getScriptInfo("cov_chain")->state, ScriptState::paused);
    EXPECT_TRUE(mgr_.getRunningScripts().empty());
    EXPECT_FALSE(mgr_.pauseScript("cov_chain")); // 重复 pause 拒绝
    // callFunction 前置 running：paused 态直接拒绝（不触碰引擎）
    EXPECT_FALSE(mgr_.callFunction("cov_chain", "nosuch"));

    // resume：paused → running
    EXPECT_TRUE(mgr_.resumeScript("cov_chain"));
    EXPECT_EQ(mgr_.getScriptInfo("cov_chain")->state, ScriptState::running);
    EXPECT_FALSE(mgr_.resumeScript("cov_chain")); // 重复 resume 拒绝

    // stop：running → stopping（等待循环 ≤50ms 内收尾）→ loaded
    EXPECT_TRUE(mgr_.stopScript("cov_chain"));
    const auto midState = mgr_.getScriptInfo("cov_chain")->state;
    EXPECT_TRUE(midState == ScriptState::stopping || midState == ScriptState::loaded);
    ASSERT_TRUE(waitForState(mgr_, "cov_chain", ScriptState::loaded, 2000));

    runner.join();
    EXPECT_FALSE(runResult.load()); // 被停止的运行不报成功

    const auto info = mgr_.getScriptInfo("cov_chain");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::loaded); // 停止终态：区别于 completed/error
    EXPECT_TRUE(info->lastError.empty());        // 停止不是错误
    EXPECT_EQ(info->engine, nullptr);            // manager 侧引擎引用已释放
}

TEST_F(ScriptManagerExecTest, RerunAfterStopCreatesFreshRun) {
    std::string path = writeScript("cov_rerun.lua",
        "for i = 1, 40 do wingman.timer.sleep(25) end\n");
    ASSERT_TRUE(mgr_.loadScript("cov_rerun", path));

    std::atomic<bool> firstRun{true};
    std::thread runner([&] { firstRun = mgr_.runScript("cov_rerun"); });
    ASSERT_TRUE(waitForState(mgr_, "cov_rerun", ScriptState::running));
    EXPECT_TRUE(mgr_.stopScript("cov_rerun"));
    ASSERT_TRUE(waitForState(mgr_, "cov_rerun", ScriptState::loaded, 2000));
    runner.join();
    EXPECT_FALSE(firstRun.load());

    // 停止收尾后重跑：loaded 不在重入防护集合内；旧引擎已由 detached 线程
    // 持有，新运行创建新引擎并完整执行至 completed
    EXPECT_TRUE(mgr_.runScript("cov_rerun"));
    EXPECT_EQ(mgr_.getScriptInfo("cov_rerun")->state, ScriptState::completed);
}

} // anonymous namespace
