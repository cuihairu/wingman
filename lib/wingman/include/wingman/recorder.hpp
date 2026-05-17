#pragma once

#include <string>
#include <vector>
#include <thread>
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wingman {

// 录制的事件类型
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
    uint64_t timestamp;    // 事件时间戳
    int x, y;              // 鼠标位置
    int button;            // 鼠标按钮
    int keyCode;           // 按键代码
    std::string text;      // 输入的文本
    int delay;             // 延迟时间
};

// 宏录制器
class MacroRecorder {
public:
    MacroRecorder();
    ~MacroRecorder();

    // 开始录制
    void start();

    // 停止录制
    void stop();

    // 暂停录制
    void pause();
    void resume();

    // 清空录制
    void clear();

    // 保存录制为 Lua 脚本
    bool saveToLua(const std::string& filepath) const;

    // 保存录制为 JSON
    bool saveToJSON(const std::string& filepath) const;

    // 加载录制
    bool loadFromJSON(const std::string& filepath);

    // 回放录制
    void playback(int speed = 100, int repeat = 1) const;

    // 获取录制状态
    bool isRecording() const { return m_recording; }
    bool isPaused() const { return m_paused; }
    size_t getEventCount() const { return m_events.size(); }

private:
    std::vector<RecordedEvent> m_events;
    bool m_recording;
    bool m_paused;
    uint64_t m_startTime;

#ifdef _WIN32
    HHOOK m_mouseHook;
    HHOOK m_keyboardHook;
#elif defined(__linux__)
    void* m_display;
    void* m_recordContext;
    std::thread m_processThread;
#elif defined(__APPLE__)
    void* m_eventTap;
    void* m_runLoopSource;
#endif

    // 获取实例
    static MacroRecorder* getInstance();

    // 记录事件
    void recordEvent(const RecordedEvent& event);

#ifdef _WIN32
    // Windows Hook 回调函数
    static LRESULT WINAPI mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
#endif

    // 获取开始时间（用于平台实现）
    uint64_t getStartTime() const { return m_startTime; }
};

} // namespace wingman
