#include <gtest/gtest.h>
#include "wingman/platform/mock_input.hpp"

using namespace wingman::platform;
using wingman::platform::mock::MockInput;

class InputTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(input.initialize(InputConfig{}));
    }

    MockInput input;
};

TEST_F(InputTest, TracksMousePosition) {
    input.mouseMove(100, 200);

    const Point pos = input.getMousePosition();
    EXPECT_EQ(pos.x, 100);
    EXPECT_EQ(pos.y, 200);
    EXPECT_EQ(input.getMouseMoveCallCount(), 1);
}

TEST_F(InputTest, SupportsRelativeMouseMovement) {
    input.mouseMove(100, 200);
    input.mouseMoveRelative(25, -50);

    const Point pos = input.getMousePosition();
    EXPECT_EQ(pos.x, 125);
    EXPECT_EQ(pos.y, 150);
    EXPECT_TRUE(input.supportsRelativeMovement());
}

TEST_F(InputTest, TracksMouseButtonState) {
    input.mouseDown(MouseButton::Left);
    EXPECT_TRUE(input.isMousePressed(MouseButton::Left));

    input.mouseUp(MouseButton::Left);
    EXPECT_FALSE(input.isMousePressed(MouseButton::Left));
}

TEST_F(InputTest, CountsMouseClicks) {
    input.mouseClick(MouseButton::Right);
    input.mouseDoubleClick(MouseButton::Left);

    EXPECT_EQ(input.getClickCallCount(MouseButton::Right), 1);
    EXPECT_EQ(input.getClickCallCount(MouseButton::Left), 2);
}

TEST_F(InputTest, TracksScrollDeltas) {
    input.mouseWheel(120);
    input.mouseWheel(-40);
    input.mouseWheelHorizontal(15);

    EXPECT_EQ(input.getScrollDelta(), 80);
    EXPECT_EQ(input.getHorizontalScrollDelta(), 15);
}

TEST_F(InputTest, TracksKeyStateAndPresses) {
    input.keyDown(KeyCode::A);
    EXPECT_TRUE(input.isKeyPressed(KeyCode::A));

    input.keyUp(KeyCode::A);
    EXPECT_FALSE(input.isKeyPressed(KeyCode::A));

    input.keyPress(KeyCode::B);
    EXPECT_EQ(input.getKeyPressCallCount(KeyCode::B), 1);
}

TEST_F(InputTest, HandlesKeyCombination) {
    input.keyCombination({KeyCode::Control, KeyCode::Shift}, KeyCode::A);

    EXPECT_FALSE(input.isKeyPressed(KeyCode::Control));
    EXPECT_FALSE(input.isKeyPressed(KeyCode::Shift));
    EXPECT_FALSE(input.isKeyPressed(KeyCode::A));
    EXPECT_EQ(input.getKeyPressCallCount(KeyCode::A), 1);
}

TEST_F(InputTest, StoresTextInput) {
    input.textInput("Hello");
    input.textInput(" World");

    EXPECT_TRUE(input.supportsTextInput());
    EXPECT_EQ(input.getInputText(), "Hello World");
}

TEST_F(InputTest, UpdatesInputDelayConfig) {
    input.setInputDelay(2500);

    EXPECT_EQ(input.getConfig().inputDelay, 2500);
}

TEST_F(InputTest, ReportsBackendInfo) {
    EXPECT_EQ(input.getBackendName(), "Mock");

    const BackendInfo info = input.getBackendInfo();
    EXPECT_EQ(info.name, "Mock");
    EXPECT_TRUE(info.isInitialized);
}
