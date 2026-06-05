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

TEST(EventHubTest, UnsubscribeById) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count = 0;
    auto id = hub.subscribe("test.byid", [&](const EventMessage&) {
        ++count;
    });

    hub.emit("test.byid");
    EXPECT_EQ(count, 1);

    hub.unsubscribe(id);
    hub.emit("test.byid");
    EXPECT_EQ(count, 1);
}

TEST(EventHubTest, UnsubscribeNonexistentIdDoesNotCrash) {
    auto& hub = EventHub::instance();
    hub.clear();

    EXPECT_NO_THROW(hub.unsubscribe(999999));
}

TEST(EventHubTest, UnsubscribeByNameNonexistentDoesNotCrash) {
    auto& hub = EventHub::instance();
    hub.clear();

    EXPECT_NO_THROW(hub.unsubscribe("nonexistent_handler_name"));
}

TEST(EventHubTest, EmitToNoSubscribersDoesNotCrash) {
    auto& hub = EventHub::instance();
    hub.clear();

    EXPECT_NO_THROW(hub.emit("no.listeners.event"));
}

TEST(EventHubTest, EmitWithAllParameters) {
    auto& hub = EventHub::instance();
    hub.clear();

    EventMessage received;
    hub.subscribe("full.params", [&](const EventMessage& msg) {
        received = msg;
    });

    nlohmann::json payload = {{"key", "value"}};
    hub.emit("full.params", payload, "test_source", "corr-123", 5);

    EXPECT_EQ(received.type, "full.params");
    EXPECT_EQ(received.source, "test_source");
    EXPECT_EQ(received.correlationId, "corr-123");
    EXPECT_EQ(received.priority, 5);
    EXPECT_GT(received.timestamp, 0u);
}

TEST(EventHubTest, MultipleSubscriptionsSameType) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count1 = 0, count2 = 0;
    hub.subscribe("multi.test", [&](const EventMessage&) { ++count1; });
    hub.subscribe("multi.test", [&](const EventMessage&) { ++count2; });

    hub.emit("multi.test");
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

TEST(EventHubTest, OnceSubscriptionRemovesOnlyOnce) {
    auto& hub = EventHub::instance();
    hub.clear();

    int onceCount = 0, persistCount = 0;
    hub.subscribe("mixed.test", [&](const EventMessage&) { ++onceCount; }, "once_h", true);
    hub.subscribe("mixed.test", [&](const EventMessage&) { ++persistCount; }, "persist_h", false);

    hub.emit("mixed.test");
    hub.emit("mixed.test");

    EXPECT_EQ(onceCount, 1);
    EXPECT_EQ(persistCount, 2);
}

TEST(EventHubTest, ClearRemovesAllSubscriptions) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count = 0;
    hub.subscribe("clear.test", [&](const EventMessage&) { ++count; });

    hub.emit("clear.test");
    EXPECT_EQ(count, 1);

    hub.clear();
    hub.emit("clear.test");
    EXPECT_EQ(count, 1);
}

TEST(EventHubTest, UnsubscribeByNameMultipleSubscriptions) {
    auto& hub = EventHub::instance();
    hub.clear();

    int count1 = 0, count2 = 0;
    hub.subscribe("name.multi", [&](const EventMessage&) { ++count1; }, "group_a");
    hub.subscribe("name.multi", [&](const EventMessage&) { ++count2; }, "group_a");

    hub.unsubscribe("group_a");
    hub.emit("name.multi");
    EXPECT_EQ(count1, 0);
    EXPECT_EQ(count2, 0);
}

TEST(EventHubTest, EmitWithEmptyPayload) {
    auto& hub = EventHub::instance();
    hub.clear();

    bool received = false;
    hub.subscribe("empty.payload", [&](const EventMessage& msg) {
        received = true;
        EXPECT_TRUE(msg.payload.is_null() || msg.payload.empty());
    });

    hub.emit("empty.payload");
    EXPECT_TRUE(received);
}

TEST(EventHubTest, SubscribeReturnsIncrementingId) {
    auto& hub = EventHub::instance();
    hub.clear();

    auto id1 = hub.subscribe("id.test1", [](const EventMessage&) {});
    auto id2 = hub.subscribe("id.test2", [](const EventMessage&) {});
    EXPECT_GT(id2, id1);
}
