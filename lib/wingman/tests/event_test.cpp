#include <gtest/gtest.h>
#include "wingman/event.hpp"

using namespace wingman;

TEST(EventHubTest, SubscribeAndEmit) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count = 0;
    hub.subscribe("task.started", [&](const EventMessage& msg) {
        ++count;
        EXPECT_EQ(msg.type, "task.started");
        EXPECT_EQ(msg.payload["id"], "abc");
    });

    hub.emit("task.started", nlohmann::json{{"id", "abc"}}, "test");
    EXPECT_EQ(count, 1);
}

TEST(EventHubTest, OnceSubscription) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count = 0;
    hub.subscribe("task.done", [&](const EventMessage&) {
        ++count;
    }, "once_handler", true);

    hub.emit("task.done");
    hub.emit("task.done");
    EXPECT_EQ(count, 1);
}

TEST(EventHubTest, UnsubscribeByName) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count = 0;
    hub.subscribe("notify.info", [&](const EventMessage&) {
        ++count;
    }, "toast");

    hub.unsubscribe("toast");
    hub.emit("notify.info");
    EXPECT_EQ(count, 0);
}
