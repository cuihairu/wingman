#include "stream_transport_test_common.hpp"

// ========== Channel / ChannelManager (channel.hpp) ==========

TEST(ChannelTest, RequestWithoutSessionResolvesNull) {
    asio::io_context io;
    auto session = Session::create(1, asio::ip::tcp::socket(io));
    std::weak_ptr<Session> weak = session;
    session.reset();  // 让 session 失效

    Channel channel(1, ChannelType::RequestResponse, weak);
    auto future = channel.request(Message::create());
    // session 已失效，请求立即以 nullptr 完成
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(future.get(), nullptr);
}

TEST(ChannelTest, HandleResponseResolvesPendingRequest) {
    asio::io_context io;
    auto session = Session::create(1, asio::ip::tcp::socket(io));

    Channel channel(1, ChannelType::RequestResponse, session);
    auto request = Message::create(MessageType::Request, "ask");
    auto future = channel.request(request);
    EXPECT_EQ(request->header.sequence, 1u);

    auto response = Message::create(MessageType::Response, "answer");
    response->header.sequence = request->header.sequence;
    channel.handleResponse(response);

    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(future.get()->body, "answer");
}

TEST(ChannelTest, CloseCancelsPendingRequests) {
    asio::io_context io;
    auto session = Session::create(1, asio::ip::tcp::socket(io));

    Channel channel(1, ChannelType::RequestResponse, session);
    auto future = channel.request(Message::create());
    channel.close();
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(future.get(), nullptr);
}

TEST(ChannelTest, Getters) {
    asio::io_context io;
    auto session = Session::create(1, asio::ip::tcp::socket(io));
    Channel channel(7, ChannelType::PubSub, session);
    EXPECT_EQ(channel.getId(), 7u);
    EXPECT_EQ(channel.getType(), ChannelType::PubSub);
}

TEST(ChannelManagerTest, CreateGetCloseChannels) {
    ChannelManager manager;
    asio::io_context io;
    auto session = Session::create(1, asio::ip::tcp::socket(io));

    auto c1 = manager.createChannel(1, ChannelType::RequestResponse, session);
    ASSERT_NE(c1, nullptr);
    EXPECT_EQ(manager.getChannel(1), c1);
    EXPECT_EQ(manager.getChannel(999), nullptr);

    manager.closeChannel(1);
    EXPECT_EQ(manager.getChannel(1), nullptr);
    // 关闭不存在的通道安全
    EXPECT_NO_THROW(manager.closeChannel(123));
}

TEST(ChannelManagerTest, RemoveBySession) {
    ChannelManager manager;
    asio::io_context io;
    {
        auto session = Session::create(1, asio::ip::tcp::socket(io));
        manager.createChannel(1, ChannelType::Stream, session);
        // session 仍存活
        manager.removeBySession(1);
        EXPECT_NE(manager.getChannel(1), nullptr);
    }
    // session 析构后，removeBySession 应移除失效通道
    manager.removeBySession(1);
    EXPECT_EQ(manager.getChannel(1), nullptr);
}

TEST(ChannelManagerTest, CloseAllChannels) {
    ChannelManager manager;
    asio::io_context io;
    auto session = Session::create(1, asio::ip::tcp::socket(io));
    manager.createChannel(1, ChannelType::Stream, session);
    manager.createChannel(2, ChannelType::PubSub, session);
    manager.closeAll();
    EXPECT_EQ(manager.getChannel(1), nullptr);
    EXPECT_EQ(manager.getChannel(2), nullptr);
}
