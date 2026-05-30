#include <gtest/gtest.h>
#include "wingman/game_profile.hpp"

using namespace wingman;

// ========== GameProfile Struct ==========

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

TEST(GameProfileTest, GameScriptConfig) {
    GameScriptConfig script;
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

// ========== Extended Validation Tests ==========

TEST(GameProfileManagerTest, ValidateProfileMissingName) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "no_name_profile";
    profile.window.title = "SomeWindow";
    std::string error;
    EXPECT_FALSE(mgr.validateProfile(profile, error));
    EXPECT_FALSE(error.empty());
}

TEST(GameProfileManagerTest, ValidateProfileMissingWindowInfo) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "no_window_profile";
    profile.name = "No Window";
    // Both title and processName are empty
    std::string error;
    EXPECT_FALSE(mgr.validateProfile(profile, error));
    EXPECT_NE(error.find("window"), std::string::npos);
}

TEST(GameProfileManagerTest, ValidateProfileWithProcessNameOnly) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "process_only_profile";
    profile.name = "Process Only";
    profile.window.processName = "game.exe";
    std::string error;
    EXPECT_TRUE(mgr.validateProfile(profile, error));
}

// ========== Active Profile Tests ==========

TEST(GameProfileManagerTest, SetActiveProfileNonexistent) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.setActiveProfile("nonexistent_active_xyz"));
}

TEST(GameProfileManagerTest, SetActiveProfileRoundtrip) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_active_profile_456";
    profile.name = "Active Test";
    profile.window.title = "ActiveWindow";
    ASSERT_TRUE(mgr.saveProfile(profile));

    EXPECT_TRUE(mgr.setActiveProfile("test_active_profile_456"));
    auto* active = mgr.getActiveProfile();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->id, "test_active_profile_456");

    mgr.deleteProfile("test_active_profile_456");
}

// ========== Template Tests ==========

TEST(GameProfileManagerTest, CreateTemplateGeneratesValidId) {
    auto& mgr = GameProfileManager::instance();
    auto tmpl = mgr.createTemplate("My Cool Game");

    EXPECT_EQ(tmpl.name, "My Cool Game");
    EXPECT_FALSE(tmpl.id.empty());
    EXPECT_EQ(tmpl.version, "1.0.0");
    EXPECT_FALSE(tmpl.description.empty());
    EXPECT_FALSE(tmpl.window.title.empty());
    EXPECT_FALSE(tmpl.window.exactMatch);
}

TEST(GameProfileManagerTest, CreateTemplateIdIsLowercase) {
    auto& mgr = GameProfileManager::instance();
    auto tmpl = mgr.createTemplate("UPPERCASE");
    // ID should be transformed to lowercase
    EXPECT_EQ(tmpl.id, "uppercase");
}

// ========== Search Tests ==========

TEST(GameProfileManagerTest, FindProfileByWindowFound) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_find_window_789";
    profile.name = "Find Window";
    profile.window.title = "UniqueTitle_789";
    profile.window.exactMatch = false;
    ASSERT_TRUE(mgr.saveProfile(profile));

    auto* found = mgr.findProfileByWindow("Prefix_UniqueTitle_789_Suffix");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, "test_find_window_789");

    mgr.deleteProfile("test_find_window_789");
}

TEST(GameProfileManagerTest, FindProfileByWindowExactMatch) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_exact_match_101";
    profile.name = "Exact Match";
    profile.window.title = "ExactTitle";
    profile.window.exactMatch = true;
    ASSERT_TRUE(mgr.saveProfile(profile));

    // Must match exactly
    auto* found = mgr.findProfileByWindow("ExactTitle");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, "test_exact_match_101");

    // Partial match should not find it
    auto* notFound = mgr.findProfileByWindow("PrefixExactTitle");
    EXPECT_EQ(notFound, nullptr);

    mgr.deleteProfile("test_exact_match_101");
}

TEST(GameProfileManagerTest, FindProfileByProcessFound) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_find_proc_202";
    profile.name = "Find Process";
    profile.window.title = "ProcWindow";
    profile.window.processName = "unique_game_202.exe";
    ASSERT_TRUE(mgr.saveProfile(profile));

    auto* found = mgr.findProfileByProcess("unique_game_202.exe");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, "test_find_proc_202");

    mgr.deleteProfile("test_find_proc_202");
}

// ========== Get All Profiles Tests ==========

TEST(GameProfileManagerTest, GetAllProfilesIncludesSaved) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_getall_303";
    profile.name = "Get All";
    profile.window.title = "GetAllWindow";
    ASSERT_TRUE(mgr.saveProfile(profile));

    auto all = mgr.getAllProfiles();
    bool found = false;
    for (const auto& p : all) {
        if (p.id == "test_getall_303") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    mgr.deleteProfile("test_getall_303");
}

// ========== Export Tests ==========

TEST(GameProfileManagerTest, ExportProfileToJsonContent) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_export_404";
    profile.name = "Export Test";
    profile.version = "2.0.0";
    profile.description = "A test profile for export";
    profile.window.title = "ExportWindow";
    ASSERT_TRUE(mgr.saveProfile(profile));

    std::string json = mgr.exportProfileToJson("test_export_404");
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("test_export_404"), std::string::npos);
    EXPECT_NE(json.find("Export Test"), std::string::npos);
    EXPECT_NE(json.find("2.0.0"), std::string::npos);

    mgr.deleteProfile("test_export_404");
}

// ========== Delete Nonexistent Tests ==========

TEST(GameProfileManagerTest, DeleteNonexistentProfileReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.deleteProfile("nonexistent_delete_xyz"));
}

// ========== Settings Map Tests ==========

TEST(GameProfileTest, SettingsMapOperations) {
    GameProfile profile;
    profile.settings["resolution"] = "1920x1080";
    profile.settings["fps"] = "60";
    profile.settings["language"] = "en";

    EXPECT_EQ(profile.settings.size(), 3u);
    EXPECT_EQ(profile.settings["resolution"], "1920x1080");
    EXPECT_EQ(profile.settings["fps"], "60");

    profile.settings.erase("fps");
    EXPECT_EQ(profile.settings.size(), 2u);
    EXPECT_EQ(profile.settings.count("fps"), 0u);
}
