#include <gtest/gtest.h>
#include "wingman/core/event_bus.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

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

class HighPriorityEvent : public Event {
public:
    HighPriorityEvent() : Event(TEST_EVENT, EventPriority::High) {}
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

TEST(EventBusTest, UnsubscribeByName) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; }, "sub1");
    bus.unsubscribe("sub1");

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count.load(), 0);
}

TEST(EventBusTest, OnceSubscription) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; }, "", true);

    TestEvent e1;
    bus.publish(e1);
    bus.publish(e1);
    EXPECT_EQ(count.load(), 1);
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
    HighPriorityEvent e;
    EXPECT_EQ(e.getPriority(), EventPriority::High);

    e.setPriority(EventPriority::Critical);
    EXPECT_EQ(e.getPriority(), EventPriority::Critical);
}

// ========== 异步发布 ==========

TEST(EventBusTest, AsyncPublish) {
    EventBus bus;
    std::atomic<int> count{0};
    std::mutex m;
    std::condition_variable cv;

    bus.subscribe(TEST_EVENT, [&](const Event&) {
        count++;
        cv.notify_all();
    });

    bus.start();

    auto event = std::make_unique<TestEvent>();
    bus.publishAsync(std::move(event));

    {
        std::unique_lock<std::mutex> lock(m);
        cv.wait_for(lock, std::chrono::seconds(2), [&] { return count.load() > 0; });
    }

    bus.stop();
    EXPECT_EQ(count.load(), 1);
}

TEST(EventBusTest, StartStopIdempotent) {
    EventBus bus;
    bus.start();
    bus.start();
    EXPECT_TRUE(bus.isRunning());
    bus.stop();
    bus.stop();
    EXPECT_FALSE(bus.isRunning());
}

TEST(EventBusTest, QueueSize) {
    EventBus bus;
    EXPECT_EQ(bus.getQueueSize(), 0u);

    // Without starting, async events queue up
    for (int i = 0; i < 5; ++i) {
        auto event = std::make_unique<TestEvent>();
        bus.publishAsync(std::move(event));
    }
    EXPECT_GE(bus.getQueueSize(), 1u);
}

TEST(EventBusTest, ProcessEvents) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });

    for (int i = 0; i < 3; ++i) {
        auto event = std::make_unique<TestEvent>();
        bus.publishAsync(std::move(event));
    }

    bus.processEvents();
    EXPECT_EQ(count.load(), 3);
}

TEST(EventBusTest, IsNotRunningByDefault) {
    EventBus bus;
    EXPECT_FALSE(bus.isRunning());
}

// ========== 回调异常不影响其他订阅者 ==========

TEST(EventBusTest, ExceptionInCallbackDoesNotAffectOthers) {
    EventBus bus;
    std::atomic<int> count{0};

    bus.subscribe(TEST_EVENT, [&](const Event&) { throw std::runtime_error("oops"); });
    bus.subscribe(TEST_EVENT, [&](const Event&) { count++; });

    TestEvent event;
    bus.publish(event);
    EXPECT_EQ(count.load(), 1);
}
