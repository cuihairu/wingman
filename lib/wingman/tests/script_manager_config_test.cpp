#include "script_manager_test_common.hpp"

// ========== saveConfig to Valid Path ==========

TEST_F(ScriptManagerFileTest, SaveConfigToValidPath) {
    ScriptManager mgr;
    mgr.setConfig("alpha", "one");
    mgr.setConfig("beta", "two");

    std::string path = tempDir + "/saved_config.cfg";
    EXPECT_TRUE(mgr.saveConfig(path));
    EXPECT_TRUE(std::filesystem::exists(path));

    // Verify the file can be loaded back
    ScriptManager mgr2;
    EXPECT_TRUE(mgr2.loadConfig(path));
    EXPECT_EQ(mgr2.getConfig("alpha"), "one");
    EXPECT_EQ(mgr2.getConfig("beta"), "two");
}

TEST_F(ScriptManagerFileTest, SaveEmptyConfig) {
    ScriptManager mgr;
    std::string path = tempDir + "/empty_config.cfg";
    EXPECT_TRUE(mgr.saveConfig(path));
    EXPECT_TRUE(std::filesystem::exists(path));
}

// ========== triggerEvent ==========

TEST_F(ScriptManagerFileTest, TriggerEventWithCallbackOnLoadError) {
    ScriptManager mgr;
    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string& name, ScriptEvent evt, const std::string& msg) {
        events.push_back(name + ":" + std::to_string(static_cast<int>(evt)) + ":" + msg);
    });

    // Loading a nonexistent file triggers an error event
    mgr.loadScript("err_script", "/nonexistent/path.lua");
    ASSERT_FALSE(events.empty());
    EXPECT_NE(events[0].find("err_script:4"), std::string::npos);  // error=4
}

TEST_F(ScriptManagerFileTest, TriggerEventWithCallbackOnUnload) {
    ScriptManager mgr;
    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string& name, ScriptEvent evt, const std::string& msg) {
        events.push_back(std::to_string(static_cast<int>(evt)));
    });

    std::string path = createTempScript("trig.lua", "-- trig");
    mgr.loadScript("trig", path);
    mgr.unloadScript("trig");

    ASSERT_GE(events.size(), 2u);
    EXPECT_EQ(events[0], "0");  // loaded
    EXPECT_EQ(events[1], "1");  // unloaded
}

TEST_F(ScriptManagerFileTest, TriggerEventWithoutCallbackDoesNotCrash) {
    ScriptManager mgr;
    // No callback set — loadScript should still work
    std::string path = createTempScript("nocb.lua", "-- nocb");
    EXPECT_TRUE(mgr.loadScript("nocb", path));
}

// ========== checkReload with Auto-Reload ==========

TEST_F(ScriptManagerFileTest, CheckReloadWithAutoReloadEnabled) {
    ScriptManager mgr;
    ScriptConfig cfg;
    cfg.autoReload = true;
    std::string path = createTempScript("autoreload.lua", "-- v1");
    mgr.loadScript("autoreload", path, cfg);

    // checkReload should return false since file hasn't changed
    EXPECT_FALSE(mgr.checkReload("autoreload"));
}

TEST_F(ScriptManagerFileTest, CheckReloadWithGlobalAutoReload) {
    ScriptManager mgr;
    mgr.setGlobalAutoReload(true);
    std::string path = createTempScript("globalar.lua", "-- v1");
    mgr.loadScript("globalar", path);

    EXPECT_FALSE(mgr.checkReload("globalar"));
}

TEST_F(ScriptManagerFileTest, CheckAllReloadsWithScripts) {
    ScriptManager mgr;
    ScriptConfig cfg;
    cfg.autoReload = true;
    std::string path = createTempScript("checkall.lua", "-- v1");
    mgr.loadScript("checkall", path, cfg);

    EXPECT_NO_THROW(mgr.checkAllReloads());
}

// ========== getAllScriptInfos with Loaded Scripts ==========

TEST_F(ScriptManagerFileTest, GetAllScriptInfosAfterLoad) {
    ScriptManager mgr;
    std::string p1 = createTempScript("info1.lua", "-- info1");
    std::string p2 = createTempScript("info2.lua", "-- info2");
    mgr.loadScript("info1", p1);
    mgr.loadScript("info2", p2);

    auto infos = mgr.getAllScriptInfos();
    EXPECT_EQ(infos.size(), 2u);
    for (const auto& info : infos) {
        EXPECT_EQ(info.language, "lua");
        EXPECT_TRUE(info.lastError.empty());
    }
}

// ========== loadScript with Config ==========

TEST_F(ScriptManagerFileTest, LoadScriptWithTimeoutConfig) {
    ScriptManager mgr;
    ScriptConfig cfg;
    cfg.timeoutMs = 5000;
    cfg.sandboxed = true;
    std::string path = createTempScript("timeout.lua", "-- timeout");
    EXPECT_TRUE(mgr.loadScript("timeout", path, cfg));

    auto info = mgr.getScriptInfo("timeout");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->config.timeoutMs, 5000);
    EXPECT_TRUE(info->config.sandboxed);
}

// ========== loadScript Reloads Existing ==========

TEST_F(ScriptManagerFileTest, LoadScriptSameNameTriggersUnloadAndReload) {
    ScriptManager mgr;
    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string&, ScriptEvent evt, const std::string&) {
        events.push_back(std::to_string(static_cast<int>(evt)));
    });

    std::string p1 = createTempScript("reload_a.lua", "-- v1");
    std::string p2 = createTempScript("reload_b.lua", "-- v2");

    mgr.loadScript("dup", p1);
    mgr.loadScript("dup", p2);  // should unload old, load new

    ASSERT_GE(events.size(), 3u);
    EXPECT_EQ(events[0], "0");  // loaded
    EXPECT_EQ(events[1], "1");  // unloaded (from reload)
    EXPECT_EQ(events[2], "0");  // loaded again
}

// ========== Environment Variable System Lookup ==========

TEST_F(ScriptManagerFileTest, GetEnvFallsBackToSystemEnv) {
    ScriptManager mgr;
    // PATH should exist on all systems
    auto val = mgr.getEnv("PATH");
    EXPECT_FALSE(val.empty());
}

TEST_F(ScriptManagerFileTest, GetEnvCustomOverridesSystem) {
    ScriptManager mgr;
    mgr.setEnv("PATH", "custom_value");
    EXPECT_EQ(mgr.getEnv("PATH"), "custom_value");
}

TEST_F(ScriptManagerFileTest, GetEnvNonexistentReturnsEmpty) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.getEnv("WINGMAN_TOTALLY_NONEXISTENT_VAR_XYZ_12345"), "");
}

// ========== CFG Config Loading ==========

TEST_F(ScriptManagerFileTest, LoadCfgConfig) {
    ScriptManager mgr;
    std::string path = createTempScript("test.cfg", "host=localhost\nport=8080\n");

    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("host"), "localhost");
    EXPECT_EQ(mgr.getConfig("port"), "8080");
}

// ========== Pause/Resume Lifecycle ==========

TEST_F(ScriptManagerFileTest, PauseNonRunningReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("pause.lua", "-- test");
    mgr.loadScript("pause", path);

    // Script is loaded but not running, pause should fail
    EXPECT_FALSE(mgr.pauseScript("pause"));
}

TEST_F(ScriptManagerFileTest, ResumeNonPausedReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("resume.lua", "-- test");
    mgr.loadScript("resume", path);

    // Script is loaded but not paused, resume should fail
    EXPECT_FALSE(mgr.resumeScript("resume"));
}

TEST_F(ScriptManagerFileTest, StopNonRunningLoadedScriptReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("stop.lua", "-- test");
    mgr.loadScript("stop", path);

    // Script is loaded but not running, stop should fail
    EXPECT_FALSE(mgr.stopScript("stop"));
}

// ========== Script Info After Load ==========

TEST_F(ScriptManagerFileTest, ScriptInfoAfterLoad) {
    ScriptManager mgr;
    std::string path = createTempScript("info.lua", "-- test");
    mgr.loadScript("info", path);

    auto info = mgr.getScriptInfo("info");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->language, "lua");
    EXPECT_TRUE(info->lastError.empty());
    EXPECT_GT(info->lastModified, 0u);
}

// ========== SetAutoReload on Loaded Script ==========

TEST_F(ScriptManagerFileTest, SetAutoReloadOnLoadedScript) {
    ScriptManager mgr;
    std::string path = createTempScript("ar.lua", "-- test");
    mgr.loadScript("ar", path);

    EXPECT_NO_THROW(mgr.setAutoReload("ar", true));
    auto info = mgr.getScriptInfo("ar");
    ASSERT_NE(info, nullptr);
    EXPECT_TRUE(info->config.autoReload);

    EXPECT_NO_THROW(mgr.setAutoReload("ar", false));
    info = mgr.getScriptInfo("ar");
    ASSERT_NE(info, nullptr);
    EXPECT_FALSE(info->config.autoReload);
}

// ========== OutputCallback Invocation ==========

TEST_F(ScriptManagerFileTest, OutputCallbackReceivesOutput) {
    ScriptManager mgr;
    std::string capturedName;
    std::string capturedOutput;
    mgr.setOutputCallback([&](const std::string& name, const std::string& output) {
        capturedName = name;
        capturedOutput = output;
    });

    mgr.logScriptOutput("my_script", "hello world");
    EXPECT_EQ(capturedName, "my_script");
    EXPECT_EQ(capturedOutput, "hello world");
}

// ========== Hot Reload ==========

TEST_F(ScriptManagerFileTest, StartStopHotReloadDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.startHotReload());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_NO_THROW(mgr.stopHotReload());
}

TEST_F(ScriptManagerFileTest, StartHotReloadTwiceIsIdempotent) {
    ScriptManager mgr;
    mgr.startHotReload();
    mgr.startHotReload();  // Second call should be no-op
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mgr.stopHotReload();
}

TEST_F(ScriptManagerFileTest, StopHotReloadWhenNotStartedIsNoOp) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.stopHotReload());
}

// ========== Sandbox Config ==========

TEST_F(ScriptManagerFileTest, SetGetSandboxConfig) {
    ScriptManager mgr;
    SandboxConfig cfg;
    cfg.disableIO = true;
    cfg.disableOS = true;
    cfg.memoryLimit = 50 * 1024 * 1024;
    cfg.instructionLimit = 500000;
    cfg.timeLimitMs = 10000;
    mgr.setSandboxConfig(cfg);

    auto& retrieved = mgr.getSandboxConfig();
    EXPECT_TRUE(retrieved.disableIO);
    EXPECT_TRUE(retrieved.disableOS);
    EXPECT_EQ(retrieved.memoryLimit, 50u * 1024 * 1024);
    EXPECT_EQ(retrieved.instructionLimit, 500000u);
    EXPECT_EQ(retrieved.timeLimitMs, 10000u);
}

// ========== Detect Language ==========

TEST_F(ScriptManagerFileTest, DetectLanguageLua) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("test.lua"), "lua");
}

TEST_F(ScriptManagerFileTest, DetectLanguageUnknownExt) {
    ScriptManager mgr;
    // Unknown extension should default to "lua"
    EXPECT_EQ(mgr.detectLanguage("test.xyz"), "lua");
}

TEST_F(ScriptManagerFileTest, DetectLanguageNoExtension) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("noext"), "lua");
}

// ========== Available Languages ==========

TEST_F(ScriptManagerFileTest, GetAvailableLanguagesReturnsVector) {
    ScriptManager mgr;
    auto langs = mgr.getAvailableLanguages();
    // Engine registration depends on linked libraries; just verify no crash
    (void)langs;
}

// ========== Engine Access ==========

TEST_F(ScriptManagerFileTest, GetEngineNonexistentReturnsNull) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.getEngine("nonexistent"), nullptr);
}

// ========== Script Queries ==========

TEST_F(ScriptManagerFileTest, HasScriptAfterLoad) {
    ScriptManager mgr;
    std::string path = createTempScript("query.lua", "-- test");
    mgr.loadScript("query", path);
    EXPECT_TRUE(mgr.hasScript("query"));
    EXPECT_FALSE(mgr.hasScript("nonexistent"));
}

TEST_F(ScriptManagerFileTest, GetScriptNamesAfterLoad) {
    ScriptManager mgr;
    std::string p1 = createTempScript("n1.lua", "-- n1");
    std::string p2 = createTempScript("n2.lua", "-- n2");
    mgr.loadScript("n1", p1);
    mgr.loadScript("n2", p2);

    auto names = mgr.getScriptNames();
    EXPECT_EQ(names.size(), 2u);
}

// ========== callFunction on Non-Running Script ==========

TEST_F(ScriptManagerFileTest, CallFunctionOnLoadedNotRunningReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("cf.lua", "-- test");
    mgr.loadScript("cf", path);

    std::string result;
    EXPECT_FALSE(mgr.callFunction("cf", "myFunc", {}, &result));
}

TEST_F(ScriptManagerFileTest, CallFunctionNonexistentScriptReturnsFalse) {
    ScriptManager mgr;
    std::string result;
    EXPECT_FALSE(mgr.callFunction("nope", "func", {}, &result));
}

// ========== runScript Nonexistent ==========

TEST_F(ScriptManagerFileTest, RunScriptNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.runScript("nonexistent"));
}

// ========== reloadScript ==========

TEST_F(ScriptManagerFileTest, ReloadScriptNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.reloadScript("nonexistent"));
}

TEST_F(ScriptManagerFileTest, ReloadScriptLoadedReturnsTrue) {
    ScriptManager mgr;
    std::string path = createTempScript("reload.lua", "-- v1");
    mgr.loadScript("reload", path);
    EXPECT_TRUE(mgr.reloadScript("reload"));
}

// ========== unloadScript Nonexistent ==========

TEST_F(ScriptManagerFileTest, UnloadScriptNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.unloadScript("nonexistent"));
}

// ========== checkReload Nonexistent ==========

TEST_F(ScriptManagerFileTest, CheckReloadNonexistentReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.checkReload("nonexistent"));
}

// ========== INI Config with Comments ==========

TEST_F(ScriptManagerFileTest, LoadIniConfigWithComments) {
    ScriptManager mgr;
    std::string content = "; comment\n# another comment\nkey1=val1\n\nkey2 = val2\n";
    std::string path = createTempScript("comments.ini", content);
    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("key1"), "val1");
    EXPECT_EQ(mgr.getConfig("key2"), "val2");
}

// ========== loadConfig Empty Path ==========

TEST_F(ScriptManagerFileTest, LoadConfigEmptyPathReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.loadConfig(""));
}

// ========== loadConfig Nonexistent File ==========

TEST_F(ScriptManagerFileTest, LoadConfigNonexistentFileReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.loadConfig("/nonexistent/config.json"));
}

// ========== saveConfig to Invalid Path ==========

TEST_F(ScriptManagerFileTest, SaveConfigToInvalidPathReturnsFalse) {
    ScriptManager mgr;
    mgr.setConfig("key", "value");
    // Saving to a path with non-existent parent directory
    EXPECT_FALSE(mgr.saveConfig(""));
}

// ========== setGlobalAutoReload ==========

TEST_F(ScriptManagerFileTest, SetGlobalAutoReloadToggle) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.setGlobalAutoReload(true));
    EXPECT_NO_THROW(mgr.setGlobalAutoReload(false));
}

// ========== loadScript with Env Vars ==========

TEST_F(ScriptManagerFileTest, LoadScriptWithEnvVars) {
    ScriptManager mgr;
    ScriptConfig cfg;
    cfg.env["MY_VAR"] = "my_value";
    cfg.env["OTHER"] = "123";
    std::string path = createTempScript("env.lua", "-- env");
    EXPECT_TRUE(mgr.loadScript("env", path, cfg));

    auto info = mgr.getScriptInfo("env");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->config.env.at("MY_VAR"), "my_value");
    EXPECT_EQ(info->config.env.at("OTHER"), "123");
}

// ========== ScriptManager Destructor with Loaded Scripts ==========

TEST_F(ScriptManagerFileTest, DestructorWithLoadedScriptsDoesNotCrash) {
    std::string path = createTempScript("destr.lua", "-- test");
    {
        ScriptManager mgr;
        mgr.loadScript("destr", path);
    }
    // Destructor should clean up without crashing
}

// ========== loadScript Same Name Reload Event ==========

TEST_F(ScriptManagerFileTest, LoadScriptSameNameWithCallback) {
    ScriptManager mgr;
    int loadCount = 0;
    int unloadCount = 0;
    mgr.setEventCallback([&](const std::string&, ScriptEvent evt, const std::string&) {
        if (evt == ScriptEvent::loaded) loadCount++;
        if (evt == ScriptEvent::unloaded) unloadCount++;
    });

    std::string p1 = createTempScript("dup1.lua", "-- v1");
    std::string p2 = createTempScript("dup2.lua", "-- v2");

    mgr.loadScript("dup", p1);
    EXPECT_EQ(loadCount, 1);
    EXPECT_EQ(unloadCount, 0);

    mgr.loadScript("dup", p2);
    EXPECT_EQ(loadCount, 2);
    EXPECT_EQ(unloadCount, 1);
}
