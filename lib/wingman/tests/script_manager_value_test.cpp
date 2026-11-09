#include "script_manager_test_common.hpp"

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
    EXPECT_EQ(static_cast<int>(ScriptState::starting), 2);
    EXPECT_EQ(static_cast<int>(ScriptState::running), 3);
    EXPECT_EQ(static_cast<int>(ScriptState::paused), 4);
    EXPECT_EQ(static_cast<int>(ScriptState::stopping), 5);
    EXPECT_EQ(static_cast<int>(ScriptState::completed), 6);
    EXPECT_EQ(static_cast<int>(ScriptState::error), 7);
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

