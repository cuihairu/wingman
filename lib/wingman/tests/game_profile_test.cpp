#include <gtest/gtest.h>
#include "wingman/game_profile.hpp"

using namespace wingman;

// ========== GameProfile 结构体 ==========

TEST(GameProfileTest, DefaultValues) {
    GameProfile profile;
    EXPECT_TRUE(profile.id.empty());
    EXPECT_TRUE(profile.name.empty());
    EXPECT_TRUE(profile.version.empty());
    EXPECT_TRUE(profile.colors.empty());
    EXPECT_TRUE(profile.images.empty());
    EXPECT_TRUE(profile.triggers.empty());
    EXPECT_TRUE(profile.scripts.empty());
    EXPECT_TRUE(profile.settings.empty());
}

TEST(GameProfileTest, WindowConfig) {
    GameProfile profile;
    profile.window.title = "TestGame*";
    profile.window.className = "GameWnd";
    profile.window.processName = "game.exe";
    profile.window.exactMatch = false;
    profile.window.fullscreen = true;

    EXPECT_EQ(profile.window.title, "TestGame*");
    EXPECT_TRUE(profile.window.fullscreen);
}

TEST(GameProfileTest, ColorConfig) {
    ColorConfig color;
    color.name = "health_red";
    color.r = 255;
    color.g = 0;
    color.b = 0;
    color.tolerance = 15;

    EXPECT_EQ(color.name, "health_red");
    EXPECT_EQ(color.r, 255);
    EXPECT_EQ(color.tolerance, 15);
}

TEST(GameProfileTest, ImageConfig) {
    ImageConfig img;
    img.name = "btn_start";
    img.path = "images/start.png";
    img.threshold = 0.95;
    img.preload = false;

    EXPECT_EQ(img.name, "btn_start");
    EXPECT_DOUBLE_EQ(img.threshold, 0.95);
    EXPECT_FALSE(img.preload);
}

TEST(GameProfileTest, TriggerConfig) {
    GameTriggerConfig trigger;
    trigger.name = "detect_login";
    trigger.type = "image";
    trigger.action = "click";
    trigger.target = "btn_login";
    trigger.interval = 500;
    trigger.enabled = false;

    EXPECT_EQ(trigger.type, "image");
    EXPECT_EQ(trigger.interval, 500);
    EXPECT_FALSE(trigger.enabled);
}

TEST(GameProfileTest, ScriptConfig) {
    ScriptConfig script;
    script.name = "main_bot";
    script.path = "scripts/main.lua";
    script.autoStart = true;
    script.restartOnCrash = true;
    script.priority = 5;

    EXPECT_EQ(script.name, "main_bot");
    EXPECT_TRUE(script.autoStart);
    EXPECT_EQ(script.priority, 5);
}

// ========== GameProfileManager ==========

TEST(GameProfileManagerTest, SingletonInstance) {
    auto& mgr1 = GameProfileManager::instance();
    auto& mgr2 = GameProfileManager::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST(GameProfileManagerTest, HasProfileFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.hasProfile("nonexistent_profile_xyz"));
}

TEST(GameProfileManagerTest, GetProfileReturnsNull) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_EQ(mgr.getProfile("nonexistent_profile_xyz"), nullptr);
}

TEST(GameProfileManagerTest, GetActiveProfileNullInitially) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_EQ(mgr.getActiveProfile(), nullptr);
}

TEST(GameProfileManagerTest, GetProfileIdsEmpty) {
    auto& mgr = GameProfileManager::instance();
    auto ids = mgr.getProfileIds();
    // May contain profiles from other tests, but the method should not crash
    EXPECT_NO_THROW(mgr.getProfileIds());
}

TEST(GameProfileManagerTest, ValidateProfileEmptyId) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    std::string error;
    EXPECT_FALSE(mgr.validateProfile(profile, error));
    EXPECT_FALSE(error.empty());
}

TEST(GameProfileManagerTest, ValidateProfileValid) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "valid_profile_test";
    profile.name = "Test Game";
    profile.window.title = "GameWindow";
    std::string error;
    EXPECT_TRUE(mgr.validateProfile(profile, error));
}

TEST(GameProfileManagerTest, CreateTemplate) {
    auto& mgr = GameProfileManager::instance();
    auto tmpl = mgr.createTemplate("MyGame");
    EXPECT_EQ(tmpl.name, "MyGame");
    EXPECT_FALSE(tmpl.id.empty());
}

TEST(GameProfileManagerTest, ExportNonexistentProfile) {
    auto& mgr = GameProfileManager::instance();
    std::string json = mgr.exportProfileToJson("nonexistent_export");
    EXPECT_TRUE(json.empty());
}

TEST(GameProfileManagerTest, SetProfilesDirectory) {
    auto& mgr = GameProfileManager::instance();
    mgr.setProfilesDirectory("/tmp/test_profiles");
    EXPECT_EQ(mgr.getProfilesDirectory(), "/tmp/test_profiles");
    // Reset
    mgr.setProfilesDirectory("");
}

TEST(GameProfileManagerTest, FindProfileByWindow) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_EQ(mgr.findProfileByWindow("nonexistent_window_xyz"), nullptr);
}

TEST(GameProfileManagerTest, FindProfileByProcess) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_EQ(mgr.findProfileByProcess("nonexistent_process_xyz"), nullptr);
}

// ========== Save/Delete/Get roundtrip ==========

TEST(GameProfileManagerTest, SaveAndGetProfile) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_save_profile_123";
    profile.name = "Test Save";
    profile.window.title = "TestWindow";
    profile.settings["key"] = "value";

    EXPECT_TRUE(mgr.saveProfile(profile));
    EXPECT_TRUE(mgr.hasProfile("test_save_profile_123"));

    auto* retrieved = mgr.getProfile("test_save_profile_123");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Test Save");
    EXPECT_EQ(retrieved->window.title, "TestWindow");

    EXPECT_TRUE(mgr.deleteProfile("test_save_profile_123"));
    EXPECT_FALSE(mgr.hasProfile("test_save_profile_123"));
}
