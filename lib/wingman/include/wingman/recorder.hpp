#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>

#include "wingman/event.hpp"

namespace wingman {

// Recorded event type
enum class RecordedEventType {
    MouseMove,
    MouseClick,
    MouseDown,
    MouseUp,
    Scroll,
    KeyDown,
    KeyUp,
    Type,
    Delay,
};

struct RecordedEvent {
    RecordedEventType type;
    uint64_t timestamp;    // Event timestamp
    int x, y;              // Mouse position
    int button;            // Mouse button
    int keyCode;           // Key code
    std::string text;      // Input text
    int delay;             // Delay time
};

// RecordedEventType → 事件载荷字符串（macro.recorded 事件用）
inline const char* recordedEventTypeName(RecordedEventType type) {
    switch (type) {
        case RecordedEventType::MouseMove:  return "mouse_move";
        case RecordedEventType::MouseClick: return "mouse_click";
        case RecordedEventType::MouseDown:  return "mouse_down";
        case RecordedEventType::MouseUp:    return "mouse_up";
        case RecordedEventType::Scroll:     return "scroll";
        case RecordedEventType::KeyDown:    return "key_down";
        case RecordedEventType::KeyUp:      return "key_up";
        case RecordedEventType::Type:       return "type";
        case RecordedEventType::Delay:      return "delay";
    }
    return "unknown";
}

// 宏状态事件统一分发到 wingman.event（source "macro"）：三平台 recorder 实现
// 共用此内联助手，避免逐文件重复 emit 代码。state ∈ idle/recording/paused/
// playing/stopped。注意 emit 同步执行订阅者回调——状态点由脚本调用线程或其
// 内部工作线程触发，脚本侧非线程安全 callable 同受 systemwatch 同款约束。
inline void emitMacroState(const char* state) {
    EventHub::instance().emit("macro.state", {{"state", state}}, "macro");
}

// Macro recorder：事件存取/去重/导出/回放等平台无关逻辑统一在 src/recorder.cpp
// （三平台实现曾各自复制一份且已漂移：win32/cocoa 锁内快照、x11 无锁直读）。
// 平台采集（Win 低层钩子线程 / Linux XRecord / macOS 事件 tap）与平台态收在
// Impl，由 src/platform/{win,linux,mac}/*_recorder.cpp 给出完整定义，头文件
// 零平台宏（平台边界守卫 allowlist 末项）。
class MacroRecorder {
public:
    MacroRecorder();
    ~MacroRecorder();

    MacroRecorder(const MacroRecorder&) = delete;
    MacroRecorder& operator=(const MacroRecorder&) = delete;

    // Start/Stop recording（平台采集：装/卸钩子、事件 tap、XRecord context）
    void start();

    // Stop recording
    void stop();

    // Pause recording
    void pause();
    void resume();

    // Clear recording
    void clear();

    // Save recording as Lua script
    bool saveToLua(const std::string& filepath) const;

    // Save recording as JSON
    bool saveToJSON(const std::string& filepath) const;

    // Load recording
    bool loadFromJSON(const std::string& filepath);

    // Playback recording
    void playback(int speed = 100, int repeat = 1) const;

    // Get recording state
    bool isRecording() const { return m_recording.load(std::memory_order_relaxed); }
    bool isPaused() const { return m_paused.load(std::memory_order_relaxed); }
    size_t getEventCount() const;

    // Platform callbacks use these accessors from free functions.
    void recordEvent(const RecordedEvent& event);
    uint64_t getStartTime() const { return m_startTime; }

private:
    // 拷贝一份事件快照（带锁），供 const 方法在锁外做 I/O / 回放
    std::vector<RecordedEvent> getEventsSnapshot() const;

    std::vector<RecordedEvent> m_events;
    std::atomic<bool> m_recording{false};
    std::atomic<bool> m_paused{false};
    uint64_t m_startTime{0};
    // 保护 m_events（hook 线程写，其它线程读/写）。
    mutable std::mutex m_eventMutex;

    // 平台态与平台采集逻辑（钩子句柄/线程、XRecord context、事件 tap 等）。
    // 完整类型只在平台实现文件可见，构造/析构亦随平台文件（unique_ptr 析构
    // 需完整类型）；事件回调经公有 API（isRecording/recordEvent 等）回访，
    // Impl 不需要友元。
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace wingman
