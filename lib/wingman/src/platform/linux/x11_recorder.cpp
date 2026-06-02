#include "wingman/recorder.hpp"

#if defined(__linux__) && !defined(__APPLE__)

#include "wingman/platform/input_factory.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <thread>
#include <chrono>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

// X11 Record Extension
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/record.h>

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
static Display* g_display = nullptr;
static XRecordContext g_recordContext = nullptr;

static unsigned long getTickCount() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<unsigned long>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// X11 Record callback
static void eventCallback(XPointer priv, XRecordInterceptData* data) {
    if (!g_instance || !g_instance->isRecording() || g_instance->isPaused()) {
        XRecordFreeData(data);
        return;
    }

    if (data->category != XRecordFromServer) {
        XRecordFreeData(data);
        return;
    }

    RecordedEvent event;
    event.timestamp = getTickCount() - g_instance->getStartTime();

    xEvent* xev = reinterpret_cast<xEvent*>(data->data);
    int type = xev->u.u.type & 0x7f;

    switch (type) {
        case KeyPress: {
            event.type = RecordedEventType::KeyDown;
            event.keyCode = xev->u.u.detail;
            g_instance->recordEvent(event);
            break;
        }

        case ButtonPress: {
            event.type = RecordedEventType::MouseDown;
            event.button = xev->u.u.detail - 1;
            g_instance->recordEvent(event);

            // Also record as click event
            RecordedEvent clickEvent = event;
            clickEvent.type = RecordedEventType::MouseClick;
            g_instance->recordEvent(clickEvent);
            break;
        }

        case ButtonRelease: {
            event.type = RecordedEventType::MouseUp;
            event.button = xev->u.u.detail - 1;
            g_instance->recordEvent(event);
            break;
        }

        case MotionNotify: {
            event.type = RecordedEventType::MouseMove;
            event.x = xev->u.keyButtonPointer.rootX;
            event.y = xev->u.keyButtonPointer.rootY;
            g_instance->recordEvent(event);
            break;
        }

        default:
            break;
    }

    XRecordFreeData(data);
}

MacroRecorder::MacroRecorder()
    : m_recording(false), m_paused(false), m_startTime(0),
      m_display(nullptr), m_recordContext(nullptr) {
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
    m_startTime = getTickCount();

    // Open two Display connections
    Display* controlDisplay = XOpenDisplay(nullptr);
    Display* dataDisplay = XOpenDisplay(nullptr);

    if (!controlDisplay || !dataDisplay) {
        m_recording = false;
        if (controlDisplay) XCloseDisplay(controlDisplay);
        if (dataDisplay) XCloseDisplay(dataDisplay);
        return;
    }

    // Check Record Extension
    int major, minor;
    if (!XRecordQueryVersion(controlDisplay, &major, &minor)) {
        m_recording = false;
        XCloseDisplay(controlDisplay);
        XCloseDisplay(dataDisplay);
        return;
    }

    // Create record range
    XRecordClientSpec clients = XRecordAllClients;

    XRecordRange* range = XRecordAllocRange();
    if (!range) {
        m_recording = false;
        XCloseDisplay(controlDisplay);
        XCloseDisplay(dataDisplay);
        return;
    }

    range->device_events.first = KeyPress;
    range->device_events.last = MotionNotify;

    m_recordContext = XRecordCreateContext(controlDisplay, 0, &clients, 1, &range, 1);
    XFree(range);

    if (!m_recordContext) {
        m_recording = false;
        XCloseDisplay(controlDisplay);
        XCloseDisplay(dataDisplay);
        return;
    }

    if (!XRecordEnableContextAsync(dataDisplay, m_recordContext, eventCallback, nullptr)) {
        XRecordFreeContext(controlDisplay, m_recordContext);
        m_recording = false;
        XCloseDisplay(controlDisplay);
        XCloseDisplay(dataDisplay);
        return;
    }

    m_display = controlDisplay;
    g_display = dataDisplay;

    // Start processing thread
    m_processThread = std::thread([this]() {
        while (m_recording) {
            if (g_display) {
                XRecordProcessReplies(g_display);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

void MacroRecorder::stop() {
    if (!m_recording) return;

    m_recording = false;

    if (m_processThread.joinable()) {
        m_processThread.join();
    }

    if (m_recordContext && m_display) {
        Display* dataDisplay = g_display;
        XRecordDisableContext(m_display, m_recordContext);
        XRecordFreeContext(m_display, m_recordContext);
        XCloseDisplay(m_display);
        XCloseDisplay(dataDisplay);
        m_recordContext = nullptr;
        m_display = nullptr;
        g_display = nullptr;
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
        unsigned long lastTimestamp = m_events[0].timestamp;

        for (const auto& event : m_events) {
            unsigned long delay = (event.timestamp - lastTimestamp) * 100 / speed;
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

            lastTimestamp = event.timestamp;
        }
    }
}

MacroRecorder* MacroRecorder::getInstance() {
    return g_instance;
}

void MacroRecorder::recordEvent(const RecordedEvent& event) {
    if (!m_events.empty() && event.type == RecordedEventType::MouseMove) {
        if (m_events.back().type == RecordedEventType::MouseMove) {
            m_events.back() = event;
            return;
        }
    }

    m_events.push_back(event);
}

unsigned long MacroRecorder::getStartTime() const {
    return m_startTime;
}

} // namespace wingman

#endif // __linux__ && !__APPLE__
