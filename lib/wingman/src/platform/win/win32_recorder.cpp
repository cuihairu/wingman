#include "wingman/recorder.hpp"

#ifdef _WIN32

#include "wingman/platform/input_factory.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <chrono>
#include <memory>
#include <thread>

namespace wingman {

namespace {

platform::IInput& getInput() {
    static std::shared_ptr<platform::IInput> input = platform::defaultSharedInput();
    return *input;
}

void sleepMs(unsigned long milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

platform::MouseButton toPlatformMouseButton(int button) {
    return static_cast<platform::MouseButton>(button);
}

} // namespace

static MacroRecorder* g_instance = nullptr;

MacroRecorder::MacroRecorder()
    : m_recording(false), m_paused(false), m_startTime(0),
      m_mouseHook(nullptr), m_keyboardHook(nullptr), m_hookThreadId(0) {
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
    m_hookThread = std::thread([this]() { hookThreadMain(); });
}

void MacroRecorder::stop() {
    if (!m_recording.load()) return;

    m_recording.store(false);

    // 唤醒 hook 线程的消息循环使其退出。
    if (m_hookThreadId != 0) {
        PostThreadMessageA(m_hookThreadId, WM_QUIT, 0, 0);
    }
    if (m_hookThread.joinable()) {
        m_hookThread.join();
    }
    m_hookThreadId = 0;
}

void MacroRecorder::hookThreadMain() {
    m_hookThreadId = GetCurrentThreadId();

    m_mouseHook = SetWindowsHookExA(
        WH_MOUSE_LL,
        mouseHookProc,
        GetModuleHandleA(nullptr),
        0
    );

    m_keyboardHook = SetWindowsHookExA(
        WH_KEYBOARD_LL,
        keyboardHookProc,
        GetModuleHandleA(nullptr),
        0
    );

    // 消息循环：低层钩子回调由系统通过消息派发到本线程。
    // WM_QUIT（来自 stop）会令 GetMessage 返回 false 从而退出循环。
    MSG msg;
    while (m_recording.load() && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
}

void MacroRecorder::pause() {
    m_paused = true;
}

void MacroRecorder::resume() {
    m_paused = false;
}

void MacroRecorder::clear() {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.clear();
}

size_t MacroRecorder::getEventCount() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    return m_events.size();
}

bool MacroRecorder::saveToLua(const std::string& filepath) const {
    std::vector<RecordedEvent> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        snapshot = m_events;
    }

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "-- Wingman Macro Recording Script\n";
    file << "-- Recorded " << snapshot.size() << " events\n\n";

    file << "util.log(\"Starting macro playback...\")\n";
    file << "local startTime = util.getTime()\n\n";

    for (const auto& event : snapshot) {
        switch (event.type) {
            case RecordedEventType::MouseMove:
                file << "input.move(" << event.x << ", " << event.y << ")\n";
                break;

            case RecordedEventType::MouseClick:
                file << "input.click(" << event.x << ", " << event.y << ", " << event.button << ")\n";
                break;

            case RecordedEventType::Scroll:
                file << "input.scroll(" << event.x << ", " << event.y << ", " << event.delay << ")\n";
                break;

            case RecordedEventType::KeyDown:
                file << "input.key(" << event.keyCode << ")\n";
                break;

            case RecordedEventType::Type:
                file << "input.type(\"" << event.text << "\", " << event.delay << ")\n";
                break;

            case RecordedEventType::Delay:
                file << "util.sleep(" << event.delay << ")\n";
                break;

            default:
                break;
        }
    }

    file << "\nutil.log(\"Macro playback completed!\")\n";

    return true;
}

bool MacroRecorder::saveToJSON(const std::string& filepath) const {
    std::vector<RecordedEvent> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        snapshot = m_events;
    }

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"events\": [\n";

    for (size_t i = 0; i < snapshot.size(); ++i) {
        const auto& event = snapshot[i];
        file << "    {\n";
        file << "      \"type\": " << static_cast<int>(event.type) << ",\n";
        file << "      \"timestamp\": " << event.timestamp << ",\n";
        file << "      \"x\": " << event.x << ",\n";
        file << "      \"y\": " << event.y << ",\n";
        file << "      \"button\": " << event.button << ",\n";
        file << "      \"keyCode\": " << event.keyCode << ",\n";
        file << "      \"delay\": " << event.delay << "\n";
        file << "    }" << (i < snapshot.size() - 1 ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    return true;
}

bool MacroRecorder::loadFromJSON(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        try {
            file >> j;
        } catch (const nlohmann::json::parse_error&) {
            return false;
        } catch (const nlohmann::json::type_error&) {
            return false;
        } catch (...) {
            return false;
        }

        if (!j.contains("events") || !j["events"].is_array()) {
            return false;
        }

        std::vector<RecordedEvent> loaded;
        for (const auto& eventJson : j["events"]) {
            RecordedEvent event;
            event.type = static_cast<RecordedEventType>(eventJson.value("type", 0));
            event.timestamp = eventJson.value("timestamp", 0);
            event.x = eventJson.value("x", 0);
            event.y = eventJson.value("y", 0);
            event.button = eventJson.value("button", 0);
            event.keyCode = eventJson.value("keyCode", 0);
            event.delay = eventJson.value("delay", 0);
            event.text = eventJson.value("text", "");

            loaded.push_back(event);
        }

        {
            std::lock_guard<std::mutex> lock(m_eventMutex);
            m_events = std::move(loaded);
        }

        return true;
    } catch (...) {
        return false;
    }
}

void MacroRecorder::playback(int speed, int repeat) const {
    std::vector<RecordedEvent> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        snapshot = m_events;
    }
    if (snapshot.empty()) return;

    for (int r = 0; r < repeat; ++r) {
        DWORD lastTimestamp = static_cast<DWORD>(snapshot[0].timestamp);

        for (const auto& event : snapshot) {
            DWORD delay = static_cast<DWORD>((event.timestamp - lastTimestamp) * 100 / speed);
            if (delay > 0) {
                sleepMs(delay);
            }

            switch (event.type) {
                case RecordedEventType::MouseMove:
                    getInput().mouseMove(event.x, event.y);
                    break;

                case RecordedEventType::MouseClick:
                    getInput().mouseMove(event.x, event.y);
                    getInput().mouseClick(toPlatformMouseButton(event.button));
                    break;

                case RecordedEventType::Scroll:
                    getInput().mouseMove(event.x, event.y);
                    getInput().mouseWheel(event.delay);
                    break;

                case RecordedEventType::KeyDown:
                    getInput().keyPress(static_cast<platform::KeyCode>(event.keyCode));
                    break;

                case RecordedEventType::Type:
                    getInput().textInput(event.text);
                    if (event.delay > 0) {
                        sleepMs(static_cast<unsigned long>(event.delay));
                    }
                    break;

                default:
                    break;
            }

            lastTimestamp = static_cast<DWORD>(event.timestamp);
        }
    }
}

LRESULT WINAPI MacroRecorder::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_instance && g_instance->isRecording() && !g_instance->isPaused()) {
        auto* hookStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        RecordedEvent event;
        event.timestamp = GetTickCount() - g_instance->m_startTime;
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

LRESULT WINAPI MacroRecorder::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_instance && g_instance->isRecording() && !g_instance->isPaused()) {
        auto* hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        RecordedEvent event;
        event.timestamp = GetTickCount() - g_instance->m_startTime;
        event.keyCode = hookStruct->vkCode;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            event.type = RecordedEventType::KeyDown;
            g_instance->recordEvent(event);
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

MacroRecorder* MacroRecorder::getInstance() {
    return g_instance;
}

void MacroRecorder::recordEvent(const RecordedEvent& event) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    if (!m_events.empty() && event.type == RecordedEventType::MouseMove) {
        if (m_events.back().type == RecordedEventType::MouseMove) {
            m_events.back() = event;
            return;
        }
    }

    m_events.push_back(event);
}

} // namespace wingman

#endif // _WIN32
