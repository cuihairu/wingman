#pragma once

#include <string>
#include <vector>
#include <thread>
#include <cstdint>
#include <atomic>
#include <mutex>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
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
    void* m_display;
    void* m_recordContext;
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
