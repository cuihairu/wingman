/**
 * Wingman Transport Tests
 * 网络传输模块单元测试
 */

#include <gtest/gtest.h>
#include "wingman/transport/session/session.hpp"
#include <thread>
#include <chrono>

using namespace wingman::transport;

// ========== Message 测试 ==========

TEST(MessageTest, CreateEmptyMessage) {
    auto msg = Message::create();
    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(msg->body.empty());
}

TEST(MessageTest, CreateTypedMessage) {
    const std::string testBody = "Hello, Wingman!";
    auto msg = Message::create(MessageType::Notify, testBody);

    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->header.type, MessageType::Notify);
    EXPECT_EQ(msg->body, testBody);
}

TEST(MessageTest, SerializeDeserialize) {
    auto original = Message::create(MessageType::Response, "Test payload");

    // 序列化
    auto buffer = original->serialize();
    EXPECT_EQ(buffer.size(), sizeof(MessageHeader) + original->body.size());

    // 反序列化
    auto deserialized = Message::deserialize(buffer);
    ASSERT_NE(deserialized, nullptr);

    EXPECT_EQ(deserialized->header.type, original->header.type);
    EXPECT_EQ(deserialized->body, original->body);
}

TEST(MessageTest, DeserializeInvalidBuffer) {
    std::vector<uint8_t> tooSmall(1);
    auto msg = Message::deserialize(tooSmall);
    EXPECT_EQ(msg, nullptr);
}

TEST(MessageTest, SerializeUpdatesLength) {
    auto msg = Message::create(MessageType::Request, "Data");
    msg->header.length = 0;

    auto buffer = msg->serialize();

    auto deserialized = Message::deserialize(buffer);
    ASSERT_NE(deserialized, nullptr);
    EXPECT_EQ(deserialized->header.length, 4);
}

// ========== MessageType 测试 ==========

TEST(MessageTypeTest, ValuesAreDistinct) {
    EXPECT_NE(MessageType::Request, MessageType::Response);
    EXPECT_NE(MessageType::Request, MessageType::Notify);
    EXPECT_NE(MessageType::Request, MessageType::Error);
}

// ========== SessionEvent 测试 ==========

TEST(SessionEventTest, ValuesAreDistinct) {
    EXPECT_NE(SessionEvent::Connected, SessionEvent::Disconnected);
    EXPECT_NE(SessionEvent::Connected, SessionEvent::Error);
    EXPECT_NE(SessionEvent::Connected, SessionEvent::Timeout);
}
