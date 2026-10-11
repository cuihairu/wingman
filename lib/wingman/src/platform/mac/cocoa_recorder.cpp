#include "wingman/recorder.hpp"

#ifdef __APPLE__

#include <thread>
#include <chrono>
#include <memory>
#include <sys/time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>

namespace wingman {

// macOS 事件 tap 采集态：CGEventTap + run loop source
struct MacroRecorder::Impl {
    CFMachPortRef eventTap{nullptr};
    CFRunLoopSourceRef runLoopSource{nullptr};
};

namespace {

MacroRecorder* g_instance = nullptr;
static CFMachPortRef g_eventTap = nullptr;
static std::thread g_runLoopThread;
static bool g_runLoopRunning = false;

static unsigned long getTickCount() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return static_cast<unsigned long>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// CGEventTap callback
static CGEventRef eventTapCallback(CGEventTapProxy /*proxy*/, CGEventType type,
                                    CGEventRef event, void* /*refcon*/) {
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
    (void)CFRunLoopGetCurrent();
    g_runLoopRunning = true;

    while (g_runLoopRunning) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    }

    g_runLoopRunning = false;
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

    m_impl->eventTap = CGEventTapCreate(kCGSessionEventTap,
                                        kCGHeadInsertEventTap,
                                        kCGEventTapOptionDefault,
                                        eventMask,
                                        eventTapCallback,
                                        nullptr);

    if (!m_impl->eventTap) {
        m_recording = false;
        return;
    }

    g_eventTap = m_impl->eventTap;
    m_impl->runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_impl->eventTap, 0);

    if (!m_impl->runLoopSource) {
        CFRelease(m_impl->eventTap);
        m_impl->eventTap = nullptr;
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

    CFRunLoopAddSource(CFRunLoopGetCurrent(), m_impl->runLoopSource, kCFRunLoopCommonModes);
    CGEventTapEnable(m_impl->eventTap, true);

    emitMacroState("recording");
}

void MacroRecorder::stop() {
    if (!m_recording) return;

    m_recording = false;

    if (m_impl->eventTap) {
        CGEventTapEnable(m_impl->eventTap, false);
    }

    if (m_impl->runLoopSource) {
        CFRunLoopRemoveSource(CFRunLoopGetCurrent(), m_impl->runLoopSource, kCFRunLoopCommonModes);
        CFRelease(m_impl->runLoopSource);
        m_impl->runLoopSource = nullptr;
    }

    g_runLoopRunning = false;

    if (g_runLoopThread.joinable()) {
        g_runLoopThread.join();
    }

    if (m_impl->eventTap) {
        CFRelease(m_impl->eventTap);
        m_impl->eventTap = nullptr;
        g_eventTap = nullptr;
    }

    emitMacroState("stopped");
}

} // namespace wingman

#endif // __APPLE__
