#include "wingman/recorder.hpp"
#include "wingman/input.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <thread>

namespace wingman {

static MacroRecorder* g_instance = nullptr;

MacroRecorder::MacroRecorder()
    : m_recording(false), m_paused(false), m_startTime(0),
      m_mouseHook(nullptr), m_keyboardHook(nullptr) {
    g_instance = this;
}

MacroRecorder::~MacroRecorder() {
    stop();
}

void MacroRecorder::start() {
    if (m_recording) return;

    m_events.clear();
    m_recording = true;
    m_paused = false;
    m_startTime = GetTickCount();

    // 安装 Hook
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
}

void MacroRecorder::stop() {
    if (!m_recording) return;

    m_recording = false;

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
    m_events.clear();
}

bool MacroRecorder::saveToLua(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "-- Wingman Macro Recording Script\n";
    file << "-- Recorded " << m_events.size() << " events\n\n";

    file << "util.log(\"Starting macro playback...\")\n";
    file << "local startTime = util.getTime()\n\n";

    for (const auto& event : m_events) {
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
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"events\": [\n";

    for (size_t i = 0; i < m_events.size(); ++i) {
        const auto& event = m_events[i];
        file << "    {\n";
        file << "      \"type\": " << static_cast<int>(event.type) << ",\n";
        file << "      \"timestamp\": " << event.timestamp << ",\n";
        file << "      \"x\": " << event.x << ",\n";
        file << "      \"y\": " << event.y << ",\n";
        file << "      \"button\": " << event.button << ",\n";
        file << "      \"keyCode\": " << event.keyCode << ",\n";
        file << "      \"delay\": " << event.delay << "\n";
        file << "    }" << (i < m_events.size() - 1 ? "," : "") << "\n";
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
        // 使用更详细的异常处理
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

        m_events.clear();
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

            m_events.push_back(event);
        }

        return true;
    } catch (...) {
        return false;
    }
}

void MacroRecorder::playback(int speed, int repeat) const {
    if (m_events.empty()) return;

    for (int r = 0; r < repeat; ++r) {
        DWORD lastTimestamp = m_events[0].timestamp;

        for (const auto& event : m_events) {
            // 计算延迟
            DWORD delay = (event.timestamp - lastTimestamp) * 100 / speed;
            if (delay > 0) {
                Input::delay(delay);
            }

            switch (event.type) {
                case RecordedEventType::MouseMove:
                    Input::move(event.x, event.y);
                    break;

                case RecordedEventType::MouseClick:
                    Input::click(event.x, event.y, static_cast<MouseButton>(event.button));
                    break;

                case RecordedEventType::Scroll:
                    Input::scroll(event.x, event.y, event.delay);
                    break;

                case RecordedEventType::KeyDown:
                    Input::key(event.keyCode);
                    break;

                case RecordedEventType::Type:
                    Input::type(event.text, event.delay);
                    break;

                default:
                    break;
            }

            lastTimestamp = event.timestamp;
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
    // 合并连续的鼠标移动事件
    if (!m_events.empty() && event.type == RecordedEventType::MouseMove) {
        if (m_events.back().type == RecordedEventType::MouseMove) {
            m_events.back() = event;
            return;
        }
    }

    m_events.push_back(event);
}

} // namespace wingman
