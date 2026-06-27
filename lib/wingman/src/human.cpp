#include "wingman/human.hpp"

#include "wingman/platform/input_factory.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>

namespace wingman {

namespace {

platform::IInput& getInput() {
    static std::shared_ptr<platform::IInput> input = platform::defaultSharedInput();
    return *input;
}

}

// ========== HumanMouse Implementation ==========

HumanMouse::HumanMouse() : rng_(std::random_device{}()) {
}

HumanMouse::HumanMouse(const HumanMouseConfig& config)
    : config_(config), rng_(std::random_device{}()) {
}

void HumanMouse::setConfig(const HumanMouseConfig& config) {
    config_ = config;
}

HumanMouseConfig HumanMouse::getConfig() const {
    return config_;
}

HumanMouse& HumanMouse::instance() {
    static HumanMouse instance;
    return instance;
}

Point HumanMouse::getCurrentPosition() const {
    auto p = getInput().getMousePosition();
    return Point{p.x, p.y};
}

int HumanMouse::randomInt(int min, int max) const {
    if (min == max) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

double HumanMouse::randomDouble(double min, double max) const {
    if (min == max) return min;
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng_);
}

void HumanMouse::randomDelay(int minMs, int maxMs) {
    if (config_.enableRandomDelay) {
        int delay = randomInt(minMs, maxMs);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds((minMs + maxMs) / 2));
    }
}

// Quadratic Bezier curve: B(t) = (1-t)^2*P0 + 2(1-t)*t*P1 + t^2*P2
// Cubic Bezier curve: B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
Point HumanMouse::calculateBezierPoint(double t, const std::vector<Point>& controlPoints) {
    if (controlPoints.size() == 2) {
        // Linear interpolation
        double x = (1 - t) * controlPoints[0].x + t * controlPoints[1].x;
        double y = (1 - t) * controlPoints[0].y + t * controlPoints[1].y;
        return Point(static_cast<int>(x), static_cast<int>(y));
    } else if (controlPoints.size() == 3) {
        // Quadratic Bezier
        double t2 = t * t;
        double mt = 1 - t;
        double mt2 = mt * mt;
        double x = mt2 * controlPoints[0].x + 2 * mt * t * controlPoints[1].x + t2 * controlPoints[2].x;
        double y = mt2 * controlPoints[0].y + 2 * mt * t * controlPoints[1].y + t2 * controlPoints[2].y;
        return Point(static_cast<int>(x), static_cast<int>(y));
    } else if (controlPoints.size() >= 4) {
        // Cubic Bezier
        double t2 = t * t;
        double t3 = t2 * t;
        double mt = 1 - t;
        double mt2 = mt * mt;
        double mt3 = mt2 * mt;
        double x = mt3 * controlPoints[0].x +
                   3 * mt2 * t * controlPoints[1].x +
                   3 * mt * t2 * controlPoints[2].x +
                   t3 * controlPoints[3].x;
        double y = mt3 * controlPoints[0].y +
                   3 * mt2 * t * controlPoints[1].y +
                   3 * mt * t2 * controlPoints[2].y +
                   t3 * controlPoints[3].y;
        return Point(static_cast<int>(x), static_cast<int>(y));
    }
    return controlPoints[0];
}

std::vector<Point> HumanMouse::generateBezierPath(const Point& start, const Point& end) {
    if (start == end) {
        return {start};
    }

    std::vector<Point> controlPoints;
    controlPoints.push_back(start);

    // Generate random control point count (limited to max 3, avoid too many control points causing complex calculations)
    int numControlPoints = randomInt(config_.minControlPoints, std::min(config_.maxControlPoints, 3));

    // Calculate intermediate control points
    int deltaX = end.x - start.x;
    int deltaY = end.y - start.y;
    double distance = std::sqrt(static_cast<double>(deltaX * deltaX + deltaY * deltaY));

    for (int i = 0; i < numControlPoints; ++i) {
        double t = static_cast<double>(i + 1) / (numControlPoints + 1);

        // Base control point on line segment
        int baseX = start.x + static_cast<int>(deltaX * t);
        int baseY = start.y + static_cast<int>(deltaY * t);

        // Add random offset perpendicular to line segment
        // Perpendicular direction vector
        double perpX = -deltaY / distance;
        double perpY = deltaX / distance;

        // Random offset amount
        double offset = randomDouble(-config_.pathVariance, config_.pathVariance);

        int ctrlX = baseX + static_cast<int>(perpX * offset);
        int ctrlY = baseY + static_cast<int>(perpY * offset);

        controlPoints.push_back(Point(ctrlX, ctrlY));
    }

    controlPoints.push_back(end);

    // Generate path points (sample Bezier curve)
    std::vector<Point> path;
    int numSamples = static_cast<int>(distance / 5);  // Sample every 5 pixels
    numSamples = std::max(numSamples, 20);  // At least 20 points
    numSamples = std::min(numSamples, 100); // At most 100 points

    for (int i = 0; i <= numSamples; ++i) {
        double t = static_cast<double>(i) / numSamples;
        path.push_back(calculateBezierPoint(t, controlPoints));
    }

    // Ensure last point is exactly the endpoint (fix floating-point error)
    if (!path.empty()) {
        path.back() = end;
    }

    return path;
}

void HumanMouse::addRandomness(std::vector<Point>& path) {
    if (!config_.enablePathRandomness) return;

    for (auto& point : path) {
        // Skip start and end points
        if (&point == &path.front() || &point == &path.back()) continue;

        // Add small random offset
        int offsetX = randomInt(-config_.pathVariance / 2, config_.pathVariance / 2);
        int offsetY = randomInt(-config_.pathVariance / 2, config_.pathVariance / 2);
        point.x += offsetX;
        point.y += offsetY;
    }
}

double HumanMouse::calculatePathLength(const std::vector<Point>& path) const {
    double length = 0;
    for (size_t i = 1; i < path.size(); ++i) {
        int dx = path[i].x - path[i - 1].x;
        int dy = path[i].y - path[i - 1].y;
        length += std::sqrt(static_cast<double>(dx * dx + dy * dy));
    }
    return length;
}

int HumanMouse::calculateDuration(double pathLength) const {
    // Calculate duration based on path length: approx 1000-2000 pixels/sec
    double pixelsPerMs = 2.0;  // Average speed
    int baseDuration = static_cast<int>(pathLength / pixelsPerMs);

    // Add randomness and clamp within config range
    int variance = randomInt(-config_.moveVariance, config_.moveVariance);
    int duration = baseDuration + variance;

    // Clamp between min and max duration
    duration = std::max(duration, config_.minMoveDuration);
    duration = std::min(duration, config_.maxMoveDuration);

    return duration;
}

void HumanMouse::moveAlongPath(const std::vector<Point>& path) {
    if (path.empty()) return;

    int totalDuration = calculateDuration(calculatePathLength(path));

    // Calculate delay between each point
    int numSegments = static_cast<int>(path.size()) - 1;
    if (numSegments <= 0) {
        getInput().mouseMove(path[0].x, path[0].y);
        return;
    }

    int delayPerSegment = totalDuration / numSegments;

    // Move along path
    for (size_t i = 0; i < path.size(); ++i) {
        getInput().mouseMove(path[i].x, path[i].y);

        // No delay needed for last point
        if (i < path.size() - 1) {
            // Add small random variation
            int actualDelay = delayPerSegment + randomInt(-5, 5);
            actualDelay = std::max(actualDelay, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(actualDelay));
        }
    }
}

void HumanMouse::moveTo(int x, int y) {
    moveTo(Point(x, y));
}

void HumanMouse::moveTo(const Point& target) {
    Point start = getCurrentPosition();

    // If already at target position, return directly
    if (start.x == target.x && start.y == target.y) {
        return;
    }

    // Generate Bezier curve path
    std::vector<Point> path = generateBezierPath(start, target);

    // Add randomness
    addRandomness(path);

    // Move along path
    moveAlongPath(path);
}

void HumanMouse::moveTo(int x, int y, int approximateDurationMs) {
    Point start = getCurrentPosition();

    if (start.x == x && start.y == y) return;

    // Temporarily modify config to match specified duration
    HumanMouseConfig originalConfig = config_;
    config_.minMoveDuration = approximateDurationMs - config_.moveVariance;
    config_.maxMoveDuration = approximateDurationMs + config_.moveVariance;

    moveTo(Point(x, y));

    config_ = originalConfig;
}

void HumanMouse::moveTo(const Point& from, const Point& to, int approximateDurationMs) {
    // 起点与终点重合：仅定位，避免空路径
    if (from.x == to.x && from.y == to.y) {
        getInput().mouseMove(to.x, to.y);
        return;
    }

    // 临时将移动时长范围约束到指定 duration（复用现有 duration 机制）
    HumanMouseConfig originalConfig = config_;
    config_.minMoveDuration = approximateDurationMs - config_.moveVariance;
    config_.maxMoveDuration = approximateDurationMs + config_.moveVariance;

    // 从显式起点生成贝塞尔路径并移动（addRandomness/moveAlongPath 为 private 成员，可直接调用）
    std::vector<Point> path = generateBezierPath(from, to);
    addRandomness(path);
    moveAlongPath(path);

    config_ = originalConfig;
}

void HumanMouse::click(int x, int y) {
    click(Point(x, y));
}

void HumanMouse::click(const Point& pos) {
    // Move to target position
    moveTo(pos.x, pos.y);

    // Random delay before click
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // Click
    getInput().mouseMove(pos.x, pos.y);
    getInput().mouseClick(platform::MouseButton::Left);

    spdlog::debug("HumanMouse: clicked at ({}, {})", pos.x, pos.y);
}

void HumanMouse::rightClick(int x, int y) {
    // Move to target position
    moveTo(x, y);

    // Random delay before click
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // Right click
    getInput().mouseMove(x, y);
    getInput().mouseClick(platform::MouseButton::Right);

    spdlog::debug("HumanMouse: right-clicked at ({}, {})", x, y);
}

void HumanMouse::middleClick(int x, int y) {
    // Move to target position
    moveTo(x, y);

    // Random delay before click
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // Middle click
    getInput().mouseMove(x, y);
    getInput().mouseClick(platform::MouseButton::Middle);

    spdlog::debug("HumanMouse: middle-clicked at ({}, {})", x, y);
}

void HumanMouse::doubleClick(int x, int y) {
    // Move to target position
    moveTo(x, y);

    // Random delay before click
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // First click
    getInput().mouseMove(x, y);
    getInput().mouseClick(platform::MouseButton::Left);

    // Double-click interval delay
    randomDelay(config_.doubleClickIntervalMin, config_.doubleClickIntervalMax);

    // Second click
    getInput().mouseMove(x, y);
    getInput().mouseClick(platform::MouseButton::Left);

    spdlog::debug("HumanMouse: double-clicked at ({}, {})", x, y);
}

void HumanMouse::drag(int fromX, int fromY, int toX, int toY) {
    drag(Point(fromX, fromY), Point(toX, toY));
}

void HumanMouse::drag(const Point& from, const Point& to) {
    // Move to start position
    moveTo(from.x, from.y);

    // Random delay before press
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // Press mouse
    getInput().mouseDown(platform::MouseButton::Left);

    // Press duration
    randomDelay(config_.clickDurationMin, config_.clickDurationMax);

    // Move to target position (keep pressed)
    moveTo(to.x, to.y);

    // Delay before release
    randomDelay(config_.clickDurationMin, config_.clickDurationMax);

    // Release mouse
    getInput().mouseUp(platform::MouseButton::Left);

    spdlog::debug("HumanMouse: dragged from ({}, {}) to ({}, {})", from.x, from.y, to.x, to.y);
}

void HumanMouse::scroll(int x, int y, int delta) {
    // Move to target position
    moveTo(x, y);

    // Delay before scroll
    randomDelay(config_.scrollDelayMin, config_.scrollDelayMax);

    // Scroll
    getInput().mouseMove(x, y);
    getInput().mouseWheel(delta);

    spdlog::debug("HumanMouse: scrolled at ({}, {}) by {}", x, y, delta);
}

// ========== HumanKeyboard Implementation ==========

HumanKeyboard::HumanKeyboard() : rng_(std::random_device{}()) {
}

HumanKeyboard::HumanKeyboard(const HumanKeyboardConfig& config)
    : config_(config), rng_(std::random_device{}()) {
}

void HumanKeyboard::setConfig(const HumanKeyboardConfig& config) {
    config_ = config;
}

HumanKeyboardConfig HumanKeyboard::getConfig() const {
    return config_;
}

HumanKeyboard& HumanKeyboard::instance() {
    static HumanKeyboard instance;
    return instance;
}

int HumanKeyboard::randomInt(int min, int max) const {
    if (min == max) return min;
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

void HumanKeyboard::randomDelay() {
    if (config_.enableRandomDelay) {
        int delay = randomInt(config_.keyDownDelayMin, config_.keyDownDelayMax);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
}

void HumanKeyboard::key(int vkCode) {
    randomDelay();
    getInput().keyPress(static_cast<platform::KeyCode>(vkCode));
    spdlog::debug("HumanKeyboard: keyed VK code {}", vkCode);
}

void HumanKeyboard::keyDown(int vkCode) {
    randomDelay();
    getInput().keyDown(static_cast<platform::KeyCode>(vkCode));
    spdlog::debug("HumanKeyboard: key down VK code {}", vkCode);
}

void HumanKeyboard::keyUp(int vkCode) {
    getInput().keyUp(static_cast<platform::KeyCode>(vkCode));
    spdlog::debug("HumanKeyboard: key up VK code {}", vkCode);
}

void HumanKeyboard::type(const std::string& text) {
    type(text, false);
}

void HumanKeyboard::type(const std::string& text, bool randomCase) {
    for (char c : text) {
        // Random case
        if (randomCase && std::isalpha(c)) {
            if (randomInt(0, 1) == 1) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            } else {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }

        // Random delay between each character
        int delay = randomInt(config_.typeDelayMin, config_.typeDelayMax);
        getInput().textInput(std::string(1, c));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    spdlog::debug("HumanKeyboard: typed text (length={})", text.length());
}

// ========== Human Main Interface Implementation ==========

HumanMouse& Human::mouse() {
    return HumanMouse::instance();
}

HumanKeyboard& Human::keyboard() {
    return HumanKeyboard::instance();
}

void Human::setMouseConfig(const HumanMouseConfig& config) {
    Human::mouse().setConfig(config);
}

void Human::setKeyboardConfig(const HumanKeyboardConfig& config) {
    Human::keyboard().setConfig(config);
}

} // namespace wingman
