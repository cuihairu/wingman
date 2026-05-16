#pragma once

#ifdef _WIN32
#include <string>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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
    DWORD timestamp;      // 事件时间戳
    int x, y;             // 鼠标位置
    int button;           // 鼠标按钮
    int keyCode;          // 按键代码
    std::string text;     // 输入的文本
    int delay;            // 延迟时间
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
    DWORD m_startTime;
    HHOOK m_mouseHook;
    HHOOK m_keyboardHook;

    // Hook 过程
    static LRESULT WINAPI mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    // 获取实例
    static MacroRecorder* getInstance();

    // 记录事件
    void recordEvent(const RecordedEvent& event);
};

} // namespace wingman

#endif // _WIN32
