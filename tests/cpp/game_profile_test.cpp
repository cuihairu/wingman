#include <gtest/gtest.h>
#include "wingman/game_profile.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using namespace wingman;

class GameProfileTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// GameTriggerConfig Tests
// ============================================================================

TEST_F(GameProfileTest, GameTriggerConfigDefaults) {
    GameTriggerConfig trigger;
    EXPECT_TRUE(trigger.name.empty());
    EXPECT_TRUE(trigger.type.empty());
    EXPECT_TRUE(trigger.action.empty());
    EXPECT_FALSE(trigger.enabled);
}

TEST_F(GameProfileTest, GameTriggerConfigWithValues) {
    GameTriggerConfig trigger;
    trigger.name = "hp_low";
    trigger.type = "color";
    trigger.action = "key_press";
    trigger.enabled = true;

    EXPECT_EQ(trigger.name, "hp_low");
    EXPECT_EQ(trigger.type, "color");
    EXPECT_EQ(trigger.action, "key_press");
    EXPECT_TRUE(trigger.enabled);
}

// ============================================================================
// GameProfile Tests
// ============================================================================

TEST_F(GameProfileTest, GameProfileDefaults) {
    GameProfile profile;
    EXPECT_TRUE(profile.id.empty());
    EXPECT_TRUE(profile.name.empty());
    EXPECT_TRUE(profile.version.empty());
    EXPECT_TRUE(profile.description.empty());
    EXPECT_TRUE(profile.triggers.empty());
}

TEST_F(GameProfileTest, CreateProfile) {
    GameProfile profile;
    profile.id = "test_game";
    profile.name = "TestGame";
    profile.version = "1.0";
    profile.description = "Test game profile";

    EXPECT_EQ(profile.id, "test_game");
    EXPECT_EQ(profile.name, "TestGame");
    EXPECT_EQ(profile.version, "1.0");
    EXPECT_EQ(profile.description, "Test game profile");
}

TEST_F(GameProfileTest, AddTriggerConfig) {
    GameProfile profile;
    profile.name = "TestGame";

    GameTriggerConfig trigger;
    trigger.name = "test_trigger";
    trigger.type = "color";
    trigger.action = "click";
    trigger.enabled = true;

    profile.triggers.push_back(trigger);
    EXPECT_EQ(profile.triggers.size(), 1);
    EXPECT_EQ(profile.triggers[0].name, "test_trigger");
}

TEST_F(GameProfileTest, AddMultipleTriggerConfigs) {
    GameProfile profile;

    GameTriggerConfig t1;
    t1.name = "trigger1";
    t1.type = "color";

    GameTriggerConfig t2;
    t2.name = "trigger2";
    t2.type = "image";

    profile.triggers.push_back(t1);
    profile.triggers.push_back(t2);

    EXPECT_EQ(profile.triggers.size(), 2);
    EXPECT_EQ(profile.triggers[0].name, "trigger1");
    EXPECT_EQ(profile.triggers[1].name, "trigger2");
}

TEST_F(GameProfileTest, ProfileToJson) {
    GameProfile profile;
    profile.id = "game1";
    profile.name = "Game One";
    profile.version = "2.0";

    std::string json = profile.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("game1"), std::string::npos);
    EXPECT_NE(json.find("Game One"), std::string::npos);
}

TEST_F(GameProfileTest, ProfileFromJson) {
    std::string json = R"({
        "id": "game1",
        "name": "Game One",
        "version": "2.0",
        "description": "Test game",
        "triggers": []
    })";

    GameProfile profile = GameProfile::fromJson(json);
    EXPECT_EQ(profile.id, "game1");
    EXPECT_EQ(profile.name, "Game One");
    EXPECT_EQ(profile.version, "2.0");
    EXPECT_EQ(profile.description, "Test game");
}

// ============================================================================
// GameProfileManager Tests
// ============================================================================

TEST_F(GameProfileTest, ManagerInstance) {
    GameProfileManager& mgr1 = GameProfileManager::instance();
    GameProfileManager& mgr2 = GameProfileManager::instance();
    EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(GameProfileTest, ManagerCreateProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();

    GameProfile profile;
    profile.id = "test_new";
    profile.name = "New Game";

    EXPECT_TRUE(mgr.createProfile(profile));
}

TEST_F(GameProfileTest, ManagerGetProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();

    // Try to get a profile that doesn't exist
    GameProfile* profile = mgr.getProfile("nonexistent_xyz");
    EXPECT_EQ(profile, nullptr);
}

TEST_F(GameProfileTest, ManagerGetAllProfiles) {
    GameProfileManager& mgr = GameProfileManager::instance();
    auto profiles = mgr.getAllProfiles();
    // May be empty or have profiles
    SUCCEED();
}

TEST_F(GameProfileTest, ManagerDeleteProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();
    // Try to delete non-existent profile
    EXPECT_FALSE(mgr.deleteProfile("nonexistent_xyz"));
}

TEST_F(GameProfileTest, ManagerSetActiveProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();
    // Try to set non-existent profile as active
    EXPECT_FALSE(mgr.setActiveProfile("nonexistent_xyz"));
}

TEST_F(GameProfileTest, ManagerGetActiveProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();
    GameProfile* profile = mgr.getActiveProfile();
    // May be null if no active profile
    SUCCEED();
}

TEST_F(GameProfileTest, ManagerLoadProfiles) {
    GameProfileManager& mgr = GameProfileManager::instance();
    // Try to load from non-existent directory
    EXPECT_FALSE(mgr.loadProfiles("nonexistent_directory"));
}

TEST_F(GameProfileTest, ManagerSaveProfiles) {
    GameProfileManager& mgr = GameProfileManager::instance();
    // Try to save to a directory
    EXPECT_NO_THROW(mgr.saveProfiles("."));
}

// ============================================================================
// GameWindowInfo Tests
// ============================================================================

TEST_F(GameProfileTest, GameWindowInfoDefaults) {
    GameWindowInfo info;
    EXPECT_TRUE(info.title.empty());
    EXPECT_TRUE(info.exeName.empty());
    EXPECT_EQ(info.hwnd, 0);
    EXPECT_EQ(info.matchMethod, WindowMatchMethod::Exact);
}

TEST_F(GameProfileTest, GameWindowInfoWithValues) {
    GameWindowInfo info;
    info.title = "Game Window";
    info.exeName = "game.exe";
    info.className = "GameWindowClass";
    info.hwnd = 12345;

    EXPECT_EQ(info.title, "Game Window");
    EXPECT_EQ(info.exeName, "game.exe");
    EXPECT_EQ(info.className, "GameWindowClass");
    EXPECT_EQ(info.hwnd, 12345);
}

TEST_F(GameProfileTest, WindowMatchMethodValues) {
    EXPECT_EQ(static_cast<int>(WindowMatchMethod::Exact), 0);
    EXPECT_EQ(static_cast<int>(WindowMatchMethod::Contains), 1);
    EXPECT_EQ(static_cast<int>(WindowMatchMethod::Regex), 2);
    EXPECT_EQ(static_cast<int>(WindowMatchMethod::Process), 3);
}
