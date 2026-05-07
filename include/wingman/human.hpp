#pragma once

#include "wingman/screen.hpp"
#include <vector>
#include <random>
#include <string>

namespace wingman {

// 人性化鼠标配置
struct HumanMouseConfig {
    // 移动相关
    int minMoveDuration = 100;      // 最小移动时长 (ms)
    int maxMoveDuration = 300;      // 最大移动时长 (ms)
    int moveVariance = 20;          // 移动时长随机偏差

    // 贝塞尔曲线控制点数量
    int minControlPoints = 1;
    int maxControlPoints = 3;

    // 路径随机性
    int pathVariance = 10;          // 路径点随机偏移量 (像素)

    // 点击相关
    int clickDelayMin = 50;         // 点击前延迟范围
    int clickDelayMax = 150;
    int clickDurationMin = 50;      // 按下持续时间
    int clickDurationMax = 100;

    // 双击间隔
    int doubleClickIntervalMin = 80;
    int doubleClickIntervalMax = 150;

    // 滚动相关
    int scrollDelayMin = 30;
    int scrollDelayMax = 80;

    // 是否启用随机延迟
    bool enableRandomDelay = true;

    // 是否启用路径随机性
    bool enablePathRandomness = true;
};

// 人性化鼠标类
class HumanMouse {
public:
    HumanMouse();
    explicit HumanMouse(const HumanMouseConfig& config);

    // 配置管理
    void setConfig(const HumanMouseConfig& config);
    HumanMouseConfig getConfig() const;

    // 移动到目标位置（带贝塞尔曲线和随机性）
    void moveTo(int x, int y);
    void moveTo(const Point& target);

    // 移动到目标位置，指定大致时长
    void moveTo(int x, int y, int approximateDurationMs);

    // 点击（带随机延迟）
    void click(int x, int y);
    void click(const Point& pos);
    void rightClick(int x, int y);
    void doubleClick(int x, int y);

    // 拖拽
    void drag(int fromX, int fromY, int toX, int toY);
    void drag(const Point& from, const Point& to);

    // 滚动
    void scroll(int x, int y, int delta);

    // 获取当前鼠标位置
    Point getCurrentPosition() const;

    // 随机延迟
    void randomDelay(int minMs, int maxMs);

    // 静态便捷方法
    static HumanMouse& instance();

private:
    // 生成贝塞尔曲线路径点
    std::vector<Point> generateBezierPath(const Point& start, const Point& end);

    // 添加随机性到路径
    void addRandomness(std::vector<Point>& path);

    // 沿路径移动
    void moveAlongPath(const std::vector<Point>& path);

    // 计算路径长度
    double calculatePathLength(const std::vector<Point>& path) const;

    // 根据路径长度计算移动时长
    int calculateDuration(double pathLength) const;

    // 随机数生成
    int randomInt(int min, int max);
    double randomDouble(double min, double max);

    // 贝塞尔曲线计算
    static Point calculateBezierPoint(double t, const std::vector<Point>& controlPoints);

    HumanMouseConfig config_;
    std::mt19937 rng_;
};

// 人性化键盘配置
struct HumanKeyboardConfig {
    // 按键延迟
    int keyDownDelayMin = 30;
    int keyDownDelayMax = 80;
    int keyDurationMin = 40;
    int keyDurationMax = 100;

    // 输入文本延迟
    int typeDelayMin = 50;
    int typeDelayMax = 150;

    // 是否启用随机延迟
    bool enableRandomDelay = true;
};

// 人性化键盘类
class HumanKeyboard {
public:
    HumanKeyboard();
    explicit HumanKeyboard(const HumanKeyboardConfig& config);

    // 配置管理
    void setConfig(const HumanKeyboardConfig& config);
    HumanKeyboardConfig getConfig() const;

    // 按键（带随机延迟）
    void key(int vkCode);
    void keyDown(int vkCode);
    void keyUp(int vkCode);

    // 输入文本（带随机延迟）
    void type(const std::string& text);
    void type(const std::string& text, bool randomCase);

    // 静态便捷方法
    static HumanKeyboard& instance();

private:
    void randomDelay();

    HumanKeyboardConfig config_;
    std::mt19937 rng_;
};

// 人性化操作总接口
class Human {
public:
    static HumanMouse& mouse();
    static HumanKeyboard& keyboard();

    // 设置全局配置
    static void setMouseConfig(const HumanMouseConfig& config);
    static void setKeyboardConfig(const HumanKeyboardConfig& config);
};

} // namespace wingman
