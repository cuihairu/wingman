#include "wingman/recorder.hpp"

#if defined(__linux__) && !defined(__APPLE__)

#include <chrono>
#include <thread>
#include <atomic>
#include <sys/time.h>

// X11 Record Extension
#include "x11_display.hpp"  // openX11Display：瞬态 accept 拒绝重试
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/record.h>

namespace wingman {

// X11 Record 采集态：控制/数据双 Display 连接 + record context + 回复处理线程
struct MacroRecorder::Impl {
    Display* controlDisplay{nullptr};
    unsigned long recordContext{0};
    std::thread processThread;
};

namespace {

void sleepMs(unsigned long milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

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

} // namespace

MacroRecorder::MacroRecorder()
    : m_impl(std::make_unique<Impl>()) {
    g_instance = this;
}

MacroRecorder::~MacroRecorder() {
    stop();
}

void MacroRecorder::start() {
    if (m_recording) return;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_events.clear();
    }
    m_recording = true;
    m_paused = false;
    m_startTime = getTickCount();

    // Open two Display connections（断开→重连竞态会瞬态拒绝，重试见 x11_display.hpp）
    Display* controlDisplay = platform::linux::openX11Display(nullptr);
    Display* dataDisplay = platform::linux::openX11Display(nullptr);

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

    m_impl->recordContext = XRecordCreateContext(controlDisplay, 0, &clients, 1, &range, 1);
    XFree(range);
    XSync(controlDisplay, False);  // 强制往返，让异步 X error 到达 handler

    if (!m_impl->recordContext || g_recordXError) {
        XSetErrorHandler(g_previousXHandler);
        if (m_impl->recordContext) {
            XRecordFreeContext(controlDisplay, m_impl->recordContext);
            m_impl->recordContext = 0;
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
    if (!XRecordEnableContextAsync(dataDisplay, m_impl->recordContext, eventCallback, nullptr)) {
        XSetErrorHandler(g_previousXHandler);
        XRecordFreeContext(controlDisplay, m_impl->recordContext);
        m_recording = false;
        XCloseDisplay(controlDisplay);
        XCloseDisplay(dataDisplay);
        return;
    }

    m_impl->controlDisplay = controlDisplay;
    g_display = dataDisplay;

    // Start processing thread
    m_impl->processThread = std::thread([this]() {
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

    emitMacroState("recording");
}

void MacroRecorder::stop() {
    if (!m_recording) return;

    m_recording = false;

    if (m_impl->processThread.joinable()) {
        m_impl->processThread.join();
    }

    if (m_impl->recordContext && m_impl->controlDisplay) {
        Display* dataDisplay = g_display;
        XRecordDisableContext(m_impl->controlDisplay, m_impl->recordContext);
        XRecordFreeContext(m_impl->controlDisplay, m_impl->recordContext);
        XCloseDisplay(m_impl->controlDisplay);
        XCloseDisplay(dataDisplay);
        m_impl->recordContext = 0;
        m_impl->controlDisplay = nullptr;
        g_display = nullptr;
        // 录制期结束，归还进程级 error handler
        XSetErrorHandler(g_previousXHandler);
    }

    emitMacroState("stopped");
}

} // namespace wingman

#endif // __linux__ && !__APPLE__
