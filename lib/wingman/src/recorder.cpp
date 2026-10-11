#include "wingman/recorder.hpp"

#include "wingman/platform/input_factory.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <chrono>
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

// —— 平台无关状态机与导出/回放。三份平台实现曾各自复制这段逻辑并已漂移
// （win32/cocoa 导出前锁内快照，x11 直接读 m_events；recordEvent 的 emit
// 也分叉成锁内/无锁两种）。此处收敛为唯一实现：导出与回放统一走锁内
// 快照，recordEvent 锁内去重落库、锁外 emit（回调可能再入本类取快照）。
// 平台采集 start/stop 与平台态留在 src/platform/{win,linux,mac}/。

void MacroRecorder::pause() {
    m_paused.store(true, std::memory_order_relaxed);
    emitMacroState("paused");
}

void MacroRecorder::resume() {
    m_paused.store(false, std::memory_order_relaxed);
    emitMacroState("recording");
}

void MacroRecorder::clear() {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.clear();
}

// 在锁内拷贝一份事件快照，供后续无锁处理（文件 I/O、回放 sleep 等）
std::vector<RecordedEvent> MacroRecorder::getEventsSnapshot() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    return m_events;
}

size_t MacroRecorder::getEventCount() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    return m_events.size();
}

bool MacroRecorder::saveToLua(const std::string& filepath) const {
    const auto events = getEventsSnapshot();

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "-- Wingman Macro Recording Script\n";
    file << "-- Recorded " << events.size() << " events\n\n";

    file << "util.log(\"Starting macro playback...\")\n";
    file << "local startTime = util.getTime()\n\n";

    for (const auto& event : events) {
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
    const auto events = getEventsSnapshot();

    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"events\": [\n";

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        file << "    {\n";
        file << "      \"type\": " << static_cast<int>(event.type) << ",\n";
        file << "      \"timestamp\": " << event.timestamp << ",\n";
        file << "      \"x\": " << event.x << ",\n";
        file << "      \"y\": " << event.y << ",\n";
        file << "      \"button\": " << event.button << ",\n";
        file << "      \"keyCode\": " << event.keyCode << ",\n";
        file << "      \"delay\": " << event.delay << "\n";
        file << "    }" << (i < events.size() - 1 ? "," : "") << "\n";
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

        // 先解析到局部 vector，再一次性加锁赋值，缩短临界区。
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
    const auto events = getEventsSnapshot();
    if (events.empty()) return;

    emitMacroState("playing");

    for (int r = 0; r < repeat; ++r) {
        unsigned long lastTimestamp = events[0].timestamp;

        for (const auto& event : events) {
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

    emitMacroState("stopped");
}

void MacroRecorder::recordEvent(const RecordedEvent& event) {
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        if (!m_events.empty() && event.type == RecordedEventType::MouseMove) {
            if (m_events.back().type == RecordedEventType::MouseMove) {
                m_events.back() = event;
                return;
            }
        }

        m_events.push_back(event);
    }

    // 录制事件流导出：每条落库事件同步发 macro.recorded（source "macro"）。
    // 仅此一处（去重后的 MouseMove 提前返回，不发同型抖动事件）。
    // emit 在锁外：回调可能再入 getEventCount/saveToLua 取快照，锁内 emit
    // 会自锁。本函数由钩子/tap 线程调用，emit 同步跑订阅者回调——脚本侧
    // 非线程安全 callable 同受 systemwatch 同款约束。
    EventHub::instance().emit("macro.recorded", {
        {"type", recordedEventTypeName(event.type)},
        {"x", event.x},
        {"y", event.y},
        {"keyCode", event.keyCode},
        {"timestamp", event.timestamp},
    }, "macro");
}

} // namespace wingman
