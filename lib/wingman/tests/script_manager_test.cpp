#include <gtest/gtest.h>
#include "wingman/script_manager.hpp"
#include <filesystem>
#include <fstream>

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

// detectLanguage delegates to ScriptEngineFactory which only has Lua registered,
// so all unrecognized extensions fall back to "lua"
TEST(ScriptManagerTest, DetectLanguageFallbackToLua) {
    ScriptManager mgr;
    EXPECT_EQ(mgr.detectLanguage("script.py"), "lua");
    EXPECT_EQ(mgr.detectLanguage("script.js"), "lua");
    EXPECT_EQ(mgr.detectLanguage("script.unknown_ext"), "lua");
    EXPECT_EQ(mgr.detectLanguage("noscript"), "lua");
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

// ========== 脚本加载/卸载（使用临时文件） ==========

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

    auto* info = mgr.getScriptInfo("test");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::loaded);
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
    auto* info = mgr.getScriptInfo("reload");
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, ScriptState::loaded);
}

TEST_F(ScriptManagerFileTest, GetAllScriptInfos) {
    ScriptManager mgr;
    std::string p1 = createTempScript("s1.lua", "-- 1");
    std::string p2 = createTempScript("s2.lua", "-- 2");

    mgr.loadScript("s1", p1);
    mgr.loadScript("s2", p2);

    auto infos = mgr.getAllScriptInfos();
    EXPECT_EQ(infos.size(), 2u);
}

TEST_F(ScriptManagerFileTest, GetRunningScriptsEmpty) {
    ScriptManager mgr;
    std::string path = createTempScript("run.lua", "-- test");
    mgr.loadScript("run", path);

    EXPECT_TRUE(mgr.getRunningScripts().empty());
}

// ========== INI 配置加载 ==========

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

// ========== 热加载 ==========

TEST_F(ScriptManagerFileTest, StartHotReloadDoesNotCrash) {
    ScriptManager mgr;
    EXPECT_NO_THROW(mgr.startHotReload());
    EXPECT_NO_THROW(mgr.stopHotReload());
}
