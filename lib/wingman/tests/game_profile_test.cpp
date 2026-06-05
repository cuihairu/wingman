#include <gtest/gtest.h>
#include "wingman/game_profile.hpp"
#include <filesystem>
#include <fstream>

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

// ========== toJson Coverage Tests ==========

TEST(GameProfileManagerTest, ExportProfileWithAllConfigTypes) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_tojson_500";
    profile.name = "ToJson Test";
    profile.version = "1.0";
    profile.description = "Tests toJson helpers";
    profile.window.title = "ToJsonWnd";
    profile.window.processName = "tojson.exe";

    // ColorConfig
    ColorConfig color;
    color.name = "health_red";
    color.r = 255;
    color.g = 0;
    color.b = 0;
    color.tolerance = 15;
    profile.colors.push_back(color);

    // ImageConfig
    ImageConfig img;
    img.name = "btn_start";
    img.path = "images/start.png";
    img.threshold = 0.95;
    img.preload = true;
    profile.images.push_back(img);

    // GameTriggerConfig
    GameTriggerConfig trigger;
    trigger.name = "auto_click";
    trigger.type = "color";
    trigger.action = "click";
    trigger.target = "100,200";
    trigger.interval = 1000;
    trigger.enabled = true;
    profile.triggers.push_back(trigger);

    // GameScriptConfig
    GameScriptConfig script;
    script.name = "main_script";
    script.path = "scripts/main.lua";
    script.autoStart = true;
    script.restartOnCrash = false;
    script.priority = 1;
    profile.scripts.push_back(script);

    // Settings
    profile.settings["resolution"] = "1920x1080";

    ASSERT_TRUE(mgr.saveProfile(profile));

    std::string json = mgr.exportProfileToJson("test_tojson_500");
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("health_red"), std::string::npos);
    EXPECT_NE(json.find("btn_start"), std::string::npos);
    EXPECT_NE(json.find("auto_click"), std::string::npos);
    EXPECT_NE(json.find("main_script"), std::string::npos);

    mgr.deleteProfile("test_tojson_500");
}

TEST(GameProfileManagerTest, ExportProfileWithEmptyCollections) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_tojson_501";
    profile.name = "Empty Collections";
    profile.window.title = "EmptyWnd";
    ASSERT_TRUE(mgr.saveProfile(profile));

    std::string json = mgr.exportProfileToJson("test_tojson_501");
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("EmptyWnd"), std::string::npos);

    mgr.deleteProfile("test_tojson_501");
}

TEST(GameProfileManagerTest, ImportProfileFromJson) {
    auto& mgr = GameProfileManager::instance();

    std::string json = R"({
        "id": "test_import_502",
        "name": "Import Test",
        "version": "1.0",
        "window": {"title": "ImportWnd", "processName": "import.exe", "exactMatch": false, "fullscreen": false}
    })";

    EXPECT_TRUE(mgr.importProfileFromJson(json, "test_import_502"));
    EXPECT_TRUE(mgr.hasProfile("test_import_502"));

    mgr.deleteProfile("test_import_502");
}

TEST(GameProfileManagerTest, ImportInvalidJsonReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.importProfileFromJson("not valid json", "test_import_503"));
}

TEST(GameProfileManagerTest, ExportProfilePackageNonexistentReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.exportProfilePackage("nonexistent_pkg", "/tmp/pkg.json"));
}

// ========== Import/Export Package Tests ==========

TEST(GameProfileManagerTest, ExportAndImportProfilePackage) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_pkg_roundtrip";
    profile.name = "Package Test";
    profile.window.title = "PkgWindow";
    profile.window.processName = "pkg.exe";
    ASSERT_TRUE(mgr.saveProfile(profile));

    auto tempDir = std::filesystem::temp_directory_path() / "wingman_pkg_test";
    std::filesystem::create_directories(tempDir);
    std::string pkgPath = (tempDir / "profile.json").string();

    EXPECT_TRUE(mgr.exportProfilePackage("test_pkg_roundtrip", pkgPath));
    EXPECT_TRUE(std::filesystem::exists(pkgPath));

    // Remove original, re-import from file
    mgr.deleteProfile("test_pkg_roundtrip");
    EXPECT_FALSE(mgr.hasProfile("test_pkg_roundtrip"));

    EXPECT_TRUE(mgr.importProfilePackage(pkgPath));
    EXPECT_TRUE(mgr.hasProfile("test_pkg_roundtrip"));

    mgr.deleteProfile("test_pkg_roundtrip");
    std::filesystem::remove_all(tempDir);
}

TEST(GameProfileManagerTest, ImportProfilePackageNonexistentFile) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.importProfilePackage("/nonexistent/path/profile.json"));
}

TEST(GameProfileManagerTest, ImportProfileFromJsonWithEmptyIdOverride) {
    auto& mgr = GameProfileManager::instance();

    std::string json = R"({
        "id": "original_id",
        "name": "Override Test",
        "window": {"title": "OverrideWnd"}
    })";

    // Empty id override should keep original id
    EXPECT_TRUE(mgr.importProfileFromJson(json, ""));
    EXPECT_TRUE(mgr.hasProfile("original_id"));
    mgr.deleteProfile("original_id");
}

TEST(GameProfileManagerTest, ImportProfileFromJsonWithColorsImagesTriggersScripts) {
    auto& mgr = GameProfileManager::instance();

    std::string json = R"({
        "id": "test_import_full",
        "name": "Full Import",
        "version": "2.0",
        "description": "Full profile import",
        "window": {"title": "FullWnd", "className": "WndClass", "processName": "full.exe", "exactMatch": true, "fullscreen": true},
        "colors": [{"name": "red", "r": 255, "g": 0, "b": 0, "tolerance": 10}],
        "images": [{"name": "btn", "path": "btn.png", "threshold": 0.9, "preload": true}],
        "triggers": [{"name": "trig", "type": "color", "action": "click", "target": "100,200", "interval": 500, "enabled": true}],
        "scripts": [{"name": "scr", "path": "scr.lua", "autoStart": true, "restartOnCrash": false, "priority": 3}],
        "settings": {"key1": "val1", "key2": "val2"}
    })";

    EXPECT_TRUE(mgr.importProfileFromJson(json, "test_import_full"));
    auto* p = mgr.getProfile("test_import_full");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "Full Import");
    EXPECT_EQ(p->version, "2.0");
    EXPECT_EQ(p->description, "Full profile import");
    EXPECT_EQ(p->window.className, "WndClass");
    EXPECT_TRUE(p->window.exactMatch);
    EXPECT_TRUE(p->window.fullscreen);
    EXPECT_EQ(p->colors.size(), 1u);
    EXPECT_EQ(p->colors[0].name, "red");
    EXPECT_EQ(p->images.size(), 1u);
    EXPECT_EQ(p->images[0].name, "btn");
    EXPECT_EQ(p->triggers.size(), 1u);
    EXPECT_EQ(p->triggers[0].name, "trig");
    EXPECT_EQ(p->scripts.size(), 1u);
    EXPECT_EQ(p->scripts[0].name, "scr");
    EXPECT_EQ(p->settings.size(), 2u);

    mgr.deleteProfile("test_import_full");
}

// ========== INI Format Parsing ==========

TEST(GameProfileManagerTest, LoadProfileFromIniFormat) {
    auto& mgr = GameProfileManager::instance();

    auto tempDir = std::filesystem::temp_directory_path() / "wingman_ini_test";
    std::filesystem::create_directories(tempDir);
    std::string iniPath = (tempDir / "profile.ini").string();

    std::ofstream f(iniPath);
    f << "# comment line\n";
    f << "; another comment\n";
    f << "[profile]\n";
    f << "id = ini_test_profile\n";
    f << "name = INI Test\n";
    f << "version = 1.0\n";
    f << "description = Test INI parsing\n";
    f << "[window]\n";
    f << "title = IniWindow\n";
    f << "process = ini.exe\n";
    f << "exact_match = true\n";
    f.close();

    EXPECT_TRUE(mgr.loadProfileFromFile(iniPath));
    EXPECT_TRUE(mgr.hasProfile("ini_test_profile"));

    auto* p = mgr.getProfile("ini_test_profile");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "INI Test");
    EXPECT_EQ(p->window.title, "IniWindow");
    EXPECT_EQ(p->window.processName, "ini.exe");
    EXPECT_TRUE(p->window.exactMatch);

    mgr.deleteProfile("ini_test_profile");
    std::filesystem::remove_all(tempDir);
}

TEST(GameProfileManagerTest, LoadProfileFromInvalidFileReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.loadProfileFromFile("/nonexistent/path.json"));
}

TEST(GameProfileManagerTest, SaveProfileWithEmptyIdReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.name = "No ID";
    EXPECT_FALSE(mgr.saveProfile(profile));
}

TEST(GameProfileManagerTest, LoadProfileFromDirNonexistent) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.loadProfileFromDir("/nonexistent_dir_xyz"));
}
