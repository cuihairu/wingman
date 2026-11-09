#include "stream_transport_test_common.hpp"

// ========== stream_type.hpp ==========

TEST(StreamTypeTest, Names) {
    EXPECT_STREQ(streamTypeName(StreamType::CONTROL), "CONTROL");
    EXPECT_STREQ(streamTypeName(StreamType::SCREEN), "SCREEN");
    EXPECT_STREQ(streamTypeName(StreamType::EVENT), "EVENT");
    EXPECT_STREQ(streamTypeName(static_cast<StreamType>(99)), "UNKNOWN");
}

TEST(StreamTypeTest, DefaultParamsControl) {
    auto p = StreamParams::getDefault(StreamType::CONTROL);
    EXPECT_EQ(p.type, StreamType::CONTROL);
    EXPECT_TRUE(p.tcpNoDelay);
    EXPECT_TRUE(p.keepAlive);
    EXPECT_EQ(p.keepAliveIdle, 30);
    EXPECT_EQ(p.keepAliveInterval, 5);
    EXPECT_EQ(p.keepAliveCount, 3);
    EXPECT_EQ(p.maxMessageSize, 1u * 1024 * 1024);
    EXPECT_EQ(p.timeoutMs, 5000);
}

TEST(StreamTypeTest, DefaultParamsScreen) {
    auto p = StreamParams::getDefault(StreamType::SCREEN);
    EXPECT_EQ(p.type, StreamType::SCREEN);
    EXPECT_TRUE(p.tcpCork);
    EXPECT_EQ(p.sendBufferSize, 256 * 1024);
    EXPECT_EQ(p.recvBufferSize, 256 * 1024);
    EXPECT_EQ(p.maxMessageSize, 16u * 1024 * 1024);
    EXPECT_EQ(p.timeoutMs, 10000);
}

TEST(StreamTypeTest, DefaultParamsEvent) {
    auto p = StreamParams::getDefault(StreamType::EVENT);
    EXPECT_EQ(p.type, StreamType::EVENT);
    EXPECT_TRUE(p.tcpNoDelay);
    EXPECT_TRUE(p.keepAlive);
    EXPECT_EQ(p.maxMessageSize, 256u * 1024);
    EXPECT_EQ(p.timeoutMs, 3000);
}

TEST(StreamTypeTest, DefaultParamsUnknownTypeReturnsZeroed) {
    // switch 未覆盖的枚举值落入末尾的 return {}（全部字段零初始化）
    auto p = StreamParams::getDefault(static_cast<StreamType>(99));
    EXPECT_EQ(p.type, StreamType::CONTROL);  // 0
    EXPECT_FALSE(p.tcpNoDelay);
    EXPECT_FALSE(p.keepAlive);
    EXPECT_EQ(p.maxMessageSize, 0u);
    EXPECT_EQ(p.timeoutMs, 0);
}
