// notify_bridge 端到端：installNotifyBridge 订阅 notify.tray.* 意图事件 →
// 转投 EventBuffer（method 去掉 "notify." 前缀）→ GUI 经 events.drain 拉取。
// 断言转发命名、负载透传、timestamp 落盘；重复 install 幂等（同名订阅不叠加，
// 防止同一意图被转发 N 次的订阅泄漏回归）。
#include <gtest/gtest.h>

#include "wingman/runtime/notify_bridge.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/event.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {

/// 清场：drain 掉缓冲区内既有事件（其他测试/胶水可能残留），
/// 返回清掉的数量便于诊断。
std::size_t drainAll() {
    return wingman::runtime::EventBuffer::instance().drain(5000).size();
}

/// 在缓冲区里找指定 method 的事件（找不到返回 nullptr）
const wingman::runtime::IpcEvent* findEvent(const std::vector<wingman::runtime::IpcEvent>& events,
    const std::string& method) {
    for (const auto& e : events) {
        if (e.method == method) return &e;
    }
    return nullptr;
}

} // namespace

TEST(NotifyBridgeTest, ForwardsIntentEventsToEventBuffer) {
    wingman::runtime::installNotifyBridge();
    ASSERT_EQ(drainAll(), 0u);

    wingman::EventHub::instance().emit("notify.tray.show", json{{"ts", 1}}, "test");
    wingman::EventHub::instance().emit("notify.tray.badge", json{{"text", "3"}}, "test");

    auto events = wingman::runtime::EventBuffer::instance().drain(100);

    const auto* show = findEvent(events, "tray.show");
    ASSERT_NE(show, nullptr) << "notify.tray.show 未转发到 EventBuffer";
    EXPECT_EQ(show->payload.value("ts", 0), 1);

    const auto* badge = findEvent(events, "tray.badge");
    ASSERT_NE(badge, nullptr) << "notify.tray.badge 未转发到 EventBuffer";
    EXPECT_EQ(badge->payload.value("text", ""), "3");
    EXPECT_GT(badge->timestamp, 0u);
}

TEST(NotifyBridgeTest, InstallIsIdempotent) {
    wingman::runtime::installNotifyBridge();
    wingman::runtime::installNotifyBridge(); // 二次安装必须 no-op
    ASSERT_EQ(drainAll(), 0u);

    wingman::EventHub::instance().emit("notify.tray.tooltip", json{{"text", "hi"}}, "test");

    auto events = wingman::runtime::EventBuffer::instance().drain(100);
    int hits = 0;
    for (const auto& e : events) {
        if (e.method == "tray.tooltip") ++hits;
    }
    // 订阅泄漏回归：重复 install 下同一意图会被投递 2 次
    EXPECT_EQ(hits, 1);
}
