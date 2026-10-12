#pragma once

// Android 端触发器引擎（A2 扩展，docs/android-agent-design.md；development-todo
// 「触发器系统端侧跑通（定时/像素触发）」）。对齐桌面 SmartTriggerManager 的
// 词汇与语义——条件全满足才执行动作、checkInterval 轮询、maxTriggers 自停、
// 条件/动作均为声明式数据（不持有 Lua 状态）——但按 Android 原生形态实现：
//
//  - 单监视线程遍历全部已启动触发器（设备友好，避免每触发器一线程）；
//  - 每轮每触发器最多取一帧（bitmap-first，与 A2 找色 API 同口径），条件
//    判定走 ImageAnalyzer（color_*）与像素直读（pixel_changed），timer 条件
//    走 steady 时钟边沿（上次满足后满 timerMs 才再满足）；
//  - 动作直接走 AndroidHostBridge（tap/long_press/swipe/stop），桥为 null
//    或取帧失败时该轮按未满足处理（降级不崩，与 A2 API 同口径）；
//  - 进程级单例：脚本退出后触发器继续在服务进程内监视（与桌面
//    SmartTriggerManager::instance() 单例语义一致）。桥生命周期归宿主，
//    宿主拆桥前必须 stopAll（见 development-todo 真机验证 ⚠️）。

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "platform/android/android_host_bridge.hpp"
#include "wingman/vision/image_analyzer.hpp"

namespace wingman::android {

enum class TriggerConditionType {
    ColorFound,     // 区域内找到颜色
    ColorNotFound,  // 区域内颜色消失
    PixelChanged,   // 采样点颜色相对上一拍变化超过容差
    Timer,          // 距上次满足满 timerMs（边沿触发，一次满足只计一次）
};

enum class TriggerActionType {
    Tap,        // 轻点 (x, y)
    LongPress,  // 长按 (x, y, durationMs)
    Swipe,      // 滑动 (x1, y1) -> (x2, y2, durationMs)
    Stop,       // 停止本触发器（自停）
};

struct TriggerCondition {
    TriggerConditionType type = TriggerConditionType::ColorFound;
    Color color;                 // color_found / color_not_found / pixel_changed
    int tolerance = 10;          // 颜色容差（0-255）
    Rect region;                 // color_* 搜索区域（空 = 全屏）
    int x = 0;                   // pixel_changed 采样点
    int y = 0;
    int timerMs = 0;             // timer 间隔

    // 运行期状态（监视线程独占写，检查时在锁内拷出）
    bool hasPrevColor = false;
    Color prevColor;
    int64_t lastMetMs = 0;       // steady ms；0 = 从未满足
};

struct TriggerAction {
    TriggerActionType type = TriggerActionType::Tap;
    int x = 0;
    int y = 0;
    int x2 = 0;
    int y2 = 0;
    int durationMs = 0;          // long_press / swipe 时长
};

struct TriggerDef {
    std::string name;
    int checkIntervalMs = 500;   // 轮询间隔（<50 收敛到 50，防忙等）
    int maxTriggers = 0;         // 0 = 不限；达到后自停（running=false）
    std::vector<TriggerCondition> conditions;
    std::vector<TriggerAction> actions;
};

class AndroidTriggerManager {
public:
    static AndroidTriggerManager& instance();
    ~AndroidTriggerManager();

    AndroidTriggerManager(const AndroidTriggerManager&) = delete;
    AndroidTriggerManager& operator=(const AndroidTriggerManager&) = delete;

    // 宿主桥（监视线程与动作执行共用；null = 只登记不触发，降级语义）
    void setBridge(platform::android::AndroidHostBridge* bridge);

    // 登记触发器；同名已存在返回 false（先 remove 再 add）
    bool add(const TriggerDef& def);
    bool remove(const std::string& name);
    // start 对已运行触发器幂等返回 true；未知名返回 false
    bool start(const std::string& name);
    bool stop(const std::string& name);
    // 返回实际完成状态切换的触发器数
    int startAll();
    int stopAll();

    bool isRunning(const std::string& name) const;
    // 触发次数；未知名返回 -1
    int getTriggerCount(const std::string& name) const;
    // 登记中的触发器总数
    int count() const;

private:
    AndroidTriggerManager();

    // atomic 成员使隐式拷贝删除；显式按值拷贝（与桌面 ScriptInfo 同款处理，
    // 保住 map 值语义与监视线程快照拷贝）
    struct Entry {
        TriggerDef def;
        std::atomic<bool> running{false};
        std::atomic<int> fireCount{0};
        // start 时刻（timer 条件的首个计时基准；steady ms）
        int64_t startedAtMs = 0;
        // 上次检查时刻（checkIntervalMs 轮询门；0 = 尚未检查过）
        int64_t lastCheckedMs = 0;

        Entry() = default;
        Entry(const Entry& other)
            : def(other.def),
              running(other.running.load(std::memory_order_acquire)),
              fireCount(other.fireCount.load(std::memory_order_acquire)),
              startedAtMs(other.startedAtMs),
              lastCheckedMs(other.lastCheckedMs) {}
    };

    void ensureWatchThread();
    void watchLoop();
    // 在锁内拷出待检查条目（含运行期条件状态），锁外判定与执行
    std::vector<Entry> snapshotActive() const;
    void commitState(const std::string& name, const Entry& evaluated);

    static int64_t nowMs();

    mutable std::mutex mutex_;
    std::map<std::string, Entry> triggers_;
    platform::android::AndroidHostBridge* bridge_ = nullptr;

    std::atomic<bool> watchStop_{false};
    std::thread watchThread_;
    std::condition_variable wake_;
    std::mutex wakeMutex_;
};

} // namespace wingman::android
