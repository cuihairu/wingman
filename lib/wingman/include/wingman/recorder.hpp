#pragma once

#include <string>
#include <vector>
#include <thread>
#include <cstdint>
#include <atomic>
#include <mutex>

#include "wingman/event.hpp"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef __linux__
struct _XDisplay;
#endif

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


// Macro recorder
class MacroRecorder {
public:
    MacroRecorder();
    ~MacroRecorder();

    // Start recording
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
    std::atomic<bool> m_recording;
    std::atomic<bool> m_paused;
    uint64_t m_startTime;
    // 保护 m_events（hook 线程写，其它线程读/写）。
    mutable std::mutex m_eventMutex;

#ifdef _WIN32
    HHOOK m_mouseHook;
    HHOOK m_keyboardHook;
    // 低层钩子必须在装钩子的线程上跑消息循环才会触发；录制期间独占此线程。
    std::thread m_hookThread;
    DWORD m_hookThreadId{0};
    void hookThreadMain();
#elif defined(__linux__)
    _XDisplay* m_display;
    unsigned long m_recordContext;
    std::thread m_processThread;
#elif defined(__APPLE__)
    CFMachPortRef m_eventTap;
    CFRunLoopSourceRef m_runLoopSource;
#endif

    // Get instance
    static MacroRecorder* getInstance();

#ifdef _WIN32
    // Windows Hook callback function
    static LRESULT WINAPI mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif
};

} // namespace wingman
