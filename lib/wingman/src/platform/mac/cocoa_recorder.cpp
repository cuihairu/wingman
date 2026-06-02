#include "wingman/recorder.hpp"

#ifdef __APPLE__

#include "wingman/platform/input_factory.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <thread>
#include <chrono>
#include <memory>
#include <sys/time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>

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
static CFMachPortRef g_eventTap = nullptr;
static CFRunLoopSourceRef g_runLoopSource = nullptr;
static std::thread g_runLoopThread;
static bool g_runLoopRunning = false;

static unsigned long getTickCount() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<unsigned long>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// CGEventTap callback
static CGEventRef eventTapCallback(CGEventTapProxy proxy, CGEventType type,
                                    CGEventRef event, void* refcon) {
    if (!g_instance || !g_instance->isRecording() || g_instance->isPaused()) {
        return event;
    }

    RecordedEvent recordEvent;
    recordEvent.timestamp = getTickCount() - g_instance->getStartTime();

    CGPoint location = CGEventGetLocation(event);
    recordEvent.x = static_cast<int>(location.x);
    recordEvent.y = static_cast<int>(location.y);

    switch (type) {
        case kCGEventMouseMoved: {
            recordEvent.type = RecordedEventType::MouseMove;
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged: {
            recordEvent.type = RecordedEventType::MouseMove;
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventLeftMouseDown: {
            recordEvent.type = RecordedEventType::MouseDown;
            recordEvent.button = 0;
            g_instance->recordEvent(recordEvent);

            RecordedEvent clickEvent = recordEvent;
            clickEvent.type = RecordedEventType::MouseClick;
            g_instance->recordEvent(clickEvent);
            break;
        }

        case kCGEventLeftMouseUp: {
            recordEvent.type = RecordedEventType::MouseUp;
            recordEvent.button = 0;
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventRightMouseDown: {
            recordEvent.type = RecordedEventType::MouseDown;
            recordEvent.button = 2;
            g_instance->recordEvent(recordEvent);

            RecordedEvent clickEvent = recordEvent;
            clickEvent.type = RecordedEventType::MouseClick;
            g_instance->recordEvent(clickEvent);
            break;
        }

        case kCGEventRightMouseUp: {
            recordEvent.type = RecordedEventType::MouseUp;
            recordEvent.button = 2;
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventOtherMouseDown: {
            recordEvent.type = RecordedEventType::MouseDown;
            recordEvent.button = 1;
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventOtherMouseUp: {
            recordEvent.type = RecordedEventType::MouseUp;
            recordEvent.button = 1;
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventScrollWheel: {
            recordEvent.type = RecordedEventType::Scroll;
            int64_t delta = static_cast<int64_t>(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1));
            recordEvent.delay = static_cast<int>(delta);
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventKeyDown: {
            recordEvent.type = RecordedEventType::KeyDown;
            CGKeyCode keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            recordEvent.keyCode = static_cast<int>(keyCode);
            g_instance->recordEvent(recordEvent);
            break;
        }

        case kCGEventKeyUp: {
            recordEvent.type = RecordedEventType::KeyUp;
            CGKeyCode keyCode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            recordEvent.keyCode = static_cast<int>(keyCode);
            g_instance->recordEvent(recordEvent);
            break;
        }

        default:
            break;
    }

    return event;
}

// RunLoop thread function
static void runLoopThreadFunc() {
    CFRunLoopRef runLoop = CFRunLoopGetCurrent();
    g_runLoopRunning = true;

    while (g_runLoopRunning) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    }

    g_runLoopRunning = false;
}

MacroRecorder::MacroRecorder()
    : m_recording(false), m_paused(false), m_startTime(0),
      m_eventTap(nullptr) {
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

    // Create Event Tap
    CGEventMask eventMask = CGEventMaskBit(kCGEventMouseMoved) |
                           CGEventMaskBit(kCGEventLeftMouseDragged) |
                           CGEventMaskBit(kCGEventRightMouseDragged) |
                           CGEventMaskBit(kCGEventLeftMouseDown) |
                           CGEventMaskBit(kCGEventLeftMouseUp) |
                           CGEventMaskBit(kCGEventRightMouseDown) |
                           CGEventMaskBit(kCGEventRightMouseUp) |
                           CGEventMaskBit(kCGEventOtherMouseDown) |
                           CGEventMaskBit(kCGEventOtherMouseUp) |
                           CGEventMaskBit(kCGEventScrollWheel) |
                           CGEventMaskBit(kCGEventKeyDown) |
                           CGEventMaskBit(kCGEventKeyUp);

    m_eventTap = CGEventTapCreate(kCGSessionEventTap,
                                   kCGHeadInsertEventTap,
                                   kCGEventTapOptionDefault,
                                   eventMask,
                                   eventTapCallback,
                                   nullptr);

    if (!m_eventTap) {
        m_recording = false;
        return;
    }

    g_eventTap = m_eventTap;
    m_runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_eventTap, 0);

    if (!m_runLoopSource) {
        CFRelease(m_eventTap);
        m_eventTap = nullptr;
        g_eventTap = nullptr;
        m_recording = false;
        return;
    }

    // Start RunLoop thread
    g_runLoopRunning = false;
    g_runLoopThread = std::thread(runLoopThreadFunc);

    // Wait for RunLoop to start
    while (!g_runLoopRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), m_runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(m_eventTap, true);
}

void MacroRecorder::stop() {
    if (!m_recording) return;

    m_recording = false;

    if (m_eventTap) {
        CGEventTapEnable(m_eventTap, false);
    }

    if (m_runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), m_runLoopSource, kCFRunLoopCommonModes);
        CFRelease(m_runLoopSource);
        m_runLoopSource = nullptr;
    }

    g_runLoopRunning = false;

    if (g_runLoopThread.joinable()) {
        g_runLoopThread.join();
    }

    if (m_eventTap) {
        CFRelease(m_eventTap);
        m_eventTap = nullptr;
        g_eventTap = nullptr;
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

#endif // __APPLE__
