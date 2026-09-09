/**
 * Simple Protocol Tests
 * 简化网络协议单元测试
 */

#include <gtest/gtest.h>
#include "wingman/transport/simple_protocol.hpp"
#include <asio.hpp>
#include <cstring>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

using namespace wingman::transport;

// ========== SimpleMessage 测试 ==========

TEST(SimpleMessageTest, CreateEmptyMessage) {
    std::vector<uint8_t> empty;
    auto msg = SimpleMessage::create(empty);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->size(), 0);
}

TEST(SimpleMessageTest, CreateFromString) {
    const std::string testStr = "Hello, Wingman!";
    auto msg = SimpleMessage::create(testStr);

    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->size(), testStr.size());
    EXPECT_EQ(msg->getPayloadAsString(), testStr);
}

TEST(SimpleMessageTest, CreateFromVector) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto msg = SimpleMessage::create(data);

    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->size(), 5);
    EXPECT_EQ(msg->getPayload(), data);
}

TEST(SimpleMessageTest, CreateTooLargeMessage) {
    // 创建超过 MAX_MESSAGE_SIZE 的消息
    std::vector<uint8_t> tooLarge(SimpleMessage::MAX_MESSAGE_SIZE + 1, 0);
    auto msg = SimpleMessage::create(tooLarge);
    EXPECT_EQ(msg, nullptr);
}

TEST(SimpleMessageTest, SerializeDeserialize) {
    const std::string originalData = "Test payload for serialization";
    auto original = SimpleMessage::create(originalData);

    // 序列化
    auto buffer = original->serialize();
    EXPECT_EQ(buffer.size(), SimpleMessage::LENGTH_SIZE + originalData.size());

    // 验证长度字段（前4字节）
    uint32_t length;
    std::memcpy(&length, buffer.data(), SimpleMessage::LENGTH_SIZE);
    length = Protocol::networkToHost32(length);
    EXPECT_EQ(length, originalData.size());
}

// ========== MessageReceiver 测试 ==========

TEST(MessageReceiverTest, ReceiveCompleteMessage) {
    const std::string testData = "Complete message";

    // 构造一个完整的消息 [长度][数据]
    std::vector<uint8_t> buffer;
    uint32_t length = Protocol::hostToNetwork32(static_cast<uint32_t>(testData.size()));
    const uint8_t* lengthPtr = reinterpret_cast<const uint8_t*>(&length);
    buffer.insert(buffer.end(), lengthPtr, lengthPtr + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), testData.begin(), testData.end());

    MessageReceiver receiver;
    auto messages = receiver.receive(buffer.data(), buffer.size());

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0]->getPayloadAsString(), testData);
    EXPECT_EQ(receiver.getBufferSize(), 0);  // 应该完全消耗
}

TEST(MessageReceiverTest, ReceiveMultipleMessages) {
    std::vector<uint8_t> buffer;

    // 第一个消息
    const std::string msg1 = "First";
    uint32_t len1 = Protocol::hostToNetwork32(static_cast<uint32_t>(msg1.size()));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&len1),
                 reinterpret_cast<uint8_t*>(&len1) + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), msg1.begin(), msg1.end());

    // 第二个消息
    const std::string msg2 = "Second";
    uint32_t len2 = Protocol::hostToNetwork32(static_cast<uint32_t>(msg2.size()));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&len2),
                 reinterpret_cast<uint8_t*>(&len2) + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), msg2.begin(), msg2.end());

    MessageReceiver receiver;
    auto messages = receiver.receive(buffer.data(), buffer.size());

    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0]->getPayloadAsString(), msg1);
    EXPECT_EQ(messages[1]->getPayloadAsString(), msg2);
}

TEST(MessageReceiverTest, ReceivePartialMessage) {
    const std::string testData = "Partial message test";

    // 只发送长度部分
    uint32_t length = Protocol::hostToNetwork32(static_cast<uint32_t>(testData.size()));
    std::vector<uint8_t> partialData;
    partialData.insert(partialData.end(), reinterpret_cast<uint8_t*>(&length),
                      reinterpret_cast<uint8_t*>(&length) + SimpleMessage::LENGTH_SIZE);
    // 只发送一半的数据
    partialData.insert(partialData.end(), testData.begin(),
                      testData.begin() + testData.size() / 2);

    MessageReceiver receiver;
    auto messages = receiver.receive(partialData.data(), partialData.size());

    // 应该没有完整消息
    EXPECT_EQ(messages.size(), 0);
    EXPECT_GT(receiver.getBufferSize(), 0);  // 数据在缓冲区中

    // 发送剩余数据
    std::vector<uint8_t> remainingData(testData.begin() + testData.size() / 2,
                                       testData.end());
    messages = receiver.receive(remainingData.data(), remainingData.size());

    // 现在应该有一个完整消息
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0]->getPayloadAsString(), testData);
}

TEST(MessageReceiverTest, ReceiveWithExtraData) {
    // 构造: [完整消息] [额外数据]
    // 额外数据必须少于 LENGTH_SIZE，否则会被当作长度前缀解析
    const std::string msgData = "Message";
    const std::string extraData = "Ex";

    std::vector<uint8_t> buffer;

    // 完整消息
    uint32_t len = Protocol::hostToNetwork32(static_cast<uint32_t>(msgData.size()));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&len),
                 reinterpret_cast<uint8_t*>(&len) + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), msgData.begin(), msgData.end());

    // 额外数据（不足一个消息）
    buffer.insert(buffer.end(), extraData.begin(), extraData.end());

    MessageReceiver receiver;
    auto messages = receiver.receive(buffer.data(), buffer.size());

    // 应该解析出一个消息，额外数据留在缓冲区
    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0]->getPayloadAsString(), msgData);
    EXPECT_EQ(receiver.getBufferSize(), extraData.size());
}

TEST(MessageReceiverTest, ClearBuffer) {
    // 不完整的数据（不足 LENGTH_SIZE，避免被当作长度前缀解析）
    std::vector<uint8_t> partialData = {1, 2, 3};

    MessageReceiver receiver;
    receiver.receive(partialData.data(), partialData.size());

    EXPECT_GT(receiver.getBufferSize(), 0);

    receiver.clear();
    EXPECT_EQ(receiver.getBufferSize(), 0);
}

TEST(MessageReceiverTest, ReceiveFromString) {
    const std::string testData = "String test";
    std::vector<uint8_t> buffer;

    uint32_t length = Protocol::hostToNetwork32(static_cast<uint32_t>(testData.size()));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&length),
                 reinterpret_cast<uint8_t*>(&length) + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), testData.begin(), testData.end());

    // 转换为字符串
    std::string bufferStr(buffer.begin(), buffer.end());

    MessageReceiver receiver;
    auto messages = receiver.receive(bufferStr);

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0]->getPayloadAsString(), testData);
}

// ========== Protocol 辅助函数测试 ==========

TEST(ProtocolTest, HostToNetwork32Conversion) {
    // 测试小端序机器上的转换
    uint32_t host = 0x12345678;
    uint32_t network = Protocol::hostToNetwork32(host);

    // 网络字节序应该是大端
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&network);
    EXPECT_EQ(bytes[0], 0x12);
    EXPECT_EQ(bytes[1], 0x34);
    EXPECT_EQ(bytes[2], 0x56);
    EXPECT_EQ(bytes[3], 0x78);
}

TEST(ProtocolTest, NetworkToHost32Conversion) {
    const uint32_t host_original = 0x12345678;
    // 先转为网络字节序再转回主机字节序，应得到原始值
    uint32_t network = Protocol::hostToNetwork32(host_original);
    uint32_t host = Protocol::networkToHost32(network);

    EXPECT_EQ(host, host_original);

    // 主机字节序在内存中的布局
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&host);
#ifdef __BYTE_ORDER__
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    EXPECT_EQ(bytes[0], 0x78);
    EXPECT_EQ(bytes[1], 0x56);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);
    #else
    EXPECT_EQ(bytes[0], 0x12);
    EXPECT_EQ(bytes[1], 0x34);
    EXPECT_EQ(bytes[2], 0x56);
    EXPECT_EQ(bytes[3], 0x78);
    #endif
#endif
}

TEST(ProtocolTest, RoundTripConversion) {
    uint32_t original = 0xDEADBEEF;
    uint32_t network = Protocol::hostToNetwork32(original);
    uint32_t restored = Protocol::networkToHost32(network);
    EXPECT_EQ(original, restored);
}

TEST(ProtocolTest, ZeroConversion) {
    uint32_t zero = 0;
    EXPECT_EQ(Protocol::hostToNetwork32(zero), 0);
    EXPECT_EQ(Protocol::networkToHost32(zero), 0);
}

// ========== 边界情况测试 ==========

TEST(MessageReceiverTest, ZeroLengthMessage) {
    std::vector<uint8_t> buffer;

    // 长度为0的消息
    uint32_t length = Protocol::hostToNetwork32(0);
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&length),
                 reinterpret_cast<uint8_t*>(&length) + SimpleMessage::LENGTH_SIZE);

    MessageReceiver receiver;
    auto messages = receiver.receive(buffer.data(), buffer.size());

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0]->size(), 0);
}

TEST(MessageReceiverTest, MaxSizeMessage) {
    // 测试最大允许的消息大小
    std::vector<uint8_t> maxData(SimpleMessage::MAX_MESSAGE_SIZE, 'X');

    std::vector<uint8_t> buffer;
    uint32_t length = Protocol::hostToNetwork32(SimpleMessage::MAX_MESSAGE_SIZE);
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&length),
                 reinterpret_cast<uint8_t*>(&length) + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), maxData.begin(), maxData.end());

    MessageReceiver receiver;
    auto messages = receiver.receive(buffer.data(), buffer.size());

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0]->size(), SimpleMessage::MAX_MESSAGE_SIZE);
}

TEST(MessageReceiverTest, OversizedMessage) {
    std::vector<uint8_t> buffer;

    // 超过最大大小的长度
    uint32_t length = Protocol::hostToNetwork32(SimpleMessage::MAX_MESSAGE_SIZE + 1);
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&length),
                 reinterpret_cast<uint8_t*>(&length) + SimpleMessage::LENGTH_SIZE);

    MessageReceiver receiver;
    auto messages = receiver.receive(buffer.data(), buffer.size());

    // 应该被拒绝，缓冲区被清空
    EXPECT_EQ(messages.size(), 0);
    EXPECT_EQ(receiver.getBufferSize(), 0);
}

TEST(MessageReceiverTest, EmptyReceive) {
    MessageReceiver receiver;
    auto messages = receiver.receive(reinterpret_cast<const uint8_t*>(""), 0);
    EXPECT_EQ(messages.size(), 0);
}

// ========== 序列化/反序列化测试 ==========

TEST(SimpleMessageTest, SerializeEmptyMessage) {
    auto msg = SimpleMessage::create("");
    auto buffer = msg->serialize();

    EXPECT_EQ(buffer.size(), SimpleMessage::LENGTH_SIZE);  // 只有长度字段
}

TEST(SimpleMessageTest, SerializeLargeMessage) {
    std::string largeData(10000, 'A');  // 10KB
    auto msg = SimpleMessage::create(largeData);
    auto buffer = msg->serialize();

    EXPECT_EQ(buffer.size(), SimpleMessage::LENGTH_SIZE + 10000);
}

TEST(MessageReceiverTest, FragmentedReceive) {
    const std::string testData = "Fragmented test data";
    std::vector<uint8_t> buffer;

    uint32_t length = Protocol::hostToNetwork32(static_cast<uint32_t>(testData.size()));
    buffer.insert(buffer.end(), reinterpret_cast<uint8_t*>(&length),
                  reinterpret_cast<uint8_t*>(&length) + SimpleMessage::LENGTH_SIZE);
    buffer.insert(buffer.end(), testData.begin(), testData.end());

    MessageReceiver receiver;

    // 每次只接收1个字节
    for (size_t i = 0; i < buffer.size(); ++i) {
        auto messages = receiver.receive(&buffer[i], 1);
        // 只有最后一个字节后才会得到完整消息
        if (i < buffer.size() - 1) {
            EXPECT_EQ(messages.size(), 0);
        } else {
            EXPECT_EQ(messages.size(), 1);
        }
    }

    EXPECT_EQ(receiver.getBufferSize(), 0);
}

// ========== Protocol Socket I/O 测试 ==========

namespace {

// 回环 TCP 连接对：两端均为阻塞 socket，可跨平台获取 native_handle()
class LoopbackPair {
public:
    LoopbackPair() {
        asio::ip::tcp::acceptor acceptor(io_,
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
        a_ = std::make_unique<asio::ip::tcp::socket>(io_);
        std::thread connector([&] {
            asio::error_code ec;
            a_->connect(acceptor.local_endpoint(), ec);
        });
        b_ = std::make_unique<asio::ip::tcp::socket>(acceptor.accept());
        connector.join();
    }

    Protocol::SocketType aHandle() const { return a_->native_handle(); }
    Protocol::SocketType bHandle() const { return b_->native_handle(); }

    void writeFromB(const uint8_t* data, size_t size) {
        asio::error_code ec;
        asio::write(*b_, asio::buffer(data, size), ec);
    }

    void closeB() {
        asio::error_code ec;
        b_->close(ec);
    }

    void resetBWithLinger() {
        // SO_LINGER{1,0} 后 close 会直接发送 RST 而不是 FIN
        asio::error_code ec;
        b_->set_option(asio::socket_base::linger(true, 0), ec);
        b_->close(ec);
    }

private:
    asio::io_context io_;
    std::unique_ptr<asio::ip::tcp::socket> a_;
    std::unique_ptr<asio::ip::tcp::socket> b_;
};

std::vector<uint8_t> makeFrame(uint32_t payloadLength) {
    uint32_t n = Protocol::hostToNetwork32(payloadLength);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&n);
    return std::vector<uint8_t>(p, p + SimpleMessage::LENGTH_SIZE);
}

} // namespace

TEST(ProtocolIOTest, SendMessageReadMessageRoundTrip) {
    LoopbackPair pair;

    auto original = SimpleMessage::create(std::string("round-trip payload"));
    std::error_code ec;
    ASSERT_TRUE(Protocol::sendMessage(pair.bHandle(), *original, ec));
    EXPECT_FALSE(ec);

    auto received = Protocol::readMessage(pair.aHandle(), ec);
    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->getPayloadAsString(), "round-trip payload");
    EXPECT_FALSE(ec);
}

TEST(ProtocolIOTest, SendMessageReadMessageEmptyPayload) {
    LoopbackPair pair;

    auto original = SimpleMessage::create(std::string{});
    std::error_code ec;
    ASSERT_TRUE(Protocol::sendMessage(pair.bHandle(), *original, ec));

    // 长度为 0 的消息：负载体循环不应执行
    auto received = Protocol::readMessage(pair.aHandle(), ec);
    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->size(), 0u);
}

TEST(ProtocolIOTest, ReadMessageAssemblesSplitWrites) {
    LoopbackPair pair;
    using namespace std::chrono_literals;

    const std::string payload = "assembled-from-split-writes";
    auto frame = makeFrame(static_cast<uint32_t>(payload.size()));

    // 长度字段拆成 2+2 字节发送，迫使读取端多次进入 recv 循环
    pair.writeFromB(frame.data(), 2);
    std::this_thread::sleep_for(20ms);
    pair.writeFromB(frame.data() + 2, 2);
    std::this_thread::sleep_for(20ms);

    // 负载拆成两段发送
    pair.writeFromB(reinterpret_cast<const uint8_t*>(payload.data()), payload.size() / 2);
    std::this_thread::sleep_for(20ms);
    pair.writeFromB(reinterpret_cast<const uint8_t*>(payload.data()) + payload.size() / 2,
                    payload.size() - payload.size() / 2);

    std::error_code ec;
    auto received = Protocol::readMessage(pair.aHandle(), ec);
    ASSERT_NE(received, nullptr);
    EXPECT_EQ(received->getPayloadAsString(), payload);
}

TEST(ProtocolIOTest, SendMessageOnInvalidSocketFails) {
    auto msg = SimpleMessage::create(std::string("data"));
    std::error_code ec;
    EXPECT_FALSE(Protocol::sendMessage(static_cast<Protocol::SocketType>(-1), *msg, ec));
    EXPECT_TRUE(ec);
}

TEST(ProtocolIOTest, ReadMessageOnInvalidSocketFails) {
    std::error_code ec;
    auto msg = Protocol::readMessage(static_cast<Protocol::SocketType>(-1), ec);
    EXPECT_EQ(msg, nullptr);
    EXPECT_TRUE(ec);
}

TEST(ProtocolIOTest, ReadMessageOnClosedConnectionReturnsCleanError) {
    LoopbackPair pair;
    pair.closeB();  // FIN -> recv 返回 0

    std::error_code ec;
    auto msg = Protocol::readMessage(pair.aHandle(), ec);
    EXPECT_EQ(msg, nullptr);
    EXPECT_FALSE(ec);  // 对端正常关闭：ec 保持为 0
}

TEST(ProtocolIOTest, ReadMessageOversizedLengthFails) {
    LoopbackPair pair;
    auto frame = makeFrame(SimpleMessage::MAX_MESSAGE_SIZE + 1);
    pair.writeFromB(frame.data(), frame.size());

    std::error_code ec;
    auto msg = Protocol::readMessage(pair.aHandle(), ec);
    EXPECT_EQ(msg, nullptr);
    EXPECT_TRUE(ec);
}

TEST(ProtocolIOTest, ReadMessageTruncatedPayloadByClose) {
    LoopbackPair pair;

    // 声明 8 字节负载，但只发送 3 字节后正常关闭
    auto frame = makeFrame(8);
    pair.writeFromB(frame.data(), frame.size());
    pair.writeFromB(reinterpret_cast<const uint8_t*>("abc"), 3);
    pair.closeB();

    std::error_code ec;
    auto msg = Protocol::readMessage(pair.aHandle(), ec);
    EXPECT_EQ(msg, nullptr);
    EXPECT_FALSE(ec);  // 负载体阶段遇到对端关闭
}

TEST(ProtocolIOTest, ReadMessagePayloadResetError) {
    LoopbackPair pair;
    using namespace std::chrono_literals;

    // 声明 8 字节负载，只发送 4 字节后以 RST 方式关闭
    auto frame = makeFrame(8);
    pair.writeFromB(frame.data(), frame.size());
    pair.writeFromB(reinterpret_cast<const uint8_t*>("abcd"), 4);
    // 等待数据送达对端后再触发 RST
    std::this_thread::sleep_for(100ms);
    pair.resetBWithLinger();

    std::error_code ec;
    auto msg = Protocol::readMessage(pair.aHandle(), ec);
    EXPECT_EQ(msg, nullptr);
    EXPECT_TRUE(ec);  // 负载体阶段遇到连接重置
}
