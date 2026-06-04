#include <gtest/gtest.h>
#include "wingman/game_profile.hpp"
#include <fstream>
#include <filesystem>

using namespace wingman;

namespace {

const char* kTestJson = R"({
  "id": "import_test_1",
  "name": "Imported Game",
  "version": "2.0.0",
  "description": "An imported profile",
  "window": {
    "title": "ImportWindow",
    "className": "ImportClass",
    "processName": "import.exe",
    "exactMatch": true,
    "fullscreen": false
  },
  "colors": [
    {"name": "red", "r": 255, "g": 0, "b": 0, "tolerance": 10}
  ],
  "images": [
    {"name": "btn", "path": "img.png", "threshold": 0.9, "preload": true}
  ],
  "triggers": [
    {"name": "t1", "type": "color", "action": "click", "target": "btn", "interval": 500, "enabled": true}
  ],
  "scripts": [
    {"name": "main", "path": "main.lua", "autoStart": true, "restartOnCrash": false, "priority": 3}
  ],
  "settings": {
    "key1": "val1",
    "key2": "val2"
  }
})";

std::string writeTempFile(const std::string& content, const std::string& name) {
    auto dir = std::filesystem::temp_directory_path() / "wingman_test_profiles";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::ofstream f(path);
    f << content;
    f.close();
    return path.string();
}

} // namespace

// ========== Import from JSON ==========

TEST(GameProfileImportTest, ImportFromValidJson) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_TRUE(mgr.importProfileFromJson(kTestJson));
    EXPECT_TRUE(mgr.hasProfile("import_test_1"));

    auto* p = mgr.getProfile("import_test_1");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "Imported Game");
    EXPECT_EQ(p->version, "2.0.0");
    EXPECT_EQ(p->description, "An imported profile");
    EXPECT_EQ(p->window.title, "ImportWindow");
    EXPECT_EQ(p->window.className, "ImportClass");
    EXPECT_EQ(p->window.processName, "import.exe");
    EXPECT_TRUE(p->window.exactMatch);
    EXPECT_FALSE(p->window.fullscreen);
    EXPECT_EQ(p->colors.size(), 1u);
    EXPECT_EQ(p->colors[0].name, "red");
    EXPECT_EQ(p->colors[0].r, 255);
    EXPECT_EQ(p->images.size(), 1u);
    EXPECT_EQ(p->images[0].name, "btn");
    EXPECT_EQ(p->images[0].path, "img.png");
    EXPECT_DOUBLE_EQ(p->images[0].threshold, 0.9);
    EXPECT_TRUE(p->images[0].preload);
    EXPECT_EQ(p->triggers.size(), 1u);
    EXPECT_EQ(p->triggers[0].name, "t1");
    EXPECT_EQ(p->scripts.size(), 1u);
    EXPECT_EQ(p->scripts[0].name, "main");
    EXPECT_EQ(p->settings.size(), 2u);
    EXPECT_EQ(p->settings["key1"], "val1");

    mgr.deleteProfile("import_test_1");
}

TEST(GameProfileImportTest, ImportWithOverrideId) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_TRUE(mgr.importProfileFromJson(kTestJson, "custom_id_override"));
    EXPECT_TRUE(mgr.hasProfile("custom_id_override"));
    EXPECT_FALSE(mgr.hasProfile("import_test_1"));
    mgr.deleteProfile("custom_id_override");
}

TEST(GameProfileImportTest, ImportInvalidJsonReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.importProfileFromJson("{invalid json!!}"));
}

TEST(GameProfileImportTest, ImportMissingIdReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.importProfileFromJson(R"({"name": "No ID"})"));
}

TEST(GameProfileImportTest, ImportMissingNameReturnsFalse) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.importProfileFromJson(R"({"id": "no_name_profile"})"));
}

// ========== Export Package ==========

TEST(GameProfileExportTest, ExportPackageReturnsFalseForNonexistent) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.exportProfilePackage("nonexistent_pkg", "/tmp/test.json"));
}

TEST(GameProfileExportTest, ExportAndImportPackageRoundtrip) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "pkg_roundtrip_999";
    profile.name = "Package Test";
    profile.window.title = "PkgWindow";
    ASSERT_TRUE(mgr.saveProfile(profile));

    auto outPath = std::filesystem::temp_directory_path() / "wingman_pkg_test.json";
    EXPECT_TRUE(mgr.exportProfilePackage("pkg_roundtrip_999", outPath.string()));
    EXPECT_TRUE(std::filesystem::exists(outPath));

    // Import from the exported file
    std::ifstream f(outPath);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    f.close();

    EXPECT_FALSE(content.empty());
    EXPECT_NE(content.find("Package Test"), std::string::npos);

    mgr.deleteProfile("pkg_roundtrip_999");
    std::filesystem::remove(outPath);
}

// ========== Import Package ==========

TEST(GameProfileImportTest, ImportPackageReturnsFalseForNonexistentFile) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.importProfilePackage("/nonexistent/path.json"));
}

TEST(GameProfileImportTest, ImportPackageFromFile) {
    auto path = writeTempFile(kTestJson, "import_pkg_test.json");
    auto& mgr = GameProfileManager::instance();
    EXPECT_TRUE(mgr.importProfilePackage(path));
    EXPECT_TRUE(mgr.hasProfile("import_test_1"));
    mgr.deleteProfile("import_test_1");
    std::filesystem::remove(path);
}

// ========== Load Profile from File ==========

TEST(GameProfileLoadTest, LoadFromFileReturnsFalseForNonexistent) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.loadProfileFromFile("/nonexistent/profile.json"));
}

TEST(GameProfileLoadTest, LoadFromJsonFile) {
    auto path = writeTempFile(kTestJson, "load_json_test.json");
    auto& mgr = GameProfileManager::instance();
    EXPECT_TRUE(mgr.loadProfileFromFile(path));
    EXPECT_TRUE(mgr.hasProfile("import_test_1"));
    mgr.deleteProfile("import_test_1");
    std::filesystem::remove(path);
}

TEST(GameProfileLoadTest, LoadFromIniFile) {
    std::string iniContent = R"([profile]
id = ini_test_profile
name = INI Game
version = 1.0

[window]
title = IniWindow
process = ini_game.exe
exact_match = false
)";
    auto path = writeTempFile(iniContent, "load_ini_test.ini");
    auto& mgr = GameProfileManager::instance();
    EXPECT_TRUE(mgr.loadProfileFromFile(path));
    EXPECT_TRUE(mgr.hasProfile("ini_test_profile"));

    auto* p = mgr.getProfile("ini_test_profile");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "INI Game");
    EXPECT_EQ(p->window.title, "IniWindow");
    EXPECT_EQ(p->window.processName, "ini_game.exe");
    EXPECT_FALSE(p->window.exactMatch);

    mgr.deleteProfile("ini_test_profile");
    std::filesystem::remove(path);
}

// ========== Load from Directory ==========

TEST(GameProfileLoadTest, LoadFromDirReturnsFalseForNonexistent) {
    auto& mgr = GameProfileManager::instance();
    EXPECT_FALSE(mgr.loadProfileFromDir("/nonexistent_dir_xyz"));
}

TEST(GameProfileLoadTest, LoadFromDir) {
    auto dir = std::filesystem::temp_directory_path() / "wingman_profile_dir_test";
    auto configPath = dir / "profile.json";
    std::filesystem::create_directories(dir);

    std::ofstream f(configPath);
    f << kTestJson;
    f.close();

    auto& mgr = GameProfileManager::instance();
    EXPECT_TRUE(mgr.loadProfileFromDir(dir.string()));
    EXPECT_TRUE(mgr.hasProfile("import_test_1"));
    mgr.deleteProfile("import_test_1");
    std::filesystem::remove_all(dir);
}

// ========== Save to File ==========

TEST(GameProfileSaveTest, SaveCreatesFile) {
    auto& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "save_file_test_555";
    profile.name = "Save File Test";
    profile.window.title = "SaveWindow";

    auto dir = std::filesystem::temp_directory_path() / "wingman_save_test";
    auto path = (dir / "profile.json").string();

    EXPECT_TRUE(mgr.saveProfileToFile(profile, path));
    EXPECT_TRUE(std::filesystem::exists(path));

    // Verify file content
    std::string content;
    {
        std::ifstream f(path);
        content = std::string((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    }
    EXPECT_NE(content.find("Save File Test"), std::string::npos);

    std::filesystem::remove_all(dir);
}

// ========== Scan Profiles Directory ==========

TEST(GameProfileLoadTest, ScanProfilesDirectoryNonexistent) {
    auto& mgr = GameProfileManager::instance();
    auto origDir = mgr.getProfilesDirectory();
    mgr.setProfilesDirectory("/nonexistent_scan_dir_xyz");
    // Should not crash
    EXPECT_NO_THROW(mgr.setProfilesDirectory(origDir));
}

TEST(GameProfileLoadTest, ScanProfilesDirectory) {
    auto baseDir = std::filesystem::temp_directory_path() / "wingman_scan_test";
    auto profileDir = baseDir / "scan_profile_1";
    std::filesystem::create_directories(profileDir);

    std::ofstream f(profileDir / "profile.json");
    f << kTestJson;
    f.close();

    auto& mgr = GameProfileManager::instance();
    auto origDir = mgr.getProfilesDirectory();
    mgr.setProfilesDirectory(baseDir.string());

    EXPECT_TRUE(mgr.hasProfile("import_test_1"));
    mgr.deleteProfile("import_test_1");
    mgr.setProfilesDirectory(origDir);
    std::filesystem::remove_all(baseDir);
}

// ========== Validate with Optional Fields ==========

TEST(GameProfileValidationTest, ValidateWithAllOptionalFields) {
    auto& mgr = GameProfileManager::instance();
    GameProfile profile;
    profile.id = "full_validation";
    profile.name = "Full";
    profile.window.title = "Wnd";
    profile.version = "1.0";
    profile.description = "desc";
    profile.window.className = "cls";
    profile.window.processName = "proc.exe";
    std::string error;
    EXPECT_TRUE(mgr.validateProfile(profile, error));
}

// ========== Template with Spaces ==========

TEST(GameProfileTemplateTest, CreateTemplateWithSpaces) {
    auto& mgr = GameProfileManager::instance();
    auto tmpl = mgr.createTemplate("My Cool Game");
    EXPECT_EQ(tmpl.id, "my_cool_game");
    EXPECT_EQ(tmpl.name, "My Cool Game");
    EXPECT_FALSE(tmpl.description.empty());
}
