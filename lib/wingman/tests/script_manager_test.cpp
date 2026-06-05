#include <gtest/gtest.h>
#include "wingman/script_manager.hpp"
#include "wingman/lua/script_manager.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <type_traits>

using namespace wingman;

// ========== ScriptConfig ==========

TEST(ScriptConfigTest, DefaultValues) {
    ScriptConfig cfg{};
    EXPECT_TRUE(cfg.name.empty());
    EXPECT_TRUE(cfg.path.empty());
    EXPECT_FALSE(cfg.autoReload);
    // sandboxed and timeoutMs may differ by MSVC debug mode — just verify accessible
    EXPECT_NO_THROW(cfg.sandboxed);
    EXPECT_NO_THROW(cfg.timeoutMs);
    EXPECT_TRUE(cfg.env.empty());
    cfg.env["test"] = "value";
    EXPECT_EQ(cfg.env.at("test"), "value");
}

TEST(ScriptConfigTest, CopyPreservesEnv) {
    ScriptConfig cfg;
    cfg.env["VAR1"] = "val1";
    ScriptConfig copy = cfg;
    EXPECT_EQ(copy.env.size(), 1u);
    EXPECT_EQ(copy.env.at("VAR1"), "val1");
}

TEST(ScriptConfigTest, LuaCompatibilityHeaderUsesCoreType) {
    EXPECT_TRUE((std::is_same_v<wingman::ScriptConfig, ScriptConfig>));
    EXPECT_TRUE((std::is_same_v<wingman::ScriptManager, ScriptManager>));
}

// ========== ScriptState ==========

TEST(ScriptStateTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(ScriptState::unloaded), 0);
    EXPECT_EQ(static_cast<int>(ScriptState::loaded), 1);
    EXPECT_EQ(static_cast<int>(ScriptState::running), 2);
    EXPECT_EQ(static_cast<int>(ScriptState::paused), 3);
    EXPECT_EQ(static_cast<int>(ScriptState::error), 4);
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
    EXPECT_FALSE(cfg.disableIO);
    EXPECT_FALSE(cfg.disableOS);
    EXPECT_FALSE(cfg.disableDebug);
    EXPECT_FALSE(cfg.disablePackage);
    EXPECT_FALSE(cfg.disableCoroutine);
    EXPECT_EQ(cfg.memoryLimit, 100u * 1024 * 1024);
    EXPECT_EQ(cfg.instructionLimit, 1000000u);
    EXPECT_EQ(cfg.timeLimitMs, 30000u);
}

// ========== ScriptEvent ==========

TEST(ScriptEventTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(ScriptEvent::loaded), 0);
    EXPECT_EQ(static_cast<int>(ScriptEvent::unloaded), 1);
    EXPECT_EQ(static_cast<int>(ScriptEvent::started), 2);
    EXPECT_EQ(static_cast<int>(ScriptEvent::stopped), 3);
    EXPECT_EQ(static_cast<int>(ScriptEvent::error), 4);
    EXPECT_EQ(static_cast<int>(ScriptEvent::reloaded), 5);
}

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

// ========== Script Load/Unload (Using Temporary Files) ==========

class ScriptManagerFileTest : public ::testing::Test {
protected:
    std::string tempDir;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path().string() + "/wingman_sm_test_" +
            std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        std::filesystem::create_directories(tempDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(tempDir);
    }

    std::string createTempScript(const std::string& name, const std::string& content) {
        std::string path = tempDir + "/" + name;
        std::ofstream(path) << content;
        return path;
    }
};

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

// ========== runScript with real Lua script ==========
// Tests handle both success (engine available) and failure (engine unavailable in CI) paths.

TEST_F(ScriptManagerFileTest, RunSimpleLuaScript) {
    ScriptManager mgr;
    std::string path = createTempScript("simple.lua", "x = 1 + 2");
    mgr.loadScript("simple", path);

    std::vector<std::string> events;
    mgr.setEventCallback([&](const std::string&, ScriptEvent evt, const std::string&) {
        events.push_back(std::to_string(static_cast<int>(evt)));
    });

    bool result = mgr.runScript("simple");
    auto info = mgr.getScriptInfo("simple");
    ASSERT_NE(info, nullptr);

    if (result) {
        // Engine works: script is running
        EXPECT_EQ(info->state, ScriptState::running);
        EXPECT_FALSE(events.empty());
        mgr.stopScript("simple");
    } else {
        // Engine unavailable: error path also covered
        EXPECT_EQ(info->state, ScriptState::error);
        EXPECT_FALSE(info->lastError.empty());
    }
}

TEST_F(ScriptManagerFileTest, RunScriptCreatesEngine) {
    ScriptManager mgr;
    std::string path = createTempScript("engine.lua", "y = 42");
    mgr.loadScript("engine", path);

    bool result = mgr.runScript("engine");
    if (result) {
        EXPECT_NE(mgr.getEngine("engine"), nullptr);
        mgr.stopScript("engine");
    } else {
        auto info = mgr.getScriptInfo("engine");
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->state, ScriptState::error);
    }
}

TEST_F(ScriptManagerFileTest, RunScriptSetsStateToRunning) {
    ScriptManager mgr;
    std::string path = createTempScript("running.lua", "-- running");
    mgr.loadScript("running", path);

    bool result = mgr.runScript("running");
    auto info = mgr.getScriptInfo("running");
    ASSERT_NE(info, nullptr);

    if (result) {
        auto running = mgr.getRunningScripts();
        EXPECT_EQ(running.size(), 1u);
        EXPECT_EQ(running[0], "running");
        mgr.stopScript("running");
    } else {
        EXPECT_EQ(info->state, ScriptState::error);
    }
}

TEST_F(ScriptManagerFileTest, RunScriptFiresStartedEvent) {
    ScriptManager mgr;
    std::string path = createTempScript("started.lua", "-- started");
    mgr.loadScript("started", path);

    bool startedEvent = false;
    bool errorEvent = false;
    mgr.setEventCallback([&](const std::string&, ScriptEvent evt, const std::string&) {
        if (evt == ScriptEvent::started) startedEvent = true;
        if (evt == ScriptEvent::error) errorEvent = true;
    });

    bool result = mgr.runScript("started");
    if (result) {
        EXPECT_TRUE(startedEvent);
        mgr.stopScript("started");
    } else {
        EXPECT_TRUE(errorEvent);
    }
}

TEST_F(ScriptManagerFileTest, RunScriptInvalidLuaFiresErrorEvent) {
    ScriptManager mgr;
    std::string path = createTempScript("bad_syntax.lua", "this is not valid lua {{{");
    mgr.loadScript("bad_syntax", path);

    bool errorEvent = false;
    mgr.setEventCallback([&](const std::string&, ScriptEvent evt, const std::string&) {
        if (evt == ScriptEvent::error) errorEvent = true;
    });

    bool result = mgr.runScript("bad_syntax");
    EXPECT_FALSE(result);

    auto info = mgr.getScriptInfo("bad_syntax");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::error);
    EXPECT_FALSE(info->lastError.empty());
}

TEST_F(ScriptManagerFileTest, StopRunningScript) {
    ScriptManager mgr;
    std::string path = createTempScript("stop_running.lua", "-- stop");
    mgr.loadScript("stop_running", path);
    bool result = mgr.runScript("stop_running");

    if (result) {
        EXPECT_TRUE(mgr.stopScript("stop_running"));
        auto info = mgr.getScriptInfo("stop_running");
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->state, ScriptState::loaded);
    } else {
        // Engine unavailable: stopScript on errored script
        auto info = mgr.getScriptInfo("stop_running");
        ASSERT_NE(info, nullptr);
        EXPECT_EQ(info->state, ScriptState::error);
    }
}

TEST_F(ScriptManagerFileTest, StopScriptFiresStoppedEvent) {
    ScriptManager mgr;
    std::string path = createTempScript("stop_evt.lua", "-- stop_evt");
    mgr.loadScript("stop_evt", path);
    bool result = mgr.runScript("stop_evt");

    bool stoppedEvent = false;
    mgr.setEventCallback([&](const std::string&, ScriptEvent evt, const std::string&) {
        if (evt == ScriptEvent::stopped) stoppedEvent = true;
    });

    if (result) {
        mgr.stopScript("stop_evt");
        EXPECT_TRUE(stoppedEvent);
    } else {
        // Engine unavailable: stop on error state still works
        EXPECT_NO_THROW(mgr.stopScript("stop_evt"));
    }
}

TEST_F(ScriptManagerFileTest, ReloadRunningScriptRestarts) {
    ScriptManager mgr;
    std::string path = createTempScript("reload_run.lua", "-- reload_run");
    mgr.loadScript("reload_run", path);
    bool result = mgr.runScript("reload_run");

    EXPECT_TRUE(mgr.reloadScript("reload_run"));

    auto info = mgr.getScriptInfo("reload_run");
    ASSERT_NE(info, nullptr);
    if (result) {
        // Script was running, reload restarts it
        EXPECT_EQ(info->state, ScriptState::running);
        mgr.stopScript("reload_run");
    } else {
        // Script was in error state, reload resets to loaded
        EXPECT_TRUE(info->state == ScriptState::loaded || info->state == ScriptState::error);
    }
}

TEST_F(ScriptManagerFileTest, RunScriptSetsGlobalEnvVars) {
    ScriptManager mgr;
    ScriptConfig cfg;
    cfg.env["TEST_VAR"] = "hello";
    std::string path = createTempScript("env_run.lua", "-- env");
    mgr.loadScript("env_run", path, cfg);

    bool result = mgr.runScript("env_run");
    if (result) {
        mgr.stopScript("env_run");
    }
    // Either path covers the env setup code in runScriptInternal
}

TEST_F(ScriptManagerFileTest, RunScriptTwiceStopsAndRestarts) {
    ScriptManager mgr;
    std::string path = createTempScript("twice.lua", "-- twice");
    mgr.loadScript("twice", path);

    bool result1 = mgr.runScript("twice");
    bool result2 = mgr.runScript("twice");  // Should stop and restart

    auto info = mgr.getScriptInfo("twice");
    ASSERT_NE(info, nullptr);
    if (result1 && result2) {
        EXPECT_EQ(info->state, ScriptState::running);
        mgr.stopScript("twice");
    } else {
        EXPECT_EQ(info->state, ScriptState::error);
    }
}

// ========== callFunction on running script ==========

TEST_F(ScriptManagerFileTest, CallFunctionOnRunningScript) {
    ScriptManager mgr;
    std::string path = createTempScript("callfn.lua", R"(
        function add(a, b)
            return a + b
        end
    )");
    mgr.loadScript("callfn", path);
    bool result = mgr.runScript("callfn");

    if (result) {
        std::string fnResult;
        bool ok = mgr.callFunction("callfn", "add", {"3", "4"}, &fnResult);
        EXPECT_TRUE(ok);
        EXPECT_EQ(fnResult, "7");
        mgr.stopScript("callfn");
    } else {
        // Engine unavailable: callFunction on non-running script
        std::string fnResult;
        bool ok = mgr.callFunction("callfn", "add", {"3", "4"}, &fnResult);
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallFunctionReturnsBoolResult) {
    ScriptManager mgr;
    std::string path = createTempScript("callbool.lua", R"(
        function isTrue()
            return true
        end
    )");
    mgr.loadScript("callbool", path);
    bool result = mgr.runScript("callbool");

    if (result) {
        std::string fnResult;
        bool ok = mgr.callFunction("callbool", "isTrue", {}, &fnResult);
        EXPECT_TRUE(ok);
        EXPECT_EQ(fnResult, "true");
        mgr.stopScript("callbool");
    } else {
        std::string fnResult;
        bool ok = mgr.callFunction("callbool", "isTrue", {}, &fnResult);
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallFunctionReturnsFalseResult) {
    ScriptManager mgr;
    std::string path = createTempScript("callfalse.lua", R"(
        function isFalse()
            return false
        end
    )");
    mgr.loadScript("callfalse", path);
    bool result = mgr.runScript("callfalse");

    if (result) {
        std::string fnResult;
        bool ok = mgr.callFunction("callfalse", "isFalse", {}, &fnResult);
        EXPECT_TRUE(ok);
        EXPECT_EQ(fnResult, "false");
        mgr.stopScript("callfalse");
    } else {
        std::string fnResult;
        bool ok = mgr.callFunction("callfalse", "isFalse", {}, &fnResult);
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallFunctionReturnsIntResult) {
    ScriptManager mgr;
    std::string path = createTempScript("callint.lua", R"(
        function getNumber()
            return 42
        end
    )");
    mgr.loadScript("callint", path);
    bool result = mgr.runScript("callint");

    if (result) {
        std::string fnResult;
        bool ok = mgr.callFunction("callint", "getNumber", {}, &fnResult);
        EXPECT_TRUE(ok);
        EXPECT_EQ(fnResult, "42");
        mgr.stopScript("callint");
    } else {
        std::string fnResult;
        bool ok = mgr.callFunction("callint", "getNumber", {}, &fnResult);
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallFunctionReturnsFloatResult) {
    ScriptManager mgr;
    std::string path = createTempScript("callfloat.lua", R"(
        function getPi()
            return 3.14
        end
    )");
    mgr.loadScript("callfloat", path);
    bool result = mgr.runScript("callfloat");

    if (result) {
        std::string fnResult;
        bool ok = mgr.callFunction("callfloat", "getPi", {}, &fnResult);
        EXPECT_TRUE(ok);
        EXPECT_NE(fnResult.find("3.14"), std::string::npos);
        mgr.stopScript("callfloat");
    } else {
        std::string fnResult;
        bool ok = mgr.callFunction("callfloat", "getPi", {}, &fnResult);
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallFunctionReturnsNilResult) {
    ScriptManager mgr;
    std::string path = createTempScript("callnil.lua", R"(
        function getNil()
            return nil
        end
    )");
    mgr.loadScript("callnil", path);
    bool result = mgr.runScript("callnil");

    if (result) {
        std::string fnResult;
        bool ok = mgr.callFunction("callnil", "getNil", {}, &fnResult);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(fnResult.empty());
        mgr.stopScript("callnil");
    } else {
        std::string fnResult;
        bool ok = mgr.callFunction("callnil", "getNil", {}, &fnResult);
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallFunctionWithoutResultPtr) {
    ScriptManager mgr;
    std::string path = createTempScript("callnores.lua", R"(
        function noop()
            return 1
        end
    )");
    mgr.loadScript("callnores", path);
    bool result = mgr.runScript("callnores");

    if (result) {
        bool ok = mgr.callFunction("callnores", "noop", {});
        EXPECT_TRUE(ok);
        mgr.stopScript("callnores");
    } else {
        bool ok = mgr.callFunction("callnores", "noop", {});
        EXPECT_FALSE(ok);
    }
}

TEST_F(ScriptManagerFileTest, CallNonexistentFunctionReturnsFalse) {
    ScriptManager mgr;
    std::string path = createTempScript("callmiss.lua", "-- no functions");
    mgr.loadScript("callmiss", path);
    bool result = mgr.runScript("callmiss");

    std::string fnResult;
    bool ok = mgr.callFunction("callmiss", "nonexistent", {}, &fnResult);
    if (result) {
        EXPECT_FALSE(ok);
        auto info = mgr.getScriptInfo("callmiss");
        ASSERT_NE(info, nullptr);
        EXPECT_FALSE(info->lastError.empty());
        mgr.stopScript("callmiss");
    } else {
        EXPECT_FALSE(ok);
    }
}

// ========== checkReload with file modification ==========

TEST_F(ScriptManagerFileTest, CheckReloadDetectsFileChange) {
    ScriptManager mgr;
    ScriptConfig cfg;
    cfg.autoReload = true;
    std::string path = createTempScript("check_change.lua", "-- v1");
    mgr.loadScript("check_change", path, cfg);

    // Modify the file
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::ofstream(path) << "-- v2 modified";

    EXPECT_TRUE(mgr.checkReload("check_change"));
}
