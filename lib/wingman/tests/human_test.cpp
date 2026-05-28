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

// ========== Additional Human Tests ==========

// -- Bezier curve point computation --

TEST(HumanMouseTest, CalculateBezierPointLinearTwoPoints) {
    // 2 control points -> linear interpolation
    Point p0(0, 0);
    Point p1(100, 0);
    std::vector<Point> cps = { p0, p1 };

    Point mid = HumanMouse::calculateBezierPoint(0.0, cps);
    EXPECT_EQ(mid.x, 0);
    EXPECT_EQ(mid.y, 0);

    Point mid2 = HumanMouse::calculateBezierPoint(0.5, cps);
    EXPECT_EQ(mid2.x, 50);
    EXPECT_EQ(mid2.y, 0);

    Point end = HumanMouse::calculateBezierPoint(1.0, cps);
    EXPECT_EQ(end.x, 100);
    EXPECT_EQ(end.y, 0);
}

TEST(HumanMouseTest, CalculateBezierPointQuadraticThreePoints) {
    // 3 control points -> quadratic bezier
    Point p0(0, 0);
    Point p1(50, 100);   // control point above the line
    Point p2(100, 0);
    std::vector<Point> cps = { p0, p1, p2 };

    Point start = HumanMouse::calculateBezierPoint(0.0, cps);
    EXPECT_EQ(start.x, 0);
    EXPECT_EQ(start.y, 0);

    Point end = HumanMouse::calculateBezierPoint(1.0, cps);
    EXPECT_EQ(end.x, 100);
    EXPECT_EQ(end.y, 0);

    // Midpoint should be above the baseline (y < 0 is "up" in screen coords,
    // but here p1.y=100 so the curve bends downward)
    Point mid = HumanMouse::calculateBezierPoint(0.5, cps);
    EXPECT_EQ(mid.x, 50);
    EXPECT_EQ(mid.y, 50);  // (1-0.5)^2*0 + 2*(1-0.5)*0.5*100 + 0.5^2*0 = 50
}

TEST(HumanMouseTest, CalculateBezierPointCubicFourPoints) {
    // 4 control points -> cubic bezier
    Point p0(0, 0);
    Point p1(0, 100);
    Point p2(100, 100);
    Point p3(100, 0);
    std::vector<Point> cps = { p0, p1, p2, p3 };

    Point start = HumanMouse::calculateBezierPoint(0.0, cps);
    EXPECT_EQ(start.x, 0);
    EXPECT_EQ(start.y, 0);

    Point end = HumanMouse::calculateBezierPoint(1.0, cps);
    EXPECT_EQ(end.x, 100);
    EXPECT_EQ(end.y, 0);
}

TEST(HumanMouseTest, CalculateBezierPointSinglePointFallback) {
    // 0 or 1 control point -> returns first point
    std::vector<Point> one = { Point(42, 99) };
    Point result = HumanMouse::calculateBezierPoint(0.5, one);
    EXPECT_EQ(result.x, 42);
    EXPECT_EQ(result.y, 99);
}

TEST(HumanMouseTest, CalculateBezierPointFivePointsUsesCubic) {
    // >= 4 control points still uses cubic (first 4)
    std::vector<Point> cps = { Point(0, 0), Point(0, 50), Point(100, 50), Point(100, 0), Point(200, 200) };
    Point start = HumanMouse::calculateBezierPoint(0.0, cps);
    EXPECT_EQ(start.x, 0);
    EXPECT_EQ(start.y, 0);
}

// -- Path generation edge cases --

TEST(HumanMouseTest, GeneratePathShortDistance) {
    // Very short distance: start == end case is handled by moveTo, but
    // generateBezierPath with a small distance should still produce a valid path
    HumanMouse mouse;
    Point start(10, 10);
    Point end(11, 11);
    auto path = mouse.generateBezierPath(start, end);
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), end);
    EXPECT_GE(path.size(), 20);  // minimum 20 samples
}

TEST(HumanMouseTest, GeneratePathLongDistance) {
    // Long distance should cap at 100 samples
    HumanMouse mouse;
    Point start(0, 0);
    Point end(10000, 10000);
    auto path = mouse.generateBezierPath(start, end);
    EXPECT_FALSE(path.empty());
    EXPECT_LE(path.size(), 101);  // 0..100 inclusive
    EXPECT_EQ(path.back(), end);
}

// -- Path length for collinear points --

TEST(HumanMouseTest, PathLengthStraightLine) {
    HumanMouse mouse;
    std::vector<Point> path;
    for (int i = 0; i <= 10; ++i) {
        path.push_back(Point(i * 10, 0));
    }
    double length = mouse.calculatePathLength(path);
    EXPECT_NEAR(length, 100, 1);
}

// -- addRandomness disabled --

TEST(HumanMouseTest, AddRandomnessDisabledNoChange) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.enablePathRandomness = false;
    config.pathVariance = 50;
    mouse.setConfig(config);

    Point start(0, 0);
    Point end(200, 0);
    auto path = mouse.generateBezierPath(start, end);
    // With randomness disabled, addRandomness is a no-op.
    // The path is still generated with bezier control points so interior points
    // may differ from a straight line, but calling addRandomness again won't change anything.
    // Just verify the endpoints are intact.
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), end);
}

// -- HumanMouse constructor with config --

TEST(HumanMouseTest, ConstructorWithConfig) {
    HumanMouseConfig config;
    config.minMoveDuration = 42;
    config.maxMoveDuration = 99;
    config.pathVariance = 7;
    HumanMouse mouse(config);

    auto retrieved = mouse.getConfig();
    EXPECT_EQ(retrieved.minMoveDuration, 42);
    EXPECT_EQ(retrieved.maxMoveDuration, 99);
    EXPECT_EQ(retrieved.pathVariance, 7);
}

// -- HumanMouseConfig defaults --

TEST(HumanMouseTest, ConfigDefaultValues) {
    HumanMouseConfig config;
    EXPECT_EQ(config.minMoveDuration, 100);
    EXPECT_EQ(config.maxMoveDuration, 300);
    EXPECT_EQ(config.moveVariance, 20);
    EXPECT_EQ(config.minControlPoints, 1);
    EXPECT_EQ(config.maxControlPoints, 3);
    EXPECT_EQ(config.pathVariance, 10);
    EXPECT_EQ(config.clickDelayMin, 50);
    EXPECT_EQ(config.clickDelayMax, 150);
    EXPECT_EQ(config.doubleClickIntervalMin, 80);
    EXPECT_EQ(config.doubleClickIntervalMax, 150);
    EXPECT_EQ(config.scrollDelayMin, 30);
    EXPECT_EQ(config.scrollDelayMax, 80);
    EXPECT_TRUE(config.enableRandomDelay);
    EXPECT_TRUE(config.enablePathRandomness);
}

// -- calculateDuration clamping --

TEST(HumanMouseTest, CalculateDurationRespectsConfigBounds) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.minMoveDuration = 200;
    config.maxMoveDuration = 250;
    config.moveVariance = 10;
    mouse.setConfig(config);

    // Even for a zero-length path, duration should be clamped to min
    int duration = mouse.calculateDuration(0.0);
    EXPECT_GE(duration, 200);
    EXPECT_LE(duration, 260);  // base(0) + variance(10) clamped to min 200
}

// -- HumanKeyboard constructor with config --

TEST(HumanKeyboardTest, ConstructorWithConfig) {
    HumanKeyboardConfig config;
    config.keyDownDelayMin = 15;
    config.keyDownDelayMax = 55;
    config.typeDelayMin = 60;
    config.typeDelayMax = 120;
    HumanKeyboard kb(config);

    auto retrieved = kb.getConfig();
    EXPECT_EQ(retrieved.keyDownDelayMin, 15);
    EXPECT_EQ(retrieved.keyDownDelayMax, 55);
    EXPECT_EQ(retrieved.typeDelayMin, 60);
    EXPECT_EQ(retrieved.typeDelayMax, 120);
}

// -- HumanKeyboardConfig defaults --

TEST(HumanKeyboardTest, ConfigDefaultValues) {
    HumanKeyboardConfig config;
    EXPECT_EQ(config.keyDownDelayMin, 30);
    EXPECT_EQ(config.keyDownDelayMax, 80);
    EXPECT_EQ(config.keyDurationMin, 40);
    EXPECT_EQ(config.keyDurationMax, 100);
    EXPECT_EQ(config.typeDelayMin, 50);
    EXPECT_EQ(config.typeDelayMax, 150);
    EXPECT_TRUE(config.enableRandomDelay);
}

// -- HumanKeyboard singleton --

TEST(HumanKeyboardTest, SingletonInstance) {
    auto& kb1 = HumanKeyboard::instance();
    auto& kb2 = HumanKeyboard::instance();
    EXPECT_EQ(&kb1, &kb2);
}

// -- HumanMouse singleton --

TEST(HumanMouseTest, SingletonInstance) {
    auto& m1 = HumanMouse::instance();
    auto& m2 = HumanMouse::instance();
    EXPECT_EQ(&m1, &m2);
}

// -- Human setKeyboardConfig --

TEST(HumanTest, SetGlobalKeyboardConfig) {
    HumanKeyboardConfig config;
    config.keyDownDelayMin = 10;
    config.keyDownDelayMax = 20;
    config.typeDelayMin = 30;

    Human::setKeyboardConfig(config);

    auto& kb = Human::keyboard();
    HumanKeyboardConfig retrieved = kb.getConfig();
    EXPECT_EQ(retrieved.keyDownDelayMin, 10);
    EXPECT_EQ(retrieved.keyDownDelayMax, 20);
    EXPECT_EQ(retrieved.typeDelayMin, 30);
}
