#include "wingman/recorder.hpp"

#if defined(__linux__) && !defined(__APPLE__)

#include "wingman/platform/input_factory.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <thread>
#include <chrono>
#include <memory>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>

// X11 Record Extension
#include <X11/Xlib.h>
#include <X11/Xproto.h>
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

// RECORD 请求出错探针：Xlib 默认 error handler 会直接 exit() 杀死整个进程。
// CreateContext/EnableContext 的 X error（如 context 资源未就绪时的
// XRecordBadContext）必须转为「录制不可用」的优雅降级。
// 注：控制/数据双连接的请求无全局顺序，CreateContext 后必须对控制连接
// XSync 确保 context 先于 EnableContext 落达 server。
static std::atomic<bool> g_recordXError{false};
static std::atomic<bool> g_recordDataFlowing{false};
static XErrorHandler g_previousXHandler = nullptr;

static int recordXErrorHandler(Display*, XErrorEvent*) {
    g_recordXError = true;
    return 0;  // 吞掉错误，交由调用方检测 g_recordXError 降级
}

static unsigned long getTickCount() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<unsigned long>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// X11 Record callback
static void eventCallback(XPointer priv, XRecordInterceptData* data) {
    // 任何拦截数据（含 StartOfData）都证明 context 已成功启用并开始推送
    g_recordDataFlowing = true;
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
      m_display(nullptr), m_recordContext(0) {
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

    // 装宽容 error handler：CreateContext/EnableContext 的 X error 不再走默认
    // exit() 路径，改为以 g_recordXError 探针识别并优雅降级为「录制不可用」
    g_recordXError = false;
    g_previousXHandler = XSetErrorHandler(recordXErrorHandler);

    m_recordContext = XRecordCreateContext(controlDisplay, 0, &clients, 1, &range, 1);
    XFree(range);
    XSync(controlDisplay, False);  // 强制往返，让异步 X error 到达 handler

    if (!m_recordContext || g_recordXError) {
        XSetErrorHandler(g_previousXHandler);
        if (m_recordContext) {
            XRecordFreeContext(controlDisplay, m_recordContext);
            m_recordContext = 0;
        }
        m_recording = false;
        XCloseDisplay(controlDisplay);
        XCloseDisplay(dataDisplay);
        return;
    }

    // 流动标志必须在 enable 之前清零：libXtst 的 EnableContextAsync 会在返回
    // 前同步投递 StartOfData 到回调（实测 cat=XRecordStartOfData 在 enable
    // 返回值求值期间先至）。若在 enable 之后再清零，会把回调刚置位的标志
    // 抹掉，下方 300ms 流动性检查永远超时，任何健康 X server 上录制都无法
    // 启动（此前被误判为「Xvfb 必然失败」，实为本序缺陷）。
    g_recordDataFlowing = false;
    if (!XRecordEnableContextAsync(dataDisplay, m_recordContext, eventCallback, nullptr)) {
        XSetErrorHandler(g_previousXHandler);
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

    // EnableContext 是异步请求且此后 data 连接进入流式状态——绝不能对它
    // XSync（流不终止，XSync 永久挂起）。改为等处理线程消费到首条拦截数据
    // （健康服务器毫秒级送达 StartOfData）或 X error 落地，超时按不可用降级。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
    while (!g_recordDataFlowing && !g_recordXError && std::chrono::steady_clock::now() < deadline) {
        sleepMs(5);
    }
    if (g_recordXError || !g_recordDataFlowing) {
        stop();  // 复用清理：join 线程 + disable/free context + close display + 还原 handler
        return;
    }
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
        m_recordContext = 0;
        m_display = nullptr;
        g_display = nullptr;
        // 录制期结束，归还进程级 error handler
        XSetErrorHandler(g_previousXHandler);
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

size_t MacroRecorder::getEventCount() const {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    return m_events.size();
}

} // namespace wingman

#endif // __linux__ && !__APPLE__
