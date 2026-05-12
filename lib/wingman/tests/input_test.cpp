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

// ========== 基础功能测试 ==========

TEST(InputTest, GetMousePosition) {
    Point pos = Input::getMousePosition();
    EXPECT_GE(pos.x, 0);
    EXPECT_GE(pos.y, 0);
}

TEST(InputTest, IsKeyDown) {
    // 测试 VK_SHIFT 是否可用
    bool result = Input::isKeyDown(VK_SHIFT);
    // 只要不崩溃就算通过
    SUCCEED();
}

TEST(InputTest, IsMouseDown) {
    bool result = Input::isMouseDown(MouseButton::Left);
    // 只要不崩溃就算通过
    SUCCEED();
}

TEST(InputTest, Delay) {
    auto start = std::chrono::steady_clock::now();
    Input::delay(100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 90);  // 允许10ms误差
}

TEST(InputTest, RandomDelay) {
    auto start = std::chrono::steady_clock::now();
    Input::randomDelay(50, 100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 40);  // 允许10ms误差
}

// ========== 鼠标移动测试 ==========

TEST(InputTest, MoveInstant) {
    Point original = Input::getMousePosition();

    Input::move(100, 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Point newPos = Input::getMousePosition();
    EXPECT_EQ(newPos.x, 100);
    EXPECT_EQ(newPos.y, 100);

    // 恢复原始位置
    Input::move(original.x, original.y);
}

TEST(InputTest, MoveSmooth) {
    Point original = Input::getMousePosition();

    // 平滑移动到目标位置
    Input::move(200, 200, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    Point newPos = Input::getMousePosition();
    EXPECT_EQ(newPos.x, 200);
    EXPECT_EQ(newPos.y, 200);

    // 恢复原始位置
    Input::move(original.x, original.y);
}

// ========== 鼠标点击测试 ==========

TEST(InputTest, ClickLeft) {
    Point original = Input::getMousePosition();

    // 移动到安全位置
    Input::move(300, 300);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 执行点击（只测试不崩溃）
    Input::click(300, 300, MouseButton::Left);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 恢复原始位置
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

// ========== 鼠标按下/释放测试 ==========

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

// ========== 滚轮测试 ==========

TEST(InputTest, Scroll) {
    // 只测试不崩溃
    Input::scroll(0, 0, 120);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// ========== 键盘测试 ==========

TEST(InputTest, KeyPress) {
    // 测试按下 'A' 键（只测试不崩溃）
    Input::key(0x41);  // VK_A
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(InputTest, KeyDownUp) {
    // 测试按下和释放（只测试不崩溃）
    Input::keyDown(0x41);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Input::keyUp(0x41);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(InputTest, TypeSimple) {
    // 只测试不崩溃
    Input::type("Hello");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST(InputTest, TypeWithDelay) {
    // 只测试不崩溃
    Input::type("Test", 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// ========== 边界条件测试 ==========

TEST(InputTest, MoveToNegativeCoordinates) {
    Point original = Input::getMousePosition();

    // 移动到边界位置
    Input::move(0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    Point pos = Input::getMousePosition();
    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);

    Input::move(original.x, original.y);
}

TEST(InputTest, LargeCoordinates) {
    Point original = Input::getMousePosition();

    // 移动到大坐标
    Input::move(5000, 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Windows 会限制在屏幕范围内
    Point pos = Input::getMousePosition();
    EXPECT_GT(pos.x, 0);
    EXPECT_GT(pos.y, 0);

    Input::move(original.x, original.y);
}

TEST(InputTest, ZeroDelay) {
    auto start = std::chrono::steady_clock::now();
    Input::delay(0);
    auto end = std::chrono::steady_clock::now();

    // 应该立即返回
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
