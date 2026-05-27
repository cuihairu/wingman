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

// ========== 同步发布 ==========

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

// ========== 事件属性 ==========

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

// ========== 异步发布（简化测试避免线程问题） ==========

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
