#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

#include <lua.hpp>
#include <spdlog/logger.h>

#include "wingman/screen.hpp"  // For Rect type

namespace wingman {

#ifdef _WIN32

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
struct BasicTriggerCondition {
    TriggerType type;
    std::string value;      // 具体值（如颜色、窗口标题等）
    Rect region;           // 搜索区域
    int tolerance;         // 容差
    int interval;          // 检查间隔 (ms)
    bool enabled;
};

// 触发器动作
enum class BasicTriggerAction {
    RunScript,       // 运行脚本
    Click,           // 点击
    KeyPress,        // 按键
    Type,            // 输入
    StopScript,      // 停止脚本
    PauseScript,     // 暂停脚本
    ShowMessage,     // 显示消息
    PlayAudio,       // 播放声音
    Log,             // 记录日志
};

struct TriggerActionData {
    BasicTriggerAction type;
    std::string value;
    int x, y;           // 坐标
    int delay;          // 延迟
};

// 触发器配置
struct TriggerConfig {
    std::string name;
    BasicTriggerCondition condition;
    std::vector<TriggerActionData> actions;
    bool oneShot;        // 只触发一次
    int cooldown;        // 冷却时间 (ms)
    bool enabled;
};

// 触发器实例（运行时数据）
struct TriggerInstance {
    size_t id;
    TriggerConfig config;
    DWORD startTime;       // 触发器启动时间
    DWORD lastTriggerTime;
    bool triggered;
};

// 触发器管理器
class TriggerManager {
public:
    TriggerManager();
    explicit TriggerManager(std::shared_ptr<spdlog::logger> logger);
    ~TriggerManager();

    // 添加触发器
    size_t add(const TriggerConfig& config);

    // 更新触发器配置
    bool update(size_t id, const TriggerConfig& config);

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

    // 获取所有触发器配置
    std::vector<TriggerConfig> getAllTriggerConfigs() const;

    // 获取所有触发器实例（含状态）
    std::vector<TriggerInstance> getAllTriggerInstances() const;

    // 根据ID获取配置
    std::optional<TriggerConfig> getTriggerConfig(size_t id) const;

    // 检查触发器是否存在
    bool hasTrigger(size_t id) const;

    // 获取触发器数量
    size_t getTriggerCount() const;

    // 设置 Lua 状态（用于执行 Lua 脚本动作）
    void setLuaState(lua_State* L);

    // 设置日志器
    void setLogger(std::shared_ptr<spdlog::logger> logger);

private:
    // 内部辅助方法
    void Lock() const { EnterCriticalSection(&m_cs); }
    void Unlock() const { LeaveCriticalSection(&m_cs); }

    std::vector<TriggerInstance> m_triggers;
    bool m_running;
    HANDLE m_thread;
    mutable CRITICAL_SECTION m_cs;
    lua_State* m_luaState;
    std::shared_ptr<spdlog::logger> m_logger;

    // 触发器检查线程
    static DWORD WINAPI checkThread(LPVOID param);

    // 检查单个触发器
    bool checkTrigger(TriggerInstance& trigger);

    // 执行动作
    void executeActions(const std::vector<TriggerActionData>& actions);
};

#endif // _WIN32

} // namespace wingman
