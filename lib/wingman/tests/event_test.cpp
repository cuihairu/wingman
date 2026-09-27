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

// ========== 监听器查询与按事件名清理 ==========

TEST(EventHubTest, SubscriptionQueryById) {
    auto& hub = EventHub::instance();
    hub.clear();

    auto id = hub.subscribe("query.byid", [](const EventMessage&) {}, "named_h", true);
    auto info = hub.subscription(id);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->id, id);
    EXPECT_EQ(info->type, "query.byid");
    EXPECT_EQ(info->name, "named_h");
    EXPECT_TRUE(info->once);

    EXPECT_FALSE(hub.subscription(999999).has_value());

    hub.unsubscribe(id);
    EXPECT_FALSE(hub.subscription(id).has_value());
}

TEST(EventHubTest, SubscriptionByNameReturnsEarliest) {
    auto& hub = EventHub::instance();
    hub.clear();

    auto first = hub.subscribe("by.name.first", [](const EventMessage&) {}, "dup_h");
    hub.subscribe("by.name.second", [](const EventMessage&) {}, "dup_h");

    auto info = hub.subscriptionByName("dup_h");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->id, first);
    EXPECT_EQ(info->type, "by.name.first");
    EXPECT_FALSE(info->once);

    EXPECT_FALSE(hub.subscriptionByName("missing_h").has_value());

    // 匿名订阅（name 为空串）不可按名查询
    hub.subscribe("by.name.anon", [](const EventMessage&) {});
    EXPECT_FALSE(hub.subscriptionByName("").has_value());
}

TEST(EventHubTest, SubscriptionsForTypeListsAll) {
    auto& hub = EventHub::instance();
    hub.clear();

    auto id1 = hub.subscribe("list.type", [](const EventMessage&) {}, "a");
    auto id2 = hub.subscribe("list.type", [](const EventMessage&) {});
    auto id3 = hub.subscribe("list.type", [](const EventMessage&) {}, "c", true);
    hub.subscribe("other.type", [](const EventMessage&) {}, "d");

    auto infos = hub.subscriptionsForType("list.type");
    ASSERT_EQ(infos.size(), 3u);
    // 按订阅 ID 升序，即注册顺序
    EXPECT_EQ(infos[0].id, id1);
    EXPECT_EQ(infos[0].type, "list.type");
    EXPECT_EQ(infos[0].name, "a");
    EXPECT_FALSE(infos[0].once);
    EXPECT_EQ(infos[1].id, id2);
    EXPECT_EQ(infos[1].name, "");
    EXPECT_EQ(infos[2].id, id3);
    EXPECT_TRUE(infos[2].once);

    EXPECT_TRUE(hub.subscriptionsForType("other.type").size() == 1u);
    EXPECT_TRUE(hub.subscriptionsForType("no.such.type").empty());
}

TEST(EventHubTest, ClearByTypeOnlyClearsThatEvent) {
    auto& hub = EventHub::instance();
    hub.clear();

    int countTarget = 0, countOther = 0;
    hub.subscribe("clear.target", [&](const EventMessage&) { ++countTarget; });
    hub.subscribe("clear.target", [&](const EventMessage&) { ++countTarget; }, "t2");
    hub.subscribe("clear.other", [&](const EventMessage&) { ++countOther; });

    hub.clear("clear.target");
    EXPECT_TRUE(hub.subscriptionsForType("clear.target").empty());

    hub.emit("clear.target");
    EXPECT_EQ(countTarget, 0);
    hub.emit("clear.other");
    EXPECT_EQ(countOther, 1);

    // 清理不存在的事件：无副作用不崩溃
    EXPECT_NO_THROW(hub.clear("never.subscribed"));
    EXPECT_FALSE(hub.subscriptionByName("t2").has_value());

    hub.clear();
}
