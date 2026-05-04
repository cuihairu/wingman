#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <windows.h>

namespace wingman {

// 触发器类型
enum class TriggerType {
    ColorFound,      // 颜色出现
    ColorLost,       // 颜色消失
    ImageFound,      // 图像出现
    ImageLost,       // 图像消失
    WindowOpened,    // 窗口打开
    WindowClosed,    // 窗口关闭
    ProcessStarted,  // 进程启动
    ProcessStopped,  // 进程停止
    TimeElapsed,     // 时间经过
    HotkeyPressed,   // 热键按下
    PixelChanged,    // 像素变化
};

// 触发器条件
struct TriggerCondition {
    TriggerType type;
    std::string value;      // 具体值（如颜色、窗口标题等）
    Rect region;           // 搜索区域
    int tolerance;         // 容差
    int interval;          // 检查间隔 (ms)
    bool enabled;
};

// 触发器动作
enum class TriggerAction {
    RunScript,       // 运行脚本
    Click,           // 点击
    KeyPress,        // 按键
    Type,            // 输入
    StopScript,      // 停止脚本
    PauseScript,     // 暂停脚本
    ShowMessage,     // 显示消息
    PlaySound,       // 播放声音
    Log,             // 记录日志
};

struct TriggerActionData {
    TriggerAction type;
    std::string value;
    int x, y;           // 坐标
    int delay;          // 延迟
};

// 触发器配置
struct TriggerConfig {
    std::string name;
    TriggerCondition condition;
    std::vector<TriggerActionData> actions;
    bool oneShot;        // 只触发一次
    int cooldown;        // 冷却时间 (ms)
    bool enabled;
};

// 触发器管理器
class TriggerManager {
public:
    TriggerManager();
    ~TriggerManager();

    // 添加触发器
    size_t add(const TriggerConfig& config);

    // 移除触发器
    void remove(size_t id);

    // 启动/停止触发器
    void enable(size_t id);
    void disable(size_t id);

    // 启动/停止所有触发器
    void start();
    void stop();

    // 获取触发器状态
    bool isRunning(size_t id) const;

private:
    struct TriggerInstance {
        size_t id;
        TriggerConfig config;
        DWORD lastTriggerTime;
        bool triggered;
    };

    std::vector<TriggerInstance> m_triggers;
    bool m_running;
    HANDLE m_thread;
    CRITICAL_SECTION m_cs;

    // 触发器检查线程
    static DWORD WINAPI checkThread(LPVOID param);

    // 检查单个触发器
    bool checkTrigger(TriggerInstance& trigger);

    // 执行动作
    void executeActions(const std::vector<TriggerActionData>& actions);
};

} // namespace wingman
