#include "agentcore_test_common.hpp"

// ========== EventBuffer ==========

// 单例跨用例共享：每个用例前后清空并摘除 sink；
// dropped 计数器不清零，断言一律用「用例内增量」。
class EventBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& buffer = EventBuffer::instance();
        buffer.setRemoteSink(nullptr);
        buffer.clear();
        droppedBase_ = buffer.dropped();
    }
    void TearDown() override {
        auto& buffer = EventBuffer::instance();
        buffer.setRemoteSink(nullptr);
        buffer.clear();
    }

    std::size_t droppedBase_ = 0;
};

TEST_F(EventBufferTest, PushDrainRoundTripPreservesOrderAndFields) {
    auto& buffer = EventBuffer::instance();

    buffer.push("log.line", {{"text", "one"}});
    buffer.push("trigger.fired", {{"id", 7}});
    buffer.push("screenshot.frame", {{"w", 1280}});

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_EQ(buffer.dropped(), droppedBase_);

    auto events = buffer.drain(10);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].method, "log.line");
    EXPECT_EQ(events[0].payload, nlohmann::json({{"text", "one"}}));
    EXPECT_EQ(events[1].method, "trigger.fired");
    EXPECT_EQ(events[2].method, "screenshot.frame");
    for (const auto& evt : events) {
        EXPECT_GT(evt.timestamp, 0u);
    }
    // 时间戳毫秒级且不回退
    EXPECT_LE(events[0].timestamp, events[2].timestamp);

    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.drain(10).empty());
}

TEST_F(EventBufferTest, DrainRespectsMaxAndLeavesRest) {
    auto& buffer = EventBuffer::instance();
    for (int i = 0; i < 5; ++i) {
        buffer.push("m", {{"i", i}});
    }

    auto first = buffer.drain(2);
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0].payload["i"], 0);
    EXPECT_EQ(first[1].payload["i"], 1);
    EXPECT_EQ(buffer.size(), 3u);

    EXPECT_TRUE(buffer.drain(0).empty());
    EXPECT_EQ(buffer.size(), 3u);

    auto rest = buffer.drain(10);
    ASSERT_EQ(rest.size(), 3u);
    EXPECT_EQ(rest[0].payload["i"], 2);
}

TEST_F(EventBufferTest, DrainEmptyReturnsEmpty) {
    EXPECT_TRUE(EventBuffer::instance().drain(5).empty());
}

TEST_F(EventBufferTest, ClearEmptiesBuffer) {
    auto& buffer = EventBuffer::instance();
    buffer.push("a", {});
    buffer.push("b", {});
    EXPECT_EQ(buffer.size(), 2u);

    buffer.clear();
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.drain(5).empty());
}

TEST_F(EventBufferTest, OverflowDropsOldestWhenNoLogLine) {
    auto& buffer = EventBuffer::instance();
    for (std::size_t i = 0; i < EventBuffer::kMaxEvents + 3; ++i) {
        buffer.push("keep.a", {{"i", i}});
    }

    EXPECT_EQ(buffer.size(), EventBuffer::kMaxEvents);
    EXPECT_EQ(buffer.dropped(), droppedBase_ + 3);

    // 无 log.line 时按 FIFO 逐出：最早的 3 条被丢，队首是第 4 条
    auto front = buffer.drain(1);
    ASSERT_EQ(front.size(), 1u);
    EXPECT_EQ(front[0].payload["i"], 3);
}

TEST_F(EventBufferTest, OverflowPrefersEvictingLogLine) {
    auto& buffer = EventBuffer::instance();
    for (int i = 0; i < 997; ++i) {
        buffer.push("keep.a", {{"i", i}});
    }
    for (int i = 0; i < 3; ++i) {
        buffer.push("log.line", {{"i", 1000 + i}});
    }
    ASSERT_EQ(buffer.size(), EventBuffer::kMaxEvents);

    // 再进一条非 log 事件触发容量超限：优先逐出 log.line 而非最旧的 keep.a
    buffer.push("keep.a", {{"i", 9999}});
    EXPECT_EQ(buffer.size(), EventBuffer::kMaxEvents);
    EXPECT_EQ(buffer.dropped(), droppedBase_ + 1);

    int keep = 0, logs = 0;
    bool oldestKept = true;
    for (const auto& evt : buffer.drain(EventBuffer::kMaxEvents)) {
        if (evt.method == "keep.a") {
            ++keep;
            if (evt.payload["i"] == 0) oldestKept = true;
        } else {
            ++logs;
        }
    }
    EXPECT_EQ(keep, 998);
    EXPECT_EQ(logs, 2);
    EXPECT_TRUE(oldestKept);
}

TEST_F(EventBufferTest, RemoteSinkReceivesEachPushedEvent) {
    auto& buffer = EventBuffer::instance();

    std::string sinkMethod;
    nlohmann::json sinkPayload;
    buffer.setRemoteSink([&](const std::string& method, const nlohmann::json& payload) {
        sinkMethod = method;
        sinkPayload = payload;
    });

    buffer.push("trigger.fired", {{"id", 42}});
    EXPECT_EQ(sinkMethod, "trigger.fired");
    EXPECT_EQ(sinkPayload, nlohmann::json({{"id", 42}}));

    // sink 转发不影响本地缓冲
    EXPECT_EQ(buffer.size(), 1u);
}

TEST_F(EventBufferTest, SinkMayQueryBufferWithoutDeadlock) {
    auto& buffer = EventBuffer::instance();

    // sink 在锁外回调：回调里允许再查 size / 再 push（重入），不应死锁
    buffer.setRemoteSink([&buffer](const std::string&, const nlohmann::json&) {
        (void)buffer.size();
    });
    buffer.push("m", {});

    int depth = 0;
    buffer.setRemoteSink([&](const std::string& method, const nlohmann::json&) {
        if (method == "outer" && depth == 0) {
            depth = 1;
            EventBuffer::instance().push("inner", {});
        }
    });
    buffer.push("outer", {});

    EXPECT_EQ(depth, 1);
    EXPECT_EQ(buffer.size(), 3u); // outer + inner（两次 push 各缓冲一条）+ 首条 "m"
}

TEST_F(EventBufferTest, NullSinkStillBuffers) {
    auto& buffer = EventBuffer::instance();
    buffer.setRemoteSink(nullptr);
    buffer.push("m", {{"x", 1}});
    EXPECT_EQ(buffer.size(), 1u);
}

TEST_F(EventBufferTest, IpcEventToJsonHasThreeFields) {
    const IpcEvent evt{"log.line", {{"text", "hi"}}, 12345u};
    const auto json = evt.toJson();
    ASSERT_TRUE(json.is_object());
    EXPECT_EQ(json.size(), 3u);
    EXPECT_EQ(json["method"], "log.line");
    EXPECT_EQ(json["payload"], nlohmann::json({{"text", "hi"}}));
    EXPECT_EQ(json["timestamp"], 12345u);
}

TEST_F(EventBufferTest, InstanceReturnsSameObject) {
    EXPECT_EQ(&EventBuffer::instance(), &EventBuffer::instance());
}

