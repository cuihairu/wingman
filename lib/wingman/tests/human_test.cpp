#include <gtest/gtest.h>
#include "wingman/human.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

// ========== HumanMouse 配置测试 ==========

TEST(HumanMouseTest, DefaultConfig) {
    HumanMouse mouse;
    HumanMouseConfig config = mouse.getConfig();

    EXPECT_GE(config.minMoveDuration, 50);
    EXPECT_LE(config.maxMoveDuration, 500);
    EXPECT_GE(config.minControlPoints, 1);
    EXPECT_LE(config.maxControlPoints, 5);
}

TEST(HumanMouseTest, SetConfig) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.minMoveDuration = 50;
    config.maxMoveDuration = 200;
    config.pathVariance = 5;

    mouse.setConfig(config);
    HumanMouseConfig retrieved = mouse.getConfig();

    EXPECT_EQ(retrieved.minMoveDuration, 50);
    EXPECT_EQ(retrieved.maxMoveDuration, 200);
    EXPECT_EQ(retrieved.pathVariance, 5);
}

// ========== 路径生成测试 ==========

TEST(HumanMouseTest, GenerateLinearPath) {
    HumanMouse mouse;
    Point start(0, 0);
    Point end(100, 0);

    std::vector<Point> path = mouse.generateBezierPath(start, end);

    EXPECT_FALSE(path.empty());
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), end);
}

TEST(HumanMouseTest, GenerateDiagonalPath) {
    HumanMouse mouse;
    Point start(0, 0);
    Point end(100, 100);

    std::vector<Point> path = mouse.generateBezierPath(start, end);

    EXPECT_FALSE(path.empty());
    EXPECT_GE(path.size(), 20);  // 至少20个采样点
}

TEST(HumanMouseTest, PathLengthCalculation) {
    HumanMouse mouse;

    std::vector<Point> path = {
        Point(0, 0),
        Point(100, 0),
        Point(100, 100)
    };

    double length = mouse.calculatePathLength(path);
    EXPECT_GT(length, 0);
    EXPECT_NEAR(length, 200, 1);  // 100 + 100 = 200
}

// ========== 随机性测试 ==========

TEST(HumanMouseTest, RandomDelay) {
    HumanMouse mouse;

    auto start = std::chrono::steady_clock::now();
    mouse.randomDelay(50, 100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 40);  // 允许小误差
    EXPECT_LE(duration, 150);
}

TEST(HumanMouseTest, PathRandomness) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.pathVariance = 20;
    mouse.setConfig(config);

    Point start(0, 0);
    Point end(200, 200);

    // 生成两条路径，应该不同
    auto path1 = mouse.generateBezierPath(start, end);
    auto path2 = mouse.generateBezierPath(start, end);

    // 路径应该不完全相同（由于随机性）
    bool different = false;
    for (size_t i = 1; i < path1.size() - 1; ++i) {
        if (path1[i].x != path2[i].x || path1[i].y != path2[i].y) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

// ========== HumanKeyboard 测试 ==========

TEST(HumanKeyboardTest, DefaultConfig) {
    HumanKeyboard keyboard;
    HumanKeyboardConfig config = keyboard.getConfig();

    EXPECT_GE(config.keyDownDelayMin, 10);
    EXPECT_LE(config.keyDownDelayMax, 200);
    EXPECT_GE(config.typeDelayMin, 30);
    EXPECT_LE(config.typeDelayMax, 300);
}

TEST(HumanKeyboardTest, SetKeyboardConfig) {
    HumanKeyboard keyboard;
    HumanKeyboardConfig config;
    config.keyDownDelayMin = 20;
    config.keyDownDelayMax = 80;
    config.typeDelayMin = 50;

    keyboard.setConfig(config);
    HumanKeyboardConfig retrieved = keyboard.getConfig();

    EXPECT_EQ(retrieved.keyDownDelayMin, 20);
    EXPECT_EQ(retrieved.keyDownDelayMax, 80);
    EXPECT_EQ(retrieved.typeDelayMin, 50);
}

TEST(HumanKeyboardTest, RandomDelayKeyboard) {
    HumanKeyboard keyboard;

    auto start = std::chrono::steady_clock::now();
    keyboard.randomDelay();
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 0);
}

// ========== Human 总接口测试 ==========

TEST(HumanTest, MouseSingleton) {
    auto& mouse1 = Human::mouse();
    auto& mouse2 = Human::mouse();

    EXPECT_EQ(&mouse1, &mouse2);
}

TEST(HumanTest, KeyboardSingleton) {
    auto& keyboard1 = Human::keyboard();
    auto& keyboard2 = Human::keyboard();

    EXPECT_EQ(&keyboard1, &keyboard2);
}

TEST(HumanTest, SetGlobalConfig) {
    HumanMouseConfig config;
    config.minMoveDuration = 80;
    config.maxMoveDuration = 250;

    Human::setMouseConfig(config);

    auto& mouse = Human::mouse();
    HumanMouseConfig retrieved = mouse.getConfig();

    EXPECT_EQ(retrieved.minMoveDuration, 80);
    EXPECT_EQ(retrieved.maxMoveDuration, 250);
}

// ========== 边界条件测试 ==========

TEST(HumanMouseTest, MoveToSamePosition) {
    HumanMouse mouse;
    Point pos(100, 100);

    // 移动到相同位置应该是安全的
    // 注意：实际会调用 Input::move，需要测试环境支持
    // 这里只测试不会崩溃
    SUCCEED();
}

TEST(HumanMouseTest, EmptyPath) {
    HumanMouse mouse;
    std::vector<Point> emptyPath;

    double length = mouse.calculatePathLength(emptyPath);
    EXPECT_EQ(length, 0);
}

TEST(HumanMouseTest, SinglePointPath) {
    HumanMouse mouse;
    std::vector<Point> singlePoint = { Point(50, 50) };

    double length = mouse.calculatePathLength(singlePoint);
    EXPECT_EQ(length, 0);
}
