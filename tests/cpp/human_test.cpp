#include <gtest/gtest.h>
#include "wingman/human.hpp"

using namespace wingman;

class HumanTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// HumanMouse Tests
// ============================================================================

TEST_F(HumanTest, HumanMouseConfigDefaults) {
    HumanMouseConfig config;
    EXPECT_EQ(config.minMoveDuration, 100);
    EXPECT_EQ(config.maxMoveDuration, 300);
    EXPECT_EQ(config.moveVariance, 20);
    EXPECT_EQ(config.minControlPoints, 1);
    EXPECT_EQ(config.maxControlPoints, 3);
    EXPECT_EQ(config.pathVariance, 10);
    EXPECT_EQ(config.clickDelayMin, 50);
    EXPECT_EQ(config.clickDelayMax, 150);
    EXPECT_EQ(config.clickDurationMin, 50);
    EXPECT_EQ(config.clickDurationMax, 100);
    EXPECT_EQ(config.doubleClickIntervalMin, 80);
    EXPECT_EQ(config.doubleClickIntervalMax, 150);
    EXPECT_EQ(config.scrollDelayMin, 30);
    EXPECT_EQ(config.scrollDelayMax, 80);
    EXPECT_TRUE(config.enableRandomDelay);
    EXPECT_TRUE(config.enablePathRandomness);
}

TEST_F(HumanTest, HumanMouseConstruction) {
    HumanMouse mouse1;
    EXPECT_NO_THROW();

    HumanMouseConfig config;
    config.minMoveDuration = 200;
    config.maxMoveDuration = 500;
    HumanMouse mouse2(config);
    EXPECT_EQ(mouse2.getConfig().minMoveDuration, 200);
}

TEST_F(HumanTest, HumanMouseSetGetConfig) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.minMoveDuration = 150;
    config.maxMoveDuration = 400;
    config.pathVariance = 15;

    mouse.setConfig(config);
    auto retrieved = mouse.getConfig();
    EXPECT_EQ(retrieved.minMoveDuration, 150);
    EXPECT_EQ(retrieved.maxMoveDuration, 400);
    EXPECT_EQ(retrieved.pathVariance, 15);
}

TEST_F(HumanTest, HumanMouseCurrentPosition) {
    HumanMouse mouse;
    Point pos = mouse.getCurrentPosition();
    EXPECT_GE(pos.x, 0);
    EXPECT_GE(pos.y, 0);
}

TEST_F(HumanTest, HumanMouseRandomDelay) {
    HumanMouse mouse;
    EXPECT_NO_THROW(mouse.randomDelay(10, 20));
}

TEST_F(HumanTest, HumanMouseMoveTo) {
    HumanMouse mouse;
    EXPECT_NO_THROW(mouse.moveTo(100, 100));
    EXPECT_NO_THROW(mouse.moveTo(Point(200, 200)));
    EXPECT_NO_THROW(mouse.moveTo(300, 300, 200));
}

TEST_F(HumanTest, HumanMouseClick) {
    HumanMouse mouse;
    EXPECT_NO_THROW(mouse.click(100, 100));
    EXPECT_NO_THROW(mouse.click(Point(100, 100)));
    EXPECT_NO_THROW(mouse.rightClick(100, 100));
    EXPECT_NO_THROW(mouse.doubleClick(100, 100));
}

TEST_F(HumanTest, HumanMouseDrag) {
    HumanMouse mouse;
    EXPECT_NO_THROW(mouse.drag(100, 100, 200, 200));
    EXPECT_NO_THROW(mouse.drag(Point(100, 100), Point(200, 200)));
}

TEST_F(HumanTest, HumanMouseScroll) {
    HumanMouse mouse;
    EXPECT_NO_THROW(mouse.scroll(100, 100, 120));
}

TEST_F(HumanTest, HumanMouseInstance) {
    auto& mouse1 = HumanMouse::instance();
    auto& mouse2 = HumanMouse::instance();
    EXPECT_EQ(&mouse1, &mouse2);
}

// ============================================================================
// HumanKeyboard Tests
// ============================================================================

TEST_F(HumanTest, HumanKeyboardConfigDefaults) {
    HumanKeyboardConfig config;
    EXPECT_EQ(config.keyDownDelayMin, 30);
    EXPECT_EQ(config.keyDownDelayMax, 80);
    EXPECT_EQ(config.keyDurationMin, 40);
    EXPECT_EQ(config.keyDurationMax, 100);
    EXPECT_EQ(config.typeDelayMin, 50);
    EXPECT_EQ(config.typeDelayMax, 150);
    EXPECT_TRUE(config.enableRandomDelay);
}

TEST_F(HumanTest, HumanKeyboardConstruction) {
    HumanKeyboard keyboard1;
    EXPECT_NO_THROW();

    HumanKeyboardConfig config;
    config.keyDownDelayMin = 50;
    config.keyDownDelayMax = 100;
    HumanKeyboard keyboard2(config);
    EXPECT_EQ(keyboard2.getConfig().keyDownDelayMin, 50);
}

TEST_F(HumanTest, HumanKeyboardSetGetConfig) {
    HumanKeyboard keyboard;
    HumanKeyboardConfig config;
    config.keyDownDelayMin = 40;
    config.keyDownDelayMax = 90;
    config.typeDelayMin = 60;

    keyboard.setConfig(config);
    auto retrieved = keyboard.getConfig();
    EXPECT_EQ(retrieved.keyDownDelayMin, 40);
    EXPECT_EQ(retrieved.keyDownDelayMax, 90);
    EXPECT_EQ(retrieved.typeDelayMin, 60);
}

TEST_F(HumanTest, HumanKeyboardInstance) {
    auto& keyboard1 = HumanKeyboard::instance();
    auto& keyboard2 = HumanKeyboard::instance();
    EXPECT_EQ(&keyboard1, &keyboard2);
}

// ============================================================================
// Human Tests
// ============================================================================

TEST_F(HumanTest, HumanStaticAccess) {
    EXPECT_NO_THROW(Human::mouse());
    EXPECT_NO_THROW(Human::keyboard());

    HumanMouseConfig mouseConfig;
    mouseConfig.minMoveDuration = 150;
    EXPECT_NO_THROW(Human::setMouseConfig(mouseConfig));

    HumanKeyboardConfig keyboardConfig;
    keyboardConfig.keyDownDelayMin = 40;
    EXPECT_NO_THROW(Human::setKeyboardConfig(keyboardConfig));
}
