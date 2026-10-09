// system_event_bridge 端到端：installSystemEventBridge 订阅统一系统事件源
// （systemwatch.* / filewatcher.* / trigger.fired / trigger.action /
// macro.state / macro.recorded）→ 原样转投 EventBuffer（method 与事件名一致，
// 不改名）→ GUI 经 events.drain 拉取。断言方法名透传、负载透传、timestamp
// 落盘；重复 install 幂等（同名订阅不叠加，防订阅泄漏回归）。
#include <gtest/gtest.h>

#include "wingman/runtime/notify_bridge.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/event.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {

std::size_t drainAll() {
    return wingman::runtime::EventBuffer::instance().drain(5000).size();
}

const wingman::runtime::IpcEvent* findEvent(const std::vector<wingman::runtime::IpcEvent>& events,
    const std::string& method) {
    for (const auto& e : events) {
        if (e.method == method) return &e;
    }
    return nullptr;
}

} // namespace

TEST(SystemEventBridgeTest, ForwardsSystemEventsToEventBuffer) {
    wingman::runtime::installSystemEventBridge();
    ASSERT_EQ(drainAll(), 0u);

    wingman::EventHub::instance().emit("systemwatch.process", json{{"action", "started"}, {"pid", 42}}, "test");
    wingman::EventHub::instance().emit("filewatcher.changed", json{{"type", "added"}, {"path", "/tmp/x"}}, "test");
    wingman::EventHub::instance().emit("trigger.action", json{{"name", "t1"}, {"actionCount", 2}}, "test");
    wingman::EventHub::instance().emit("macro.state", json{{"state", "playing"}}, "test");

    auto events = wingman::runtime::EventBuffer::instance().drain(100);

    // method 与事件名一一对应（不改名）
    const auto* proc = findEvent(events, "systemwatch.process");
    ASSERT_NE(proc, nullptr) << "systemwatch.process 未转发";
    EXPECT_EQ(proc->payload.value("pid", 0), 42);

    const auto* fw = findEvent(events, "filewatcher.changed");
    ASSERT_NE(fw, nullptr) << "filewatcher.changed 未转发";
    EXPECT_EQ(fw->payload.value("path", ""), "/tmp/x");

    const auto* act = findEvent(events, "trigger.action");
    ASSERT_NE(act, nullptr) << "trigger.action 未转发";
    EXPECT_EQ(act->payload.value("actionCount", 0), 2);

    const auto* macro = findEvent(events, "macro.state");
    ASSERT_NE(macro, nullptr) << "macro.state 未转发";
    EXPECT_EQ(macro->payload.value("state", ""), "playing");
    EXPECT_GT(macro->timestamp, 0u);
}

TEST(SystemEventBridgeTest, InstallIsIdempotent) {
    wingman::runtime::installSystemEventBridge();
    wingman::runtime::installSystemEventBridge(); // 二次安装必须 no-op
    ASSERT_EQ(drainAll(), 0u);

    wingman::EventHub::instance().emit("macro.recorded", json{{"type", "key_down"}}, "test");

    auto events = wingman::runtime::EventBuffer::instance().drain(100);
    int hits = 0;
    for (const auto& e : events) {
        if (e.method == "macro.recorded") ++hits;
    }
    EXPECT_EQ(hits, 1); // 订阅泄漏回归：重复 install 会被投递 2 次
}
