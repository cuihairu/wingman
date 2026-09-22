#include <gtest/gtest.h>
#include "test_helpers.hpp" // 静态初始化 WINGMAN_CONFIG_DIR -> 临时目录，避免污染真实 config
#include "wingman/script_manager.hpp"
#include <atomic>
#include <fstream>
#include <mutex>

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
// 已知不可达分支（不硬凑，见 CHANGELOG）：callFunction/pauseScript/
// resumeScript 的 running 状态路径——同步执行模型下无任何入口把脚本置为
// running（runScript 同步等待完成后即 completed），属异步执行模型的遗留。

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

} // anonymous namespace
