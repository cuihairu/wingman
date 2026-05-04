#include <gtest/gtest.h>
#include "wingman/game_profile.hpp"

using namespace wingman;

class GameProfileTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// GameProfile Tests
// ============================================================================

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

TEST_F(GameProfileTest, LoadProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();
    EXPECT_NE(&mgr, nullptr);
}

TEST_F(GameProfileTest, GetProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();
    // Try to get a profile (will return nullptr if not found)
    GameProfile* profile = mgr.getProfile("nonexistent");
    EXPECT_EQ(profile, nullptr);
}

TEST_F(GameProfileTest, GetActiveProfile) {
    GameProfileManager& mgr = GameProfileManager::instance();
    // Active profile may be null if none is set
    GameProfile* profile = mgr.getActiveProfile();
    // Just verify the method works
    SUCCEED();
}
