#pragma once

#include "wingman/screen.hpp"
#include <vector>
#include <random>
#include <string>

namespace wingman {

// Human-like mouse configuration
struct HumanMouseConfig {
    // Movement related
    int minMoveDuration = 100;      // Minimum move duration (ms)
    int maxMoveDuration = 300;      // Maximum move duration (ms)
    int moveVariance = 20;          // Move duration random deviation

    // Number of Bezier curve control points
    int minControlPoints = 1;
    int maxControlPoints = 3;

    // Path randomness
    int pathVariance = 10;          // Path point random offset (pixels)

    // Click related
    int clickDelayMin = 50;         // Pre-click delay range
    int clickDelayMax = 150;
    int clickDurationMin = 50;      // Press duration
    int clickDurationMax = 100;

    // Double-click interval
    int doubleClickIntervalMin = 80;
    int doubleClickIntervalMax = 150;

    // Scroll related
    int scrollDelayMin = 30;
    int scrollDelayMax = 80;

    // Whether to enable random delay
    bool enableRandomDelay = true;

    // Whether to enable path randomness
    bool enablePathRandomness = true;
};

// Human-like mouse class
class HumanMouse {
public:
    HumanMouse();
    explicit HumanMouse(const HumanMouseConfig& config);

    // Profile management
    void setConfig(const HumanMouseConfig& config);
    HumanMouseConfig getConfig() const;

    // Move to target position (with Bezier curve and randomness)
    void moveTo(int x, int y);
    void moveTo(const Point& target);

    // Move to target position, specifying approximate duration
    void moveTo(int x, int y, int approximateDurationMs);

    // Move from an explicit start point to target along a Bezier path (Plan 5)
    void moveTo(const Point& from, const Point& to, int approximateDurationMs);

    // Click (with random delay)
    void click(int x, int y);
    void click(const Point& pos);
    void rightClick(int x, int y);
    void doubleClick(int x, int y);

    // Drag
    void drag(int fromX, int fromY, int toX, int toY);
    void drag(const Point& from, const Point& to);

    // Scroll
    void scroll(int x, int y, int delta);

    // Get current mouse position
    Point getCurrentPosition() const;

    // Random delay
    void randomDelay(int minMs, int maxMs);

    // Static convenience methods
    static HumanMouse& instance();

    // Test support methods
    std::vector<Point> generateBezierPath(const Point& start, const Point& end);
    double calculatePathLength(const std::vector<Point>& path) const;

private:
    // Add randomness to path
    void addRandomness(std::vector<Point>& path);

    // Move along path
    void moveAlongPath(const std::vector<Point>& path);

    // Calculate move duration based on path length
    int calculateDuration(double pathLength) const;

    // Random number generation
    int randomInt(int min, int max) const;
    double randomDouble(double min, double max) const;

    // Bezier curve calculation
    static Point calculateBezierPoint(double t, const std::vector<Point>& controlPoints);

    HumanMouseConfig config_;
    mutable std::mt19937 rng_;
};

// Human-like keyboard configuration
struct HumanKeyboardConfig {
    // Key delay
    int keyDownDelayMin = 30;
    int keyDownDelayMax = 80;
    int keyDurationMin = 40;
    int keyDurationMax = 100;

    // Text input delay
    int typeDelayMin = 50;
    int typeDelayMax = 150;

    // Whether to enable random delay
    bool enableRandomDelay = true;
};

// Human-like keyboard class
class HumanKeyboard {
public:
    HumanKeyboard();
    explicit HumanKeyboard(const HumanKeyboardConfig& config);

    // Profile management
    void setConfig(const HumanKeyboardConfig& config);
    HumanKeyboardConfig getConfig() const;

    // Key press (with random delay)
    void key(int vkCode);
    void keyDown(int vkCode);
    void keyUp(int vkCode);

    // Type text (with random delay)
    void type(const std::string& text);
    void type(const std::string& text, bool randomCase);

    // Static convenience methods
    static HumanKeyboard& instance();

    // Test support methods
    void randomDelay();

private:
    int randomInt(int min, int max) const;

    HumanKeyboardConfig config_;
    mutable std::mt19937 rng_;
};

// Human-like operations master interface
class Human {
public:
    static HumanMouse& mouse();
    static HumanKeyboard& keyboard();

    // Set global configuration
    static void setMouseConfig(const HumanMouseConfig& config);
    static void setKeyboardConfig(const HumanKeyboardConfig& config);
};

} // namespace wingman
