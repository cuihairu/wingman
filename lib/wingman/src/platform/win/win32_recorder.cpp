#include "wingman/recorder.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <chrono>
#include <thread>

namespace wingman {

// Win32 低层钩子采集态：鼠标/键盘钩子句柄 + 独占装钩线程（低层钩子必须在
// 装钩子的线程上跑消息循环才会触发）
struct MacroRecorder::Impl {
    HHOOK mouseHook{nullptr};
    HHOOK keyboardHook{nullptr};
    std::thread hookThread;
    DWORD hookThreadId{0};

    void hookThreadMain(MacroRecorder& self);
    static LRESULT WINAPI mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT WINAPI keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
};

namespace {

MacroRecorder* g_instance = nullptr;

} // namespace

MacroRecorder::MacroRecorder()
    : m_impl(std::make_unique<Impl>()) {
    g_instance = this;
}

MacroRecorder::~MacroRecorder() {
    stop();
}

void MacroRecorder::start() {
    if (m_recording.load()) return;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_events.clear();
    }
    m_startTime = GetTickCount();
    m_recording.store(true);
    m_paused.store(false);

    // 低层钩子必须在装钩子的线程上跑消息循环才会触发回调。
    // 因此在独立线程里装钩子 + GetMessage 循环；stop 时 PostThreadMessage(WM_QUIT) 唤醒。
    m_impl->hookThread = std::thread([this]() { m_impl->hookThreadMain(*this); });

    emitMacroState("recording");
}

void MacroRecorder::stop() {
    if (!m_recording.load()) return;

    m_recording.store(false);

    // 唤醒 hook 线程的消息循环使其退出。
    if (m_impl->hookThreadId != 0) {
        PostThreadMessageA(m_impl->hookThreadId, WM_QUIT, 0, 0);
    }
    if (m_impl->hookThread.joinable()) {
        m_impl->hookThread.join();
    }
    m_impl->hookThreadId = 0;

    emitMacroState("stopped");
}

void MacroRecorder::Impl::hookThreadMain(MacroRecorder& self) {
    hookThreadId = GetCurrentThreadId();

    mouseHook = SetWindowsHookExA(
        WH_MOUSE_LL,
        mouseHookProc,
        GetModuleHandleA(nullptr),
        0
    );

    keyboardHook = SetWindowsHookExA(
        WH_KEYBOARD_LL,
        keyboardHookProc,
        GetModuleHandleA(nullptr),
        0
    );

    // 消息循环：低层钩子回调由系统通过消息派发到本线程。
    // WM_QUIT（来自 stop）会令 GetMessage 返回 false 从而退出循环。
    MSG msg;
    while (self.isRecording() && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (mouseHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
    }
    if (keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    }
}

LRESULT WINAPI MacroRecorder::Impl::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_instance && g_instance->isRecording() && !g_instance->isPaused()) {
        auto* hookStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        RecordedEvent event;
        event.timestamp = GetTickCount() - g_instance->getStartTime();
        event.x = hookStruct->pt.x;
        event.y = hookStruct->pt.y;

        switch (wParam) {
            case WM_MOUSEMOVE:
                event.type = RecordedEventType::MouseMove;
                break;

            case WM_LBUTTONDOWN:
                event.type = RecordedEventType::MouseDown;
                event.button = 0;
                break;

            case WM_LBUTTONUP:
                event.type = RecordedEventType::MouseUp;
                event.button = 0;
                break;

            case WM_RBUTTONDOWN:
                event.type = RecordedEventType::MouseDown;
                event.button = 2;
                break;

            case WM_RBUTTONUP:
                event.type = RecordedEventType::MouseUp;
                event.button = 2;
                break;

            case WM_MOUSEWHEEL:
                event.type = RecordedEventType::Scroll;
                event.delay = GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData) / WHEEL_DELTA;
                break;

            default:
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        g_instance->recordEvent(event);
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT WINAPI MacroRecorder::Impl::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_instance && g_instance->isRecording() && !g_instance->isPaused()) {
        auto* hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        RecordedEvent event;
        event.timestamp = GetTickCount() - g_instance->getStartTime();
        event.keyCode = hookStruct->vkCode;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            event.type = RecordedEventType::KeyDown;
            g_instance->recordEvent(event);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // namespace wingman

#endif // _WIN32
