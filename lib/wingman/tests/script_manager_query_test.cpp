#include "script_manager_test_common.hpp"

// ========== ScriptManager Construction/Destruction ==========

TEST(ScriptManagerTest, ConstructionDoesNotCrash) {
    EXPECT_NO_THROW(ScriptManager mgr);
}

// ========== Configuration Management ==========

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

// ========== Sandbox Configuration ==========

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

// ========== Event Callbacks ==========

TEST(ScriptManagerTest, SetEventCallbackDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setEventCallback([](const std::string&, ScriptEvent, const std::string&) {}));
}

TEST(ScriptManagerTest, SetOutputCallbackDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setOutputCallback([](const std::string&, const std::string&) {}));
}

// ========== Script Queries (Empty State) ==========

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

// ========== Operations on Non-Existent Scripts ==========

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

// ========== Language Detection ==========

TEST(ScriptManagerTest, DetectLanguageLua) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("script.lua"), "lua");
}

// detectLanguage delegates to ScriptEngineFactory which only has Lua registered,
// so all unrecognized extensions fall back to "lua"
TEST(ScriptManagerTest, DetectLanguageFallbackToLua) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("script.py"), "lua");
    EXPECT_EQ(mgr.detectLanguage("script.js"), "lua");
    EXPECT_EQ(mgr.detectLanguage("script.unknown_ext"), "lua");
    EXPECT_EQ(mgr.detectLanguage("noscript"), "lua");
}

// ========== Available Languages ==========

TEST(ScriptManagerTest, GetAvailableLanguages) {
    ScriptManager mgr;
    auto langs = mgr.getAvailableLanguages();
    if (!langs.empty()) {
        EXPECT_NE(std::find(langs.begin(), langs.end(), "lua"), langs.end());
    }
}

// ========== Hot Reload Control ==========

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

// ========== Config File Loading ==========

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

