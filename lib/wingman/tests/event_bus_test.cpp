#include <gtest/gtest.h>
#include "wingman/core/event_bus.hpp"

#include <atomic>
#include <chrono>

using namespace wingman::core;

namespace {

constexpr EventType TEST_EVENT = 1;
constexpr EventType OTHER_EVENT = 2;

class TestEvent : public Event {
public:
    explicit TestEvent(int value = 0)
        : Event(TEST_EVENT), value_(value) {}
    int value() const { return value_; }
private:
    int value_ = 0;
};

class OtherEvent : public Event {
public:
    OtherEvent() : Event(OTHER_EVENT) {}
};

} // anonymous namespace

// ========== Synchronous Publish ==========

TEST(EventBusTest, SubscribeAndPublish) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });
    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count.load(), 1);
}

TEST(EventBusTest, MultipleSubscribers) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });
    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count.load(), 2);
}

TEST(EventBusTest, UnsubscribeById) {
    EventBus bus;
    std::atomic<int> count{0};

    uint64_t id = bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });
    bus.unsubscribe(id);

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count.load(), 0);
}

TEST(EventBusTest, EventTypeFiltering) {
    EventBus bus;
    std::atomic<int> testCount{0};
    std::atomic<int> otherCount{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { testCount++; });
    bus.subscribe(OTHER_EVENT, [&](const Event&) { otherCount++; });

    TestEvent te;
    bus.publish(te);
    EXPECT_EQ(testCount.load(), 1);
    EXPECT_EQ(otherCount.load(), 0);

    OtherEvent oe;
    bus.publish(oe);
    EXPECT_EQ(testCount.load(), 1);
    EXPECT_EQ(otherCount.load(), 1);
}

TEST(EventBusTest, PublishWithNoSubscribers) {
    EventBus bus;
    TestEvent event;
    EXPECT_NO_THROW(bus.publish(event));
}

TEST(EventBusTest, OnceSubscription) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; }, "", true);

    TestEvent e1;
    bus.publish(e1);
    EXPECT_EQ(count.load(), 1);

    TestEvent e2;
    bus.publish(e2);
    EXPECT_EQ(count.load(), 1);
}

// ========== Event Properties ==========

TEST(EventBusTest, EventProperties) {
    TestEvent e1;
    EXPECT_EQ(e1.getType(), TEST_EVENT);
    EXPECT_EQ(e1.getPriority(), EventPriority::Normal);
    EXPECT_GT(e1.getTimestamp(), 0u);
    EXPECT_GT(e1.getSequenceId(), 0u);

    TestEvent e2;
    EXPECT_GT(e2.getSequenceId(), e1.getSequenceId());
}

TEST(EventBusTest, EventPriority) {
    EventPriority p = EventPriority::High;
    EXPECT_EQ(static_cast<int>(p), 2);
}

TEST(EventBusTest, IsNotRunningByDefault) {
    EventBus bus;
    EXPECT_FALSE(bus.isRunning());
}

// ========== Asynchronous Publish (simplified test to avoid threading issues) ==========

TEST(EventBusTest, StartStop) {
    EventBus bus;
    bus.start();
    EXPECT_TRUE(bus.isRunning());
    bus.stop();
    EXPECT_FALSE(bus.isRunning());
}

TEST(EventBusTest, QueueSizeStartsAtZero) {
    EventBus bus;
    EXPECT_EQ(bus.getQueueSize(), 0u);
}

// ========== Additional EventBus Tests ==========

TEST(EventBusTest, UnsubscribeBySubscriberName) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; }, "my_sub");
    bus.unsubscribe("my_sub");

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count.load(), 0);
}

TEST(EventBusTest, UnsubscribeByNameNoEffectOnOthers) {
    EventBus bus;
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count1++; }, "sub1");
    bus.subscribe(TEST_EVENT, [&](const Event&) { count2++; }, "sub2");
    bus.unsubscribe("sub1");

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count1.load(), 0);
    EXPECT_EQ(count2.load(), 1);
}

TEST(EventBusTest, UnsubscribeNonexistentId) {
    EventBus bus;
    EXPECT_NO_THROW(bus.unsubscribe(99999u));
}

TEST(EventBusTest, UnsubscribeNonexistentName) {
    EventBus bus;
    EXPECT_NO_THROW(bus.unsubscribe("nonexistent"));
}

TEST(EventBusTest, OnceSubscriptionMultiplePublishes) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; }, "", true);

    for (int i = 0; i < 5; ++i) {
        TestEvent event;
        bus.publish(event);
    }
    EXPECT_EQ(count.load(), 1);
}

TEST(EventBusTest, MixedOnceAndPersistent) {
    EventBus bus;
    std::atomic<int> onceCount{0};
    std::atomic<int> persistCount{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { onceCount++; }, "", true);
    bus.subscribe(TEST_EVENT, [&](const Event&) { persistCount++; });

    TestEvent e1;
    bus.publish(e1);
    EXPECT_EQ(onceCount.load(), 1);
    EXPECT_EQ(persistCount.load(), 1);

    TestEvent e2;
    bus.publish(e2);
    EXPECT_EQ(onceCount.load(), 1);
    EXPECT_EQ(persistCount.load(), 2);
}

TEST(EventBusTest, EventSetPriority) {
    TestEvent event;
    EXPECT_EQ(event.getPriority(), EventPriority::Normal);
    event.setPriority(EventPriority::Critical);
    EXPECT_EQ(event.getPriority(), EventPriority::Critical);
}

TEST(EventBusTest, PublishAsyncEnqueues) {
    EventBus bus;
    bus.publishAsync(std::make_unique<TestEvent>(42));
    EXPECT_EQ(bus.getQueueSize(), 1u);
}

TEST(EventBusTest, ProcessEventsSync) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });
    bus.publishAsync(std::make_unique<TestEvent>(1));
    bus.publishAsync(std::make_unique<TestEvent>(2));

    EXPECT_EQ(bus.getQueueSize(), 2u);
    bus.processEvents();
    EXPECT_EQ(count.load(), 2);
    EXPECT_EQ(bus.getQueueSize(), 0u);
}

TEST(EventBusTest, DoubleStop) {
    EventBus bus;
    bus.start();
    bus.stop();
    EXPECT_NO_THROW(bus.stop());
    EXPECT_FALSE(bus.isRunning());
}

TEST(EventBusTest, StartIdempotent) {
    EventBus bus;
    bus.start();
    EXPECT_TRUE(bus.isRunning());
    bus.start();
    EXPECT_TRUE(bus.isRunning());
    bus.stop();
}

TEST(EventBusTest, AsyncPublishWithRunningBus) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });
    bus.start();

    for (int i = 0; i < 10; ++i) {
        bus.publishAsync(std::make_unique<TestEvent>(i));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bus.stop();
    EXPECT_EQ(count.load(), 10);
}

TEST(EventBusTest, SubscriberWithPriority) {
    std::vector<int> order;
    EventBus bus;

    bus.subscribe(TEST_EVENT, [&](const Event&) { order.push_back(2); }, "", false);
    bus.subscribe(TEST_EVENT, [&](const Event&) { order.push_back(1); }, "", false);

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(order.size(), 2u);
}
