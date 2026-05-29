#include <gtest/gtest.h>
#include "wingman/input.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

class InputTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ========== Basic Functionality Tests ==========

TEST(InputTest, GetMousePosition) {
    Point pos = Input::getMousePosition();
    EXPECT_GE(pos.x, 0);
    EXPECT_GE(pos.y, 0);
}

TEST(InputTest, IsKeyDown) {
    // Test if VK_SHIFT is available
    bool result = Input::isKeyDown(VK_SHIFT);
    // As long as it does not crash, it passes
    SUCCEED();
}

TEST(InputTest, IsMouseDown) {
    bool result = Input::isMouseDown(MouseButton::Left);
    // As long as it does not crash, it passes
    SUCCEED();
}

TEST(InputTest, Delay) {
    auto start = std::chrono::steady_clock::now();
    Input::delay(100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 90);  // Allow 10ms error margin
}

TEST(InputTest, RandomDelay) {
    auto start = std::chrono::steady_clock::now();
    Input::randomDelay(50, 100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 40);  // Allow 10ms error margin
}

// ========== Mouse Movement Tests ==========

TEST(InputTest, MoveInstant) {
    Point original = Input::getMousePosition();

    Input::move(100, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Point newPos = Input::getMousePosition();
    EXPECT_EQ(newPos.x, 100);
    EXPECT_EQ(newPos.y, 100);

    // Restore original position
    Input::move(original.x, original.y);
}

TEST(InputTest, MoveSmooth) {
    Point original = Input::getMousePosition();

    // Smooth move to target position
    Input::move(200, 200, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    Point newPos = Input::getMousePosition();
    EXPECT_EQ(newPos.x, 200);
    EXPECT_EQ(newPos.y, 200);

    // Restore original position
    Input::move(original.x, original.y);
}

// ========== Mouse Click Tests ==========

TEST(InputTest, ClickLeft) {
    Point original = Input::getMousePosition();

    // Move to a safe position
    Input::move(300, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Execute click (only test that it does not crash)
    Input::click(300, 300, MouseButton::Left);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Restore original position
    Input::move(original.x, original.y);
}

TEST(InputTest, ClickRight) {
    Point original = Input::getMousePosition();

    Input::move(300, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Input::click(300, 300, MouseButton::Right);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Input::move(original.x, original.y);
}

TEST(InputTest, DoubleClick) {
    Point original = Input::getMousePosition();

    Input::move(300, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Input::doubleClick(300, 300, MouseButton::Left);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Input::move(original.x, original.y);
}

// ========== Mouse Down/Up Tests ==========

TEST(InputTest, MouseDownUp) {
    Point original = Input::getMousePosition();

    Input::move(300, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Input::mouseDown(MouseButton::Left);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Input::mouseUp(MouseButton::Left);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Input::move(original.x, original.y);
}

// ========== Scroll Tests ==========

TEST(InputTest, Scroll) {
    // Only test that it does not crash
    Input::scroll(0, 0, 120);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// ========== Keyboard Tests ==========

TEST(InputTest, KeyPress) {
    // Test pressing 'A' key (only test that it does not crash)
    Input::key(0x41);  // VK_A
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(InputTest, KeyDownUp) {
    // Test key down and up (only test that it does not crash)
    Input::keyDown(0x41);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Input::keyUp(0x41);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(InputTest, TypeSimple) {
    // Only test that it does not crash
    Input::type("Hello");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST(InputTest, TypeWithDelay) {
    // Only test that it does not crash
    Input::type("Test", 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// ========== Boundary Condition Tests ==========

TEST(InputTest, MoveToNegativeCoordinates) {
    Point original = Input::getMousePosition();

    // Move to boundary position
    Input::move(0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Point pos = Input::getMousePosition();
    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);

    Input::move(original.x, original.y);
}

TEST(InputTest, LargeCoordinates) {
    Point original = Input::getMousePosition();

    // Move to large coordinates
    Input::move(5000, 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Windows will clamp to screen bounds
    Point pos = Input::getMousePosition();
    EXPECT_GT(pos.x, 0);
    EXPECT_GT(pos.y, 0);

    Input::move(original.x, original.y);
}

TEST(InputTest, ZeroDelay) {
    auto start = std::chrono::steady_clock::now();
    Input::delay(0);
    auto end = std::chrono::steady_clock::now();

    // Should return immediately
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 10);
}

TEST(InputTest, SmallDelay) {
    auto start = std::chrono::steady_clock::now();
    Input::delay(10);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 5);
}
