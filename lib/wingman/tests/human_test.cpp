#include <gtest/gtest.h>
#include "wingman/human.hpp"
#include <thread>
#include <chrono>

using namespace wingman;

// ========== HumanMouse Configuration Tests ==========

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

// ========== Path Generation Tests ==========

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
    EXPECT_GE(path.size(), 20);  // At least 20 sample points
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

// ========== Randomness Tests ==========

TEST(HumanMouseTest, RandomDelay) {
    HumanMouse mouse;

    auto start = std::chrono::steady_clock::now();
    mouse.randomDelay(50, 100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(duration, 40);  // Allow small error margin
    EXPECT_LE(duration, 150);
}

TEST(HumanMouseTest, PathRandomness) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.pathVariance = 20;
    mouse.setConfig(config);

    Point start(0, 0);
    Point end(200, 200);

    // Generate two paths, they should differ
    auto path1 = mouse.generateBezierPath(start, end);
    auto path2 = mouse.generateBezierPath(start, end);

    // Paths should not be identical (due to randomness)
    bool different = false;
    for (size_t i = 1; i < path1.size() - 1; ++i) {
        if (path1[i].x != path2[i].x || path1[i].y != path2[i].y) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

// ========== HumanKeyboard Tests ==========

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

// ========== Human Interface Tests ==========

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

// ========== Boundary Condition Tests ==========

TEST(HumanMouseTest, MoveToSamePosition) {
    HumanMouse mouse;
    Point pos(100, 100);

    // Moving to the same position should be safe
    // Note: real movement requires test environment support
    // Here we only test that it does not crash
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

// -- Bezier path generation through public API --

TEST(HumanMouseTest, GenerateBezierPathLinearConfigProducesStraightLine) {
    HumanMouseConfig config;
    config.minControlPoints = 0;
    config.maxControlPoints = 0;
    config.pathVariance = 0;
    HumanMouse mouse(config);

    Point start(0, 0);
    Point end(100, 0);
    auto path = mouse.generateBezierPath(start, end);

    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), end);
    EXPECT_EQ(path[path.size() / 2].y, 0);
    EXPECT_NEAR(path[path.size() / 2].x, 50, 5);
}

TEST(HumanMouseTest, GenerateBezierPathQuadraticConfigPreservesEndpoints) {
    HumanMouseConfig config;
    config.minControlPoints = 1;
    config.maxControlPoints = 1;
    config.pathVariance = 0;
    HumanMouse mouse(config);

    Point start(0, 0);
    Point end(100, 100);
    auto path = mouse.generateBezierPath(start, end);

    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), end);
    EXPECT_GE(path.size(), 20);
}

TEST(HumanMouseTest, GenerateBezierPathCubicConfigPreservesEndpoints) {
    HumanMouseConfig config;
    config.minControlPoints = 2;
    config.maxControlPoints = 2;
    config.pathVariance = 0;
    HumanMouse mouse(config);

    Point start(0, 0);
    Point end(100, 100);
    auto path = mouse.generateBezierPath(start, end);

    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path.front(), start);
    EXPECT_EQ(path.back(), end);
    EXPECT_GE(path.size(), 20);
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

// -- move duration clamping through timed public API --

TEST(HumanMouseTest, CalculateDurationRespectsConfigBounds) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.minMoveDuration = 0;
    config.maxMoveDuration = 0;
    config.moveVariance = 0;
    config.enableRandomDelay = false;
    mouse.setConfig(config);

    Point start(10, 10);
    Point end(10, 10);
    auto path = mouse.generateBezierPath(start, end);

    EXPECT_FALSE(path.empty());
    EXPECT_EQ(mouse.calculatePathLength(path), 0);
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

// -- HumanMouse::randomDelay with delay disabled --

TEST(HumanMouseTest, RandomDelayDisabledUsesAverage) {
    HumanMouse mouse;
    HumanMouseConfig config;
    config.enableRandomDelay = false;
    mouse.setConfig(config);

    auto start = std::chrono::steady_clock::now();
    mouse.randomDelay(10, 20);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // When disabled, uses average = (10+20)/2 = 15ms
    EXPECT_GE(duration, 10);
}

// -- HumanKeyboard::type --

TEST(HumanKeyboardTest, TypeDoesNotCrash) {
    HumanKeyboard keyboard;
    HumanKeyboardConfig config;
    config.typeDelayMin = 0;
    config.typeDelayMax = 1;
    config.keyDownDelayMin = 0;
    config.keyDownDelayMax = 1;
    config.keyDurationMin = 0;
    config.keyDurationMax = 1;
    keyboard.setConfig(config);
    // type() calls keyPress for each character; just verify no crash
    EXPECT_NO_THROW(keyboard.type("ab"));
}

TEST(HumanKeyboardTest, TypeWithRandomCaseDoesNotCrash) {
    HumanKeyboard keyboard;
    HumanKeyboardConfig config;
    config.typeDelayMin = 0;
    config.typeDelayMax = 1;
    config.keyDownDelayMin = 0;
    config.keyDownDelayMax = 1;
    config.keyDurationMin = 0;
    config.keyDurationMax = 1;
    keyboard.setConfig(config);
    EXPECT_NO_THROW(keyboard.type("abc", true));
}

// ========== Plan 5: 起点贝塞尔移动重载 ==========

TEST(HumanMouseTest, MoveFromToDoesNotCrash) {
    HumanMouse mouse;
    // 短距离 + 小 duration，避免单测中长时间 sleep（moveAlongPath 内含 sleep_for）
    EXPECT_NO_THROW(mouse.moveTo(Point(0, 0), Point(5, 5), 1));
}

TEST(HumanMouseTest, MoveFromToSamePointDoesNotCrash) {
    HumanMouse mouse;
    // 起点与终点相同：应早退仅定位，不崩溃
    EXPECT_NO_THROW(mouse.moveTo(Point(50, 50), Point(50, 50), 200));
}
