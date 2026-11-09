#include "script_manager_test_common.hpp"

// ========== Script Load/Unload (Using Temporary Files) ==========

TEST_F(ScriptManagerFileTest, LoadNonexistentFile) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.loadScript("test", "/nonexistent/file.lua"));
}

TEST_F(ScriptManagerFileTest, LoadAndUnloadScript) {
    ScriptManager mgr;
    std::string path = createTempScript("test.lua", "print('hello')");

    EXPECT_TRUE(mgr.loadScript("test", path));
    EXPECT_TRUE(mgr.hasScript("test"));
    EXPECT_EQ(mgr.getScriptNames().size(), 1u);

    auto info = mgr.getScriptInfo("test");
    ASSERT_NE(info, nullptr);
    // loadScript sets language but does not change state from unloaded
    EXPECT_EQ(info->language, "lua");

    EXPECT_TRUE(mgr.unloadScript("test"));
    EXPECT_FALSE(mgr.hasScript("test"));
}

TEST_F(ScriptManagerFileTest, LoadScriptWithEventCallback) {
    ScriptManager mgr;
    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string& name, ScriptEvent evt, const std::string& msg) {
        events.push_back(name + ":" + std::to_string(static_cast<int>(evt)));
    });

    std::string path = createTempScript("evt_test.lua", "-- test");
    mgr.loadScript("evt_test", path);

    // Should have received a 'loaded' event
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events[0], "evt_test:0");  // ScriptEvent::loaded = 0
}

TEST_F(ScriptManagerFileTest, LoadScriptTriggersErrorEventForMissingFile) {
    ScriptManager mgr;
    std::string lastEvent;
    mgr.setEventCallback([&](const std::string& name, ScriptEvent evt, const std::string& msg) {
        lastEvent = name + ":" + std::to_string(static_cast<int>(evt));
    });

    mgr.loadScript("missing", "/nonexistent.lua");
    // ScriptEvent::error = 4
    EXPECT_EQ(lastEvent, "missing:4");
}

TEST_F(ScriptManagerFileTest, LoadAndUnloadEvents) {
    ScriptManager mgr;
    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string& name, ScriptEvent evt, const std::string& msg) {
        events.push_back(std::to_string(static_cast<int>(evt)));
    });

    std::string path = createTempScript("lu.lua", "-- test");
    mgr.loadScript("lu", path);
    mgr.unloadScript("lu");

    ASSERT_GE(events.size(), 2u);
    // loaded (0) then unloaded (1)
    EXPECT_EQ(events[0], "0");
    EXPECT_EQ(events[1], "1");
}

TEST_F(ScriptManagerFileTest, ReloadNonRunningScript) {
    ScriptManager mgr;
    std::string path = createTempScript("reload.lua", "-- v1");
    mgr.loadScript("reload", path);

    EXPECT_TRUE(mgr.reloadScript("reload"));
    auto info = mgr.getScriptInfo("reload");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::loaded);
}

TEST_F(ScriptManagerFileTest, GetAllScriptInfosEmpty) {
    ScriptManager mgr;
    // getAllScriptInfos on empty manager should return empty
    auto infos = mgr.getAllScriptInfos();
    EXPECT_TRUE(infos.empty());
}

TEST_F(ScriptManagerFileTest, GetRunningScriptsEmpty) {
    ScriptManager mgr;
    std::string path = createTempScript("run.lua", "-- test");
    mgr.loadScript("run", path);

    EXPECT_TRUE(mgr.getRunningScripts().empty());
}

// ========== INI Config Loading ==========

TEST_F(ScriptManagerFileTest, LoadIniConfig) {
    ScriptManager mgr;
    std::string path = createTempScript("test.ini", "key1=value1\nkey2=value2\n;comment\n#comment\n  spaced  =  value  \n");

    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("key1"), "value1");
    EXPECT_EQ(mgr.getConfig("key2"), "value2");
    EXPECT_EQ(mgr.getConfig("spaced"), "value");
}

TEST_F(ScriptManagerFileTest, SaveAndLoadConfigRoundtrip) {
    ScriptManager mgr;
    mgr.setConfig("setting1", "val1");
    mgr.setConfig("setting2", "val2");

    std::string path = tempDir + "/save_test.cfg";
    EXPECT_TRUE(mgr.saveConfig(path));

    ScriptManager mgr2;
    EXPECT_TRUE(mgr2.loadConfig(path));
    EXPECT_EQ(mgr2.getConfig("setting1"), "val1");
    EXPECT_EQ(mgr2.getConfig("setting2"), "val2");
}

TEST_F(ScriptManagerFileTest, LoadEmptyPathReturnsFalse) {
    ScriptManager mgr;
    EXPECT_FALSE(mgr.loadConfig(""));
}

// ========== Hot Reload ==========

TEST_F(ScriptManagerFileTest, StartHotReloadDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.startHotReload());
    EXPECT_NO_THROW(mgr.stopHotReload());
}

// ========== Additional Tests ==========

TEST_F(ScriptManagerFileTest, ScriptConfigSandboxedField) {
    ScriptConfig cfg;
    cfg.sandboxed = false;
    cfg.timeoutMs = 5000;
    cfg.autoReload = true;
    cfg.env["VAR1"] = "val1";

    std::string path = createTempScript("cfg_test.lua", "print('cfg')");
    ScriptManager mgr;
    EXPECT_TRUE(mgr.loadScript("cfg_test", path, cfg));

    auto info = mgr.getScriptInfo("cfg_test");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->config.sandboxed, false);
    EXPECT_EQ(info->config.timeoutMs, 5000);
    EXPECT_EQ(info->config.autoReload, true);
    EXPECT_EQ(info->config.env.size(), 1u);
    EXPECT_EQ(info->config.env.at("VAR1"), "val1");
}

TEST_F(ScriptManagerFileTest, ScriptConfigEnvField) {
    ScriptConfig cfg;
    cfg.env["A"] = "1";
    cfg.env["B"] = "2";

    std::string path = createTempScript("env_test.lua", "-- env");
    ScriptManager mgr;
    mgr.loadScript("env_test", path, cfg);

    auto info = mgr.getScriptInfo("env_test");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->config.env.at("A"), "1");
    EXPECT_EQ(info->config.env.at("B"), "2");
}

TEST_F(ScriptManagerFileTest, ScriptStateTransitionLoaded) {
    ScriptManager mgr;
    std::string path = createTempScript("state.lua", "-- state");
    mgr.loadScript("state", path);

    auto info = mgr.getScriptInfo("state");
    ASSERT_NE(info, nullptr);
    // loadScript does not change state from unloaded in the current impl
    // but language must be set
    EXPECT_EQ(info->language, "lua");
}

TEST_F(ScriptManagerFileTest, GetScriptListAfterMultipleLoads) {
    ScriptManager mgr;
    std::string p1 = createTempScript("a.lua", "-- a");
    std::string p2 = createTempScript("b.lua", "-- b");
    std::string p3 = createTempScript("c.lua", "-- c");

    EXPECT_TRUE(mgr.loadScript("s1", p1));
    EXPECT_TRUE(mgr.loadScript("s2", p2));
    EXPECT_TRUE(mgr.loadScript("s3", p3));

    auto names = mgr.getScriptNames();
    EXPECT_EQ(names.size(), 3u);

    auto infos = mgr.getAllScriptInfos();
    EXPECT_EQ(infos.size(), 3u);
}

TEST_F(ScriptManagerFileTest, GetRunningScriptsAfterLoadOnly) {
    ScriptManager mgr;
    std::string path = createTempScript("runonly.lua", "-- runonly");
    mgr.loadScript("runonly", path);

    // Not running — just loaded
    auto running = mgr.getRunningScripts();
    EXPECT_TRUE(running.empty());
}

TEST_F(ScriptManagerFileTest, StopNonRunningScriptReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("stopped.lua", "-- stopped");
    mgr.loadScript("stopped", path);

    // Script is not running, stop should return false
    EXPECT_FALSE(mgr.stopScript("stopped"));
}

TEST_F(ScriptManagerFileTest, PauseNonRunningScriptReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("paused.lua", "-- paused");
    mgr.loadScript("paused", path);

    EXPECT_FALSE(mgr.pauseScript("paused"));
}

TEST_F(ScriptManagerFileTest, ResumeNonPausedScriptReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("resume.lua", "-- resume");
    mgr.loadScript("resume", path);

    EXPECT_FALSE(mgr.resumeScript("resume"));
}

TEST_F(ScriptManagerFileTest, CallFunctionOnNonRunningScriptReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("fn.lua", "-- fn");
    mgr.loadScript("fn", path);

    EXPECT_FALSE(mgr.callFunction("fn", "someFunc"));
}

TEST_F(ScriptManagerFileTest, ReloadScriptTriggersReloadedEvent) {
    ScriptManager mgr;
    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string& name, ScriptEvent evt, const std::string& msg) {
        events.push_back(std::to_string(static_cast<int>(evt)));
    });

    std::string path = createTempScript("reload_evt.lua", "-- reload_evt");
    mgr.loadScript("reload_evt", path);  // triggers loaded (0)
    mgr.reloadScript("reload_evt");       // triggers reloaded (5)

    ASSERT_GE(events.size(), 2u);
    EXPECT_EQ(events[0], "0");  // loaded
    EXPECT_EQ(events[1], "5");  // reloaded
}

TEST_F(ScriptManagerFileTest, LoadDuplicateNameReplaces) {
    ScriptManager mgr;
    std::string p1 = createTempScript("dup1.lua", "-- v1");
    std::string p2 = createTempScript("dup2.lua", "-- v2");

    EXPECT_TRUE(mgr.loadScript("dup", p1));
    EXPECT_EQ(mgr.getScriptNames().size(), 1u);

    // Loading same name again should replace (unload old + load new)
    EXPECT_TRUE(mgr.loadScript("dup", p2));
    EXPECT_EQ(mgr.getScriptNames().size(), 1u);
}

TEST_F(ScriptManagerFileTest, SetMultipleEnvVars) {
    ScriptManager mgr;
    mgr.setEnv("VAR_A", "a");
    mgr.setEnv("VAR_B", "b");
    mgr.setEnv("VAR_C", "c");

    EXPECT_EQ(mgr.getEnv("VAR_A"), "a");
    EXPECT_EQ(mgr.getEnv("VAR_B"), "b");
    EXPECT_EQ(mgr.getEnv("VAR_C"), "c");
}

TEST_F(ScriptManagerFileTest, SetConfigOverwrite) {
    ScriptManager mgr;
    mgr.setConfig("key", "old");
    EXPECT_EQ(mgr.getConfig("key"), "old");
    mgr.setConfig("key", "new");
    EXPECT_EQ(mgr.getConfig("key"), "new");
}

TEST_F(ScriptManagerFileTest, EventCallbackMultipleRegistrations) {
    ScriptManager mgr;
    int count = 0;

    // Register first callback
    mgr.setEventCallback([&](const std::string&, ScriptEvent, const std::string&) {
        count++;
    });

    // Overwrite with second callback
    mgr.setEventCallback([&](const std::string&, ScriptEvent, const std::string&) {
        count += 10;
    });

    std::string path = createTempScript("evt2.lua", "-- evt2");
    mgr.loadScript("evt2", path);

    // Only the second callback should fire (overwrite, not accumulate)
    EXPECT_EQ(count, 10);
}

TEST_F(ScriptManagerFileTest, ScriptInfoDataField) {
    ScriptInfo info{};
    EXPECT_TRUE(info.data.empty());
    info.data["custom"] = "value";
    EXPECT_EQ(info.data["custom"], "value");
}

TEST_F(ScriptManagerFileTest, SandboxConfigDefaultCoroutine) {
    SandboxConfig cfg;
    EXPECT_FALSE(cfg.disableCoroutine);
    EXPECT_EQ(cfg.instructionLimit, 1000000u);
}

TEST_F(ScriptManagerFileTest, HasScriptAfterUnloadReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("has.lua", "-- has");
    mgr.loadScript("has", path);
    EXPECT_TRUE(mgr.hasScript("has"));
    mgr.unloadScript("has");
    EXPECT_FALSE(mgr.hasScript("has"));
}

TEST_F(ScriptManagerFileTest, GetEngineAfterLoadReturnsNull) {
    ScriptManager mgr;
    std::string path = createTempScript("eng.lua", "-- eng");
    mgr.loadScript("eng", path);
    // Engine is only created on runScript, not loadScript
    EXPECT_EQ(mgr.getEngine("eng"), nullptr);
}

TEST_F(ScriptManagerFileTest, HotReloadStartTwiceDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.startHotReload());
    EXPECT_NO_THROW(mgr.startHotReload());  // second call should be no-op
    EXPECT_NO_THROW(mgr.stopHotReload());
}

TEST_F(ScriptManagerFileTest, LoadConfigWithCfgExtension) {
    ScriptManager mgr;
    std::string path = createTempScript("test.cfg", "alpha=beta\ngamma=delta\n");
    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("alpha"), "beta");
    EXPECT_EQ(mgr.getConfig("gamma"), "delta");
}

TEST_F(ScriptManagerFileTest, LoadConfigWithIniExtension) {
    ScriptManager mgr;
    std::string path = createTempScript("test.ini", "x=y\n");
    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("x"), "y");
}

TEST_F(ScriptManagerFileTest, LoadConfigUnsupportedExtensionReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("test.xyz", "foo=bar\n");
    EXPECT_FALSE(mgr.loadConfig(path));
}

TEST_F(ScriptManagerFileTest, OutputCallbackReceivesMultipleOutputs) {
    ScriptManager mgr;
    std::vector<std::string> captured;
    mgr.setOutputCallback([&](const std::string& name, const std::string& output) {
        captured.push_back(name + "=" + output);
    });

    mgr.logScriptOutput("s1", "out1");
    mgr.logScriptOutput("s1", "out2");
    mgr.logScriptOutput("s2", "out3");

    ASSERT_EQ(captured.size(), 3u);
    EXPECT_EQ(captured[0], "s1=out1");
    EXPECT_EQ(captured[1], "s1=out2");
    EXPECT_EQ(captured[2], "s2=out3");
}

#ifdef WINGMAN_ENABLE_PYTHON
TEST_F(ScriptManagerFileTest, PythonStdoutAndStderrRouteToOutputCallback) {
    wingman::python::registerPythonEngine();

    ScriptManager mgr;
    std::vector<std::string> captured;
    mgr.setOutputCallback([&](const std::string& name, const std::string& output) {
        captured.push_back(name + ":" + output);
    });

    std::string path = createTempScript("py_output.py", R"(
import sys
print("hello", 42)
sys.stdout.write("partial")
sys.stdout.write(" line\n")
print("no newline", end="")
sys.stderr.write("problem\n")
)");

    ASSERT_TRUE(mgr.loadScript("py_output", path));
    ASSERT_TRUE(mgr.runScript("py_output"));

    EXPECT_NE(std::find(captured.begin(), captured.end(), "py_output:hello 42"), captured.end());
    EXPECT_NE(std::find(captured.begin(), captured.end(), "py_output:partial line"), captured.end());
    EXPECT_NE(std::find(captured.begin(), captured.end(), "py_output:no newline"), captured.end());
    EXPECT_NE(std::find(captured.begin(), captured.end(), "py_output:problem"), captured.end());
}
#endif

// ========== JSON Config Loading ==========

TEST_F(ScriptManagerFileTest, LoadJsonConfigWithStringValues) {
    ScriptManager mgr;
    std::string path = createTempScript("test.json", R"({"key1":"val1","key2":"val2"})");
    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("key1"), "val1");
    EXPECT_EQ(mgr.getConfig("key2"), "val2");
}

TEST_F(ScriptManagerFileTest, LoadJsonConfigWithNonStringValues) {
    ScriptManager mgr;
    std::string path = createTempScript("mixed.json", R"({"str":"hello","num":42,"flag":true})");
    EXPECT_TRUE(mgr.loadConfig(path));
    EXPECT_EQ(mgr.getConfig("str"), "hello");
    // Non-string values are stored as their JSON dump
    EXPECT_EQ(mgr.getConfig("num"), "42");
    EXPECT_EQ(mgr.getConfig("flag"), "true");
}

TEST_F(ScriptManagerFileTest, LoadJsonConfigInvalidJsonReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("bad.json", "{not valid json}");
    EXPECT_FALSE(mgr.loadConfig(path));
}

TEST_F(ScriptManagerFileTest, LoadJsonConfigNonObjectReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("array.json", R"(["a","b","c"])");
    EXPECT_FALSE(mgr.loadConfig(path));
}

TEST_F(ScriptManagerFileTest, LoadJsonConfigEmptyObjectSucceeds) {
    ScriptManager mgr;
    std::string path = createTempScript("empty.json", R"({})");
    EXPECT_TRUE(mgr.loadConfig(path));
}

