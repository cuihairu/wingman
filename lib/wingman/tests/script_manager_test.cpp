#include <gtest/gtest.h>
#include "wingman/script_manager.hpp"

using namespace wingman;

// ========== ScriptConfig ==========

TEST(ScriptConfigTest, DefaultValues) {
    ScriptConfig cfg;
    EXPECT_TRUE(cfg.name.empty());
    EXPECT_TRUE(cfg.path.empty());
    EXPECT_FALSE(cfg.autoReload);
    EXPECT_TRUE(cfg.sandboxed);
    EXPECT_EQ(cfg.timeoutMs, 30000);
    EXPECT_TRUE(cfg.env.empty());
}

// ========== ScriptState ==========

TEST(ScriptStateTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(ScriptState::unloaded), 0);
    EXPECT_NO_THROW(ScriptState s = ScriptState::loaded);
    EXPECT_NO_THROW(ScriptState s = ScriptState::running);
    EXPECT_NO_THROW(ScriptState s = ScriptState::paused);
    EXPECT_NO_THROW(ScriptState s = ScriptState::error);
}

// ========== ScriptInfo ==========

TEST(ScriptInfoTest, DefaultValues) {
    ScriptInfo info{};
    EXPECT_EQ(info.state, ScriptState::unloaded);
    EXPECT_TRUE(info.lastError.empty());
    EXPECT_EQ(info.lastModified, 0u);
    EXPECT_EQ(info.lastLoaded, 0u);
    EXPECT_TRUE(info.language.empty());
    EXPECT_TRUE(info.data.empty());
}

// ========== SandboxConfig ==========

TEST(SandboxConfigTest, DefaultValues) {
    SandboxConfig cfg;
    EXPECT_TRUE(cfg.disableIO);
    EXPECT_TRUE(cfg.disableOS);
    EXPECT_TRUE(cfg.disableDebug);
    EXPECT_TRUE(cfg.disablePackage);
    EXPECT_FALSE(cfg.disableCoroutine);
    EXPECT_EQ(cfg.memoryLimit, 100u * 1024 * 1024);
    EXPECT_EQ(cfg.instructionLimit, 1000000u);
    EXPECT_EQ(cfg.timeLimitMs, 30000u);
}

// ========== ScriptEvent ==========

TEST(ScriptEventTest, EnumValues) {
    EXPECT_NO_THROW(ScriptEvent e = ScriptEvent::loaded);
    EXPECT_NO_THROW(ScriptEvent e = ScriptEvent::unloaded);
    EXPECT_NO_THROW(ScriptEvent e = ScriptEvent::started);
    EXPECT_NO_THROW(ScriptEvent e = ScriptEvent::stopped);
    EXPECT_NO_THROW(ScriptEvent e = ScriptEvent::error);
    EXPECT_NO_THROW(ScriptEvent e = ScriptEvent::reloaded);
}

// ========== ScriptManager 构造/析构 ==========

TEST(ScriptManagerTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(ScriptManager mgr);
}

// ========== 配置管理 ==========

TEST(ScriptManagerTest, GetSetConfig) {
    ScriptManager mgr;
    mgr.setConfig("key1", "value1");
    EXPECT_EQ(mgr.getConfig("key1"), "value1");
    EXPECT_EQ(mgr.getConfig("nonexistent", "default"), "default");
    EXPECT_EQ(mgr.getConfig("nonexistent"), "");
}

TEST(ScriptManagerTest, GetSetEnv) {
    ScriptManager mgr;
    mgr.setEnv("HOME", "/home/user");
    EXPECT_EQ(mgr.getEnv("HOME"), "/home/user");
    EXPECT_EQ(mgr.getEnv("NONEXISTENT"), "");
}

// ========== 沙箱配置 ==========

TEST(ScriptManagerTest, SetGetSandboxConfig) {
    ScriptManager mgr;
    SandboxConfig cfg;
    cfg.memoryLimit = 50 * 1024 * 1024;
    cfg.timeLimitMs = 10000;
    mgr.setSandboxConfig(cfg);

    const auto& retrieved = mgr.getSandboxConfig();
    EXPECT_EQ(retrieved.memoryLimit, 50u * 1024 * 1024);
    EXPECT_EQ(retrieved.timeLimitMs, 10000u);
}

// ========== 事件回调 ==========

TEST(ScriptManagerTest, SetEventCallbackDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setEventCallback([](const std::string&, ScriptEvent, const std::string&) {}));
}

TEST(ScriptManagerTest, SetOutputCallbackDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setOutputCallback([](const std::string&, const std::string&) {}));
}

// ========== 脚本查询（空状态） ==========

TEST(ScriptManagerTest, NoScriptsInitially) {
    ScriptManager mgr;
    EXPECT_TRUE(mgr.getScriptNames().empty());
    EXPECT_TRUE(mgr.getRunningScripts().empty());
    EXPECT_TRUE(mgr.getAllScriptInfos().empty());
    EXPECT_FALSE(mgr.hasScript("nonexistent"));
}

TEST(ScriptManagerTest, GetScriptInfoReturnsNull) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.getScriptInfo("nonexistent"), nullptr);
}

TEST(ScriptManagerTest, GetEngineReturnsNull) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.getEngine("nonexistent"), nullptr);
}

// ========== 操作不存在的脚本 ==========

TEST(ScriptManagerTest, UnloadNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.unloadScript("nonexistent"));
}

TEST(ScriptManagerTest, ReloadNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.reloadScript("nonexistent"));
}

TEST(ScriptManagerTest, RunNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.runScript("nonexistent"));
}

TEST(ScriptManagerTest, StopNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.stopScript("nonexistent"));
}

TEST(ScriptManagerTest, PauseNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.pauseScript("nonexistent"));
}

TEST(ScriptManagerTest, ResumeNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.resumeScript("nonexistent"));
}

TEST(ScriptManagerTest, CallFunctionNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.callFunction("nonexistent", "func"));
}

// ========== 语言检测 ==========

TEST(ScriptManagerTest, DetectLanguageLua) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("script.lua"), "lua");
}

TEST(ScriptManagerTest, DetectLanguagePython) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("script.py"), "python");
}

TEST(ScriptManagerTest, DetectLanguageJavaScript) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("script.js"), "javascript");
}

TEST(ScriptManagerTest, DetectLanguageUnknown) {
    ScriptManager mgr;
    EXPECT_TRUE(mgr.detectLanguage("script.unknown_ext").empty());
}

// ========== 可用语言 ==========

TEST(ScriptManagerTest, GetAvailableLanguages) {
    ScriptManager mgr;
    auto langs = mgr.getAvailableLanguages();
    EXPECT_NO_THROW(mgr.getAvailableLanguages());
}

// ========== 热加载控制 ==========

TEST(ScriptManagerTest, SetAutoReloadNonexistentDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setAutoReload("nonexistent", true));
}

TEST(ScriptManagerTest, SetGlobalAutoReload) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setGlobalAutoReload(true));
    EXPECT_NO_THROW(mgr.setGlobalAutoReload(false));
}

TEST(ScriptManagerTest, StopHotReloadWithoutStartDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.stopHotReload());
}

// ========== 配置文件加载 ==========

TEST(ScriptManagerTest, LoadNonexistentConfig) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.loadConfig("/nonexistent/config.json"));
}

TEST(ScriptManagerTest, SaveConfigToInvalidPath) {
    ScriptManager mgr;
    mgr.setConfig("key", "value");
    EXPECT_FALSE(mgr.saveConfig("/nonexistent_dir_xyz/path.json"));
}

// ========== checkReload ==========

TEST(ScriptManagerTest, CheckReloadNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.checkReload("nonexistent"));
}

TEST(ScriptManagerTest, CheckAllReloadsDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.checkAllReloads());
}

// ========== logScriptOutput ==========

TEST(ScriptManagerTest, LogScriptOutputWithCallback) {
    ScriptManager mgr;
    std::string captured;
    mgr.setOutputCallback([&](const std::string& name, const std::string& output) {
        captured = name + ":" + output;
    });
    mgr.logScriptOutput("test_script", "hello");
    EXPECT_EQ(captured, "test_script:hello");
}

TEST(ScriptManagerTest, LogScriptOutputWithoutCallbackDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.logScriptOutput("test", "output"));
}
