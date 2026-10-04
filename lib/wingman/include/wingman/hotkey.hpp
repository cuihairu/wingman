#pragma once

#include "wingman/platform/platform_types.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace wingman {
namespace platform {
class IInput;
}

// 组合键描述：主键 + Ctrl/Shift/Alt 修饰位。
struct HotkeyCombo {
    platform::KeyCode key = platform::KeyCode::Space;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

// 解析 "Ctrl+Shift+A" 风格组合键文本（大小写不敏感，修饰键顺序不限；
// 主键支持 A-Z/0-9/F1-F12/命名键如 Space、Enter、Esc、方向键）。
// 非法文本（无主键、重复修饰、未知键名、Win/Meta 修饰）返回 false。
bool parseHotkeyCombo(const std::string& text, HotkeyCombo& out);

// 全局热键监听（轮询式 v1）：后台线程按固定间隔读全局键盘状态，主键与
// 修饰键同时按下时按上升沿（rising edge）触发一次回调。
// 经 IInput 抽象跨平台（Windows GetAsyncKeyState / Linux XQueryKeymap 为
// 全局键态；macOS CGEventSourceKeyState 受辅助功能权限限制，真实行为待
// 真机验证）。与 TriggerType::HotkeyPressed 的前台轮询触发器互补：后者
// 挂在 TriggerManager 轮询里，本模块独立线程、支持回调注册。
// 生命周期：首个注册自动启动轮询；注销清空后线程自查退出（下次注册
// 自动重启）；stop()/析构显式停止并 join。回调在锁外触发、可能晚于
// 注销一拍（触发前按 ID 复核，已注销不触发）。
// v1 已知限制：按键在两次轮询间按下又弹起（< 轮询间隔）可能漏检。
class HotkeyManager {
public:
    // 进程级默认实例（脚本 hotkey 模块与 GUI 桥用；函数局部静态规避
    // 静态初始化顺序问题）。测试请自行构造并注入 MockInput。
    static HotkeyManager& defaultInstance();

    // input 为空时用 platform::defaultSharedInput()（测试注入 MockInput）
    explicit HotkeyManager(std::shared_ptr<platform::IInput> input = nullptr);
    ~HotkeyManager();

    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    // 注册组合键回调，返回注册 ID（0 = 失败：组合非法或回调为空）。
    uint64_t registerHotkey(const HotkeyCombo& combo, std::function<void()> callback);
    uint64_t registerHotkey(const std::string& comboText, std::function<void()> callback);

    // 注销；ID 不存在返回 false。
    bool unregister(uint64_t id);

    size_t count() const;

    // 手动控制轮询（脚本模块用注册/注销自动启停即可）
    void start(uint32_t pollIntervalMs = 30);
    void stop();
    bool isRunning() const;

private:
    struct Entry {
        HotkeyCombo combo;
        std::function<void()> callback;
        bool wasPressed = false;
    };

    void ensureRunning();
    void workerLoop();
    void pollOnce();

    std::shared_ptr<platform::IInput> input_;
    mutable std::mutex mutex_;
    std::unordered_map<uint64_t, Entry> entries_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> intervalMs_{30};
    uint64_t nextId_ = 1;
};

} // namespace wingman
