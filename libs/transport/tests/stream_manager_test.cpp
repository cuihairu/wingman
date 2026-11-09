#include "stream_transport_test_common.hpp"

// ========== StreamManager ==========

TEST(StreamManagerTest, CreateAndGetStream) {
    StreamManager manager;
    auto stream = manager.createStream(StreamType::CONTROL, "ctrl");
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->getType(), StreamType::CONTROL);
    EXPECT_EQ(manager.getStream("ctrl"), stream);
    EXPECT_TRUE(manager.hasStream("ctrl"));
    EXPECT_EQ(manager.getStreamCount(), 1u);
}

TEST(StreamManagerTest, GetMissingStreamReturnsNull) {
    StreamManager manager;
    EXPECT_EQ(manager.getStream("missing"), nullptr);
    EXPECT_FALSE(manager.hasStream("missing"));
    EXPECT_EQ(manager.getStreamCount(), 0u);
}

TEST(StreamManagerTest, CreateWithAutoName) {
    StreamManager manager;
    auto s1 = manager.createStream(StreamType::CONTROL, "");
    auto s2 = manager.createStream(StreamType::CONTROL, "");
    EXPECT_NE(s1, s2);
    EXPECT_EQ(manager.getStreamCount(), 2u);

    auto names = manager.getStreamNames();
    ASSERT_EQ(names.size(), 2u);
    EXPECT_NE(names[0], names[1]);
}

TEST(StreamManagerTest, CreateSameNameReplaces) {
    StreamManager manager;
    auto s1 = manager.createStream(StreamType::CONTROL, "same");
    auto s2 = manager.createStream(StreamType::SCREEN, "same");
    EXPECT_EQ(manager.getStreamCount(), 1u);
    EXPECT_EQ(manager.getStream("same"), s2);
    EXPECT_NE(manager.getStream("same"), s1);
}

TEST(StreamManagerTest, EmptyParamsGetDefaults) {
    StreamManager manager;
    auto stream = manager.createStream(StreamType::EVENT, "evt");  // 空 params -> 默认
    EXPECT_EQ(stream->getParams().type, StreamType::EVENT);
    EXPECT_EQ(stream->getParams().timeoutMs, 3000);
}

TEST(StreamManagerTest, CustomParamsKept) {
    StreamManager manager;
    StreamParams params;
    params.type = StreamType::SCREEN;
    params.timeoutMs = 777;
    auto stream = manager.createStream(StreamType::SCREEN, "custom", params);
    EXPECT_EQ(stream->getParams().timeoutMs, 777);
}

TEST(StreamManagerTest, RemoveStream) {
    StreamManager manager;
    manager.createStream(StreamType::CONTROL, "a");
    EXPECT_TRUE(manager.hasStream("a"));
    manager.removeStream("a");
    EXPECT_FALSE(manager.hasStream("a"));
    // 移除不存在的流是安全的
    EXPECT_NO_THROW(manager.removeStream("nope"));
}

TEST(StreamManagerTest, GetStreamsByType) {
    StreamManager manager;
    manager.createStream(StreamType::CONTROL, "c1");
    manager.createStream(StreamType::CONTROL, "c2");
    manager.createStream(StreamType::SCREEN, "s1");

    EXPECT_EQ(manager.getStreamsByType(StreamType::CONTROL).size(), 2u);
    EXPECT_EQ(manager.getStreamsByType(StreamType::SCREEN).size(), 1u);
    EXPECT_EQ(manager.getStreamsByType(StreamType::EVENT).size(), 0u);
}

TEST(StreamManagerTest, DisconnectAllAndClear) {
    StreamManager manager;
    auto s = manager.createStream(StreamType::CONTROL, "x");
    (void)s;
    EXPECT_NO_THROW(manager.disconnectAll());
    EXPECT_EQ(manager.getStreamCount(), 1u);

    manager.clear();
    EXPECT_EQ(manager.getStreamCount(), 0u);
    EXPECT_TRUE(manager.getStreamNames().empty());
}
