#include "wingman/human.hpp"
#include "wingman/input.hpp"
#include "wingman/screen.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <random>

namespace wingman {

// ========== HumanMouse 实现 ==========

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
    return Input::getMousePosition();
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

// 二阶贝塞尔曲线: B(t) = (1-t)²P0 + 2(1-t)tP1 + t²P2
// 三阶贝塞尔曲线: B(t) = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3
Point HumanMouse::calculateBezierPoint(double t, const std::vector<Point>& controlPoints) {
    if (controlPoints.size() == 2) {
        // 线性插值
        double x = (1 - t) * controlPoints[0].x + t * controlPoints[1].x;
        double y = (1 - t) * controlPoints[0].y + t * controlPoints[1].y;
        return Point(static_cast<int>(x), static_cast<int>(y));
    } else if (controlPoints.size() == 3) {
        // 二阶贝塞尔
        double t2 = t * t;
        double mt = 1 - t;
        double mt2 = mt * mt;
        double x = mt2 * controlPoints[0].x + 2 * mt * t * controlPoints[1].x + t2 * controlPoints[2].x;
        double y = mt2 * controlPoints[0].y + 2 * mt * t * controlPoints[1].y + t2 * controlPoints[2].y;
        return Point(static_cast<int>(x), static_cast<int>(y));
    } else if (controlPoints.size() >= 4) {
        // 三阶贝塞尔
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
    std::vector<Point> controlPoints;
    controlPoints.push_back(start);

    // 生成随机控制点数量（限制为1或2，避免过多控制点导致复杂计算）
    int numControlPoints = randomInt(config_.minControlPoints, std::min(config_.maxControlPoints, 2));

    // 计算中间控制点
    int deltaX = end.x - start.x;
    int deltaY = end.y - start.y;
    double distance = std::sqrt(static_cast<double>(deltaX * deltaX + deltaY * deltaY));

    for (int i = 0; i < numControlPoints; ++i) {
        double t = static_cast<double>(i + 1) / (numControlPoints + 1);

        // 基础控制点在线段上
        int baseX = start.x + static_cast<int>(deltaX * t);
        int baseY = start.y + static_cast<int>(deltaY * t);

        // 添加垂直于线段的随机偏移
        // 垂直方向向量
        double perpX = -deltaY / distance;
        double perpY = deltaX / distance;

        // 随机偏移量
        double offset = randomDouble(-config_.pathVariance, config_.pathVariance);

        int ctrlX = baseX + static_cast<int>(perpX * offset);
        int ctrlY = baseY + static_cast<int>(perpY * offset);

        controlPoints.push_back(Point(ctrlX, ctrlY));
    }

    controlPoints.push_back(end);

    // 生成路径点（采样贝塞尔曲线）
    std::vector<Point> path;
    int numSamples = static_cast<int>(distance / 5);  // 每5像素采样一次
    numSamples = std::max(numSamples, 20);  // 至少20个点
    numSamples = std::min(numSamples, 100); // 最多100个点

    for (int i = 0; i <= numSamples; ++i) {
        double t = static_cast<double>(i) / numSamples;
        path.push_back(calculateBezierPoint(t, controlPoints));
    }

    // 确保最后一个点就是终点（修正浮点误差）
    if (!path.empty()) {
        path.back() = end;
    }

    return path;
}

void HumanMouse::addRandomness(std::vector<Point>& path) {
    if (!config_.enablePathRandomness) return;

    for (auto& point : path) {
        // 跳过起点和终点
        if (&point == &path.front() || &point == &path.back()) continue;

        // 添加小幅随机偏移
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
    // 根据路径长度计算时长：约 1000-2000 像素/秒
    double pixelsPerMs = 2.0;  // 平均速度
    int baseDuration = static_cast<int>(pathLength / pixelsPerMs);

    // 添加随机性并限制在配置范围内
    int variance = randomInt(-config_.moveVariance, config_.moveVariance);
    int duration = baseDuration + variance;

    // 限制在最小和最大时长之间
    duration = std::max(duration, config_.minMoveDuration);
    duration = std::min(duration, config_.maxMoveDuration);

    return duration;
}

void HumanMouse::moveAlongPath(const std::vector<Point>& path) {
    if (path.empty()) return;

    int totalDuration = calculateDuration(calculatePathLength(path));

    // 计算每个点之间的延迟
    int numSegments = static_cast<int>(path.size()) - 1;
    if (numSegments <= 0) {
        Input::move(path[0].x, path[0].y);
        return;
    }

    int delayPerSegment = totalDuration / numSegments;

    // 沿路径移动
    for (size_t i = 0; i < path.size(); ++i) {
        Input::move(path[i].x, path[i].y);

        // 最后一个点不需要延迟
        if (i < path.size() - 1) {
            // 添加小的随机变化
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

    // 如果已经在目标位置，直接返回
    if (start.x == target.x && start.y == target.y) {
        return;
    }

    // 生成贝塞尔曲线路径
    std::vector<Point> path = generateBezierPath(start, target);

    // 添加随机性
    addRandomness(path);

    // 沿路径移动
    moveAlongPath(path);
}

void HumanMouse::moveTo(int x, int y, int approximateDurationMs) {
    Point start = getCurrentPosition();

    if (start.x == x && start.y == y) return;

    // 临时修改配置以匹配指定时长
    HumanMouseConfig originalConfig = config_;
    config_.minMoveDuration = approximateDurationMs - config_.moveVariance;
    config_.maxMoveDuration = approximateDurationMs + config_.moveVariance;

    moveTo(Point(x, y));

    config_ = originalConfig;
}

void HumanMouse::click(int x, int y) {
    click(Point(x, y));
}

void HumanMouse::click(const Point& pos) {
    // 移动到目标位置
    moveTo(pos.x, pos.y);

    // 点击前随机延迟
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // 点击
    Input::click(pos.x, pos.y);

    spdlog::debug("HumanMouse: clicked at ({}, {})", pos.x, pos.y);
}

void HumanMouse::rightClick(int x, int y) {
    // 移动到目标位置
    moveTo(x, y);

    // 点击前随机延迟
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // 右键点击
    Input::click(x, y, MouseButton::Right);

    spdlog::debug("HumanMouse: right-clicked at ({}, {})", x, y);
}

void HumanMouse::doubleClick(int x, int y) {
    // 移动到目标位置
    moveTo(x, y);

    // 点击前随机延迟
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // 第一次点击
    Input::click(x, y);

    // 双击间隔延迟
    randomDelay(config_.doubleClickIntervalMin, config_.doubleClickIntervalMax);

    // 第二次点击
    Input::click(x, y);

    spdlog::debug("HumanMouse: double-clicked at ({}, {})", x, y);
}

void HumanMouse::drag(int fromX, int fromY, int toX, int toY) {
    drag(Point(fromX, fromY), Point(toX, toY));
}

void HumanMouse::drag(const Point& from, const Point& to) {
    // 移动到起始位置
    moveTo(from.x, from.y);

    // 按下前随机延迟
    randomDelay(config_.clickDelayMin, config_.clickDelayMax);

    // 按下鼠标
    Input::mouseDown(MouseButton::Left);

    // 按下持续时间
    randomDelay(config_.clickDurationMin, config_.clickDurationMax);

    // 移动到目标位置（保持按下状态）
    moveTo(to.x, to.y);

    // 释放前延迟
    randomDelay(config_.clickDurationMin, config_.clickDurationMax);

    // 释放鼠标
    Input::mouseUp(MouseButton::Left);

    spdlog::debug("HumanMouse: dragged from ({}, {}) to ({}, {})", from.x, from.y, to.x, to.y);
}

void HumanMouse::scroll(int x, int y, int delta) {
    // 移动到目标位置
    moveTo(x, y);

    // 滚动前延迟
    randomDelay(config_.scrollDelayMin, config_.scrollDelayMax);

    // 滚动
    Input::scroll(x, y, delta);

    spdlog::debug("HumanMouse: scrolled at ({}, {}) by {}", x, y, delta);
}

// ========== HumanKeyboard 实现 ==========

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
    Input::key(vkCode);
    spdlog::debug("HumanKeyboard: keyed VK code {}", vkCode);
}

void HumanKeyboard::keyDown(int vkCode) {
    randomDelay();
    Input::keyDown(vkCode);
    spdlog::debug("HumanKeyboard: key down VK code {}", vkCode);
}

void HumanKeyboard::keyUp(int vkCode) {
    Input::keyUp(vkCode);
    spdlog::debug("HumanKeyboard: key up VK code {}", vkCode);
}

void HumanKeyboard::type(const std::string& text) {
    type(text, false);
}

void HumanKeyboard::type(const std::string& text, bool randomCase) {
    for (char c : text) {
        // 随机大小写
        if (randomCase && std::isalpha(c)) {
            if (randomInt(0, 1) == 1) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            } else {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }

        // 每个字符之间有随机延迟
        int delay = randomInt(config_.typeDelayMin, config_.typeDelayMax);
        Input::type(std::string(1, c));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    spdlog::debug("HumanKeyboard: typed text (length={})", text.length());
}

// ========== Human 总接口实现 ==========

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
