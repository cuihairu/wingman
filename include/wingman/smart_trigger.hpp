#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>
#include "wingman/vision.hpp"
#include "wingman/ocr.hpp"

namespace wingman {

// 触发条件类型
enum class TriggerConditionType {
    COLOR_FOUND,        // 颜色出现
    COLOR_NOT_FOUND,    // 颜色消失
    IMAGE_FOUND,        // 图像出现
    IMAGE_NOT_FOUND,    // 图像消失
    TEXT_FOUND,         // 文字出现
    TEXT_NOT_FOUND,     // 文字消失
    EDGE_DETECTED,      // 边缘检测
    COLOR_CHANGED,      // 颜色变化
    OCR_CONTAINS,       // OCR 包含文本
    OCR_EQUALS,         // OCR 等于文本
};

// 触发动作类型
enum class TriggerActionType {
    CLICK,              // 点击
    KEY_PRESS,          // 按键
    WAIT,               // 等待
    LUA_SCRIPT,         // 执行 Lua 脚本
    CUSTOM_CALLBACK,           // 回调函数
    STOP,               // 停止触发器
    LOG                 // 日志
};

// 触发条件配置
struct TriggerCondition {
    TriggerConditionType type;
    Color targetColor;               // 用于颜色相关
    int tolerance = 0;               // 颜色容差
    std::string templatePath;        // 用于图像匹配
    std::string targetText;          // 用于文字识别
    Rect searchRegion = {};          // 搜索区域
    double threshold = 0.8;          // 匹配阈值
    Color previousColor;             // 用于颜色变化检测
    bool hasPreviousColor = false;   // 是否有之前的颜色
};

// 触发动作配置
struct TriggerAction {
    TriggerActionType type;
    Point clickPosition = {};        // 点击位置
    int keyCode = 0;                 // 按键代码
    int waitMs = 0;                  // 等待时间
    std::string luaScript;           // Lua 脚本内容
    std::string logMessage;          // 日志消息
    std::function<void()> callback;  // 回调函数
};

// 智能触发器
class SmartTrigger {
public:
    SmartTrigger(const std::string& name);
    ~SmartTrigger();

    // 添加条件
    void addCondition(const TriggerCondition& condition);

    // 添加动作
    void addAction(const TriggerAction& action);

    // 设置检查间隔（毫秒）
    void setCheckInterval(int intervalMs);

    // 设置最大触发次数（0 = 无限）
    void setMaxTriggers(int maxCount);

    // 开始监听
    bool start();

    // 停止监听
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_; }

    // 获取触发次数
    int getTriggerCount() const { return triggerCount_; }

    // 重置触发次数
    void resetTriggerCount() { triggerCount_ = 0; }

    // 获取名称
    const std::string& getName() const { return name_; }

private:
    std::string name_;
    std::vector<TriggerCondition> conditions_;
    std::vector<TriggerAction> actions_;
    int checkIntervalMs_ = 100;
    int maxTriggers_ = 0;
    std::atomic<int> triggerCount_{0};
    std::atomic<bool> running_{false};
    std::thread watchThread_;

    // 检查条件
    bool checkConditions();

    // 执行动作
    void executeActions();

    // 监视线程
    void watchLoop();
};

// 智能触发器管理器
class SmartTriggerManager {
public:
    static SmartTriggerManager& instance();

    // 创建触发器
    std::shared_ptr<SmartTrigger> createTrigger(const std::string& name);

    // 获取触发器
    std::shared_ptr<SmartTrigger> getTrigger(const std::string& name);

    // 移除触发器
    void removeTrigger(const std::string& name);

    // 启动所有触发器
    void startAll();

    // 停止所有触发器
    void stopAll();

    // 获取所有触发器
    std::vector<std::shared_ptr<SmartTrigger>> getAllTriggers() const;

private:
    std::map<std::string, std::shared_ptr<SmartTrigger>> triggers_;
    mutable std::mutex mutex_;
};

} // namespace wingman
