#include "stream_transport_test_common.hpp"

// ========== StreamChannel ==========

TEST(StreamChannelTest, InitialState) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_EQ(channel.getType(), StreamType::CONTROL);
    EXPECT_EQ(channel.getState(), StreamState::Disconnected);
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getRemoteEndpoint(), "");
    EXPECT_EQ(channel.getLocalPort(), 0);
}

TEST(StreamChannelTest, StreamStateName) {
    EXPECT_STREQ(streamStateName(StreamState::Disconnected), "Disconnected");
    EXPECT_STREQ(streamStateName(StreamState::Connecting), "Connecting");
    EXPECT_STREQ(streamStateName(StreamState::Connected), "Connected");
    EXPECT_STREQ(streamStateName(StreamState::Disconnecting), "Disconnecting");
    EXPECT_STREQ(streamStateName(StreamState::Error), "Error");
    EXPECT_STREQ(streamStateName(static_cast<StreamState>(99)), "Unknown");
}

TEST(StreamChannelTest, ConnectInvalidPortFails) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.connect("127.0.0.1", 70000));
    EXPECT_EQ(channel.getState(), StreamState::Error);

    StreamChannel channel2(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel2.connect("127.0.0.1", -1));
}

TEST(StreamChannelTest, ConnectUnresolvableHostFails) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    // 使用语法非法的主机名（空格字符，RFC 1123 不允许出现在主机名中），
    // getaddrinfo 在所有平台都会本地直接拒绝，不发起任何 DNS 查询。
    // 不要使用语法合法但期望解析失败的名字（如 .invalid TLD）：在通配符
    // DNS 或配置了搜索域的网络中它们可能被解析成功，导致测试偶发失败。
    EXPECT_FALSE(channel.connect("unresolvable host name", 12345));
    EXPECT_EQ(channel.getState(), StreamState::Error);

    // 超过 63 字符的标签同样是本地立即拒绝的语法错误
    StreamChannel channel2(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel2.connect(std::string(64, 'a') + ".invalid", 12345));
    EXPECT_EQ(channel2.getState(), StreamState::Error);
}

TEST(StreamChannelTest, ConnectEmptyHostFails) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.connect("", 12345));
    EXPECT_EQ(channel.getState(), StreamState::Error);
}

TEST(StreamChannelTest, ConnectWhenNotDisconnectedFails) {
    int port = findFreePort();
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    ASSERT_TRUE(channel.listen("127.0.0.1", port));
    // 已处于监听（Connected）状态时再次 connect 应被拒绝
    EXPECT_FALSE(channel.connect("127.0.0.1", port));
    channel.disconnect();
}

TEST(StreamChannelTest, ConnectViaHostnameToRefusedPortFails) {
    int port = findFreePort();  // 无监听 -> 连接拒绝
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    // 通过域名解析（getaddrinfo 成功）后再连接失败
    EXPECT_FALSE(channel.connect("localhost", port));
    EXPECT_EQ(channel.getState(), StreamState::Error);
}

TEST(StreamChannelTest, ListenOnInUsePortFails) {
    int port = findFreePort();

    // 用另一个 acceptor 占住端口
    asio::io_context io;
    asio::ip::tcp::acceptor holder(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port);
    holder.open(endpoint.protocol());
    asio::error_code bindEc;
    holder.bind(endpoint, bindEc);
    ASSERT_FALSE(bindEc);
    holder.listen();

    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.listen("127.0.0.1", port));
    EXPECT_EQ(channel.getState(), StreamState::Error);
}

TEST(StreamChannelTest, ConnectRefusedFails) {
    int port = findFreePort();
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.connect("127.0.0.1", port));
    EXPECT_EQ(channel.getState(), StreamState::Error);
}

TEST(StreamChannelTest, ListenInvalidPortFails) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.listen("127.0.0.1", 99999));
    EXPECT_EQ(channel.getState(), StreamState::Error);
}

TEST(StreamChannelTest, ConnectFailsWhenDescriptorsExhausted) {
#ifndef _WIN32
    // fd 穷尽时 ::socket() 返回 EMFILE，connect 应报错而非崩溃
    FdExhaustionGuard guard;
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.connect("127.0.0.1", 12345));
    EXPECT_EQ(channel.getState(), StreamState::Error);
#else
    GTEST_SKIP() << "fd exhaustion test is POSIX-only";
#endif
}

TEST(StreamChannelTest, ListenFailsWhenDescriptorsExhausted) {
#ifndef _WIN32
    FdExhaustionGuard guard;
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_FALSE(channel.listen("127.0.0.1", 12345));
    EXPECT_EQ(channel.getState(), StreamState::Error);
#else
    GTEST_SKIP() << "fd exhaustion test is POSIX-only";
#endif
}

TEST(StreamChannelTest, ListenWhenNotDisconnectedFails) {
    int port = findFreePort();
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    ASSERT_TRUE(channel.listen("127.0.0.1", port));
    // 已监听（Connected）时再次 listen 返回 false
    EXPECT_FALSE(channel.listen("127.0.0.1", port));
    channel.disconnect();
}

TEST(StreamChannelTest, AcceptWhenNotListeningReturnsNull) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_EQ(channel.accept(), nullptr);
}

TEST(StreamChannelTest, AcceptOnNonListeningSocketFails) {
    int port = findFreePort();
    StreamChannel server(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    StreamChannel client(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    ASSERT_TRUE(server.listen("127.0.0.1", port));
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    // client 处于 Connected 状态，但其 socket 并非监听 socket，
    // ::accept 应失败并返回 nullptr
    EXPECT_EQ(client.accept(), nullptr);

    client.disconnect();
    server.disconnect();
}

TEST(StreamChannelTest, SendWhenDisconnectedFails) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    const uint8_t data[] = {1, 2, 3};
    EXPECT_FALSE(channel.send(data, sizeof(data)));
    EXPECT_FALSE(channel.send(std::vector<uint8_t>{1, 2, 3}));
    EXPECT_FALSE(channel.send(std::string("abc")));
}

TEST(StreamChannelTest, StopReceivingWithoutStartIsSafe) {
    StreamChannel channel(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    EXPECT_NO_THROW(channel.stopReceiving());
}

class StreamChannelEnv : public ::testing::Test {
protected:
    void SetUp() override {
        port = findFreePort();

        server = std::make_unique<StreamChannel>(StreamType::CONTROL,
                                                 StreamParams::getDefault(StreamType::CONTROL));
        client = std::make_unique<StreamChannel>(StreamType::CONTROL,
                                                 StreamParams::getDefault(StreamType::CONTROL));
    }

    void TearDown() override {
        if (client) client->disconnect();
        if (server) server->disconnect();
    }

    // 建立服务器 + 客户端 + 已 accept 的服务端通道
    bool connectPair() {
        if (!server->listen("127.0.0.1", port)) return false;

        std::atomic<bool> accepted{false};
        std::thread acceptThread([&] {
            acceptedChannel = server->accept();
            accepted.store(true);
        });

        if (!client->connect("127.0.0.1", port)) {
            acceptThread.join();
            return false;
        }
        acceptThread.join();
        if (!accepted.load() || !acceptedChannel) return false;

        acceptedChannel->startReceiving([this](const uint8_t* data, size_t size) {
            serverSink.append(data, size);
        });
        client->startReceiving([this](const uint8_t* data, size_t size) {
            clientSink.append(data, size);
        });
        return acceptedChannel->isConnected() && client->isConnected();
    }

    int port = 0;
    std::unique_ptr<StreamChannel> server;
    std::unique_ptr<StreamChannel> client;
    std::unique_ptr<StreamChannel> acceptedChannel;
    DataSink serverSink;
    DataSink clientSink;
};

TEST_F(StreamChannelEnv, ListenConnectAccept) {
    ASSERT_TRUE(connectPair());
    EXPECT_EQ(server->getState(), StreamState::Connected);
    EXPECT_EQ(client->getState(), StreamState::Connected);
    EXPECT_EQ(acceptedChannel->getState(), StreamState::Connected);
    EXPECT_EQ(acceptedChannel->getType(), StreamType::CONTROL);
    EXPECT_NE(acceptedChannel->getRemoteEndpoint(), "");
    EXPECT_NE(acceptedChannel->getRemoteEndpoint().find("127.0.0.1"), std::string::npos);
    EXPECT_GT(client->getLocalPort(), 0);
    EXPECT_EQ(client->getRemoteEndpoint(), "127.0.0.1:" + std::to_string(port));
}

TEST_F(StreamChannelEnv, ClientToServerData) {
    ASSERT_TRUE(connectPair());

    const std::string payload = "hello stream";
    ASSERT_TRUE(client->send(payload));
    ASSERT_TRUE(serverSink.waitForSize(payload.size()));
    EXPECT_EQ(serverSink.data(), std::vector<uint8_t>(payload.begin(), payload.end()));
}

TEST_F(StreamChannelEnv, ServerToClientData) {
    ASSERT_TRUE(connectPair());

    const std::vector<uint8_t> payload = {0x00, 0x01, 0xFF, 0xFE};
    ASSERT_TRUE(acceptedChannel->send(payload));
    ASSERT_TRUE(clientSink.waitForSize(payload.size()));
    EXPECT_EQ(clientSink.data(), payload);
}

TEST_F(StreamChannelEnv, MultipleMessagesAreFramed) {
    ASSERT_TRUE(connectPair());

    // 多条消息：接收端应按消息边界回调
    std::mutex mutex;
    std::vector<std::vector<uint8_t>> messages;
    std::condition_variable cv;
    acceptedChannel->setDataCallback([&](const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex);
        messages.emplace_back(data, data + size);
        cv.notify_all();
    });

    for (int i = 0; i < 5; ++i) {
        client->send(std::string("msg") + std::to_string(i));
    }

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, 2s, [&] { return messages.size() >= 5; });
    ASSERT_EQ(messages.size(), 5u);
    EXPECT_EQ(messages[0], std::vector<uint8_t>({'m', 's', 'g', '0'}));
    EXPECT_EQ(messages[4], std::vector<uint8_t>({'m', 's', 'g', '4'}));
}

TEST_F(StreamChannelEnv, StartReceivingIdempotent) {
    ASSERT_TRUE(connectPair());

    size_t count = 0;
    std::mutex mutex;
    acceptedChannel->setDataCallback([&](const uint8_t*, size_t) {
        std::lock_guard<std::mutex> lock(mutex);
        ++count;
    });
    // 第二次 startReceiving 应该被忽略（否则回调被覆盖）
    acceptedChannel->startReceiving([](const uint8_t*, size_t) {});

    client->send("x");
    std::this_thread::sleep_for(300ms);
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(count, 1u);
}

TEST_F(StreamChannelEnv, DisconnectStopsReceiveLoop) {
    ASSERT_TRUE(connectPair());

    client->disconnect();
    EXPECT_EQ(client->getState(), StreamState::Disconnected);

    // 服务端接收循环因连接关闭而退出
    std::this_thread::sleep_for(200ms);
    acceptedChannel->disconnect();
    server->disconnect();
    EXPECT_EQ(acceptedChannel->getState(), StreamState::Disconnected);
}

TEST_F(StreamChannelEnv, ErrorCallbackOnPeerClose) {
    ASSERT_TRUE(connectPair());

    std::atomic<bool> errored{false};
    client->setErrorCallback([&](const std::error_code&) { errored = true; });

    acceptedChannel->disconnect();
    std::this_thread::sleep_for(300ms);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(StreamChannelEnv, SendAfterDisconnectFails) {
    ASSERT_TRUE(connectPair());
    client->disconnect();
    EXPECT_FALSE(client->send("nope"));
}

TEST_F(StreamChannelEnv, SendOversizedPayloadFails) {
    ASSERT_TRUE(connectPair());

    // 超过 SimpleMessage::MAX_MESSAGE_SIZE 的负载在组包阶段即被拒绝
    const std::vector<uint8_t> tooLarge(SimpleMessage::MAX_MESSAGE_SIZE + 1, 'L');
    EXPECT_FALSE(client->send(tooLarge.data(), tooLarge.size()));
    // 通道本身不受影响，仍可正常发送
    ASSERT_TRUE(client->send("still-alive"));
    ASSERT_TRUE(serverSink.waitForSize(10));
}

TEST_F(StreamChannelEnv, ScreenParamsSocketBuffersApplied) {
    port = findFreePort();

    auto screenServer = std::make_unique<StreamChannel>(StreamType::SCREEN,
                                                         StreamParams::getDefault(StreamType::SCREEN));
    auto screenClient = std::make_unique<StreamChannel>(StreamType::SCREEN,
                                                         StreamParams::getDefault(StreamType::SCREEN));
    ASSERT_TRUE(screenServer->listen("127.0.0.1", port));

    std::unique_ptr<StreamChannel> accepted;
    std::thread acceptThread([&] { accepted = screenServer->accept(); });
    ASSERT_TRUE(screenClient->connect("127.0.0.1", port));
    acceptThread.join();
    ASSERT_NE(accepted, nullptr);

    // SCREEN 默认参数带 sendBufferSize/recvBufferSize（256KB），
    // 连接时应应用 SO_SNDBUF/SO_RCVBUF 选项
    DataSink sink;
    accepted->startReceiving([&](const uint8_t* data, size_t size) { sink.append(data, size); });
    const std::string payload(2048, 'S');
    ASSERT_TRUE(screenClient->send(payload));
    ASSERT_TRUE(sink.waitForSize(payload.size()));
    EXPECT_EQ(sink.data(), std::vector<uint8_t>(payload.begin(), payload.end()));

    screenClient->disconnect();
    accepted->disconnect();
    screenServer->disconnect();
}

TEST_F(StreamChannelEnv, ReceiveErrorCallbackOnConnectionReset) {
    port = findFreePort();

    // 服务端使用原生 asio socket，便于设置 SO_LINGER 触发 RST
    asio::io_context io;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port);
    asio::ip::tcp::acceptor acceptor(io, endpoint);
    ASSERT_TRUE(client->connect("127.0.0.1", port));
    auto peer = acceptor.accept();

    std::atomic<bool> errored{false};
    client->startReceiving([](const uint8_t*, size_t) {},
                           [&](const std::error_code&) { errored = true; });

    // 先发送数据再以 RST 方式关闭：接收端先读到数据，随后 recv 报错
    std::string junk(64, 'J');
    asio::write(peer, asio::buffer(junk));
    std::this_thread::sleep_for(100ms);
    asio::error_code ec;
    peer.set_option(asio::socket_base::linger(true, 0), ec);
    peer.close(ec);

    ASSERT_TRUE([&] {
        for (int i = 0; i < 100; ++i) {
            if (errored.load()) return true;
            std::this_thread::sleep_for(20ms);
        }
        return errored.load();
    }());
    // 接收循环因错误退出后状态应变为 Error
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(client->getState(), StreamState::Error);
}

// ========== StreamChannelPair ==========

TEST(StreamChannelPairTest, PairConnectAndSend) {
    int requestPort = findFreePort();
    int responsePort = findFreePort();

    StreamChannel requestServer(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    StreamChannel responseServer(StreamType::EVENT, StreamParams::getDefault(StreamType::EVENT));
    ASSERT_TRUE(requestServer.listen("127.0.0.1", requestPort));
    ASSERT_TRUE(responseServer.listen("127.0.0.1", responsePort));

    std::unique_ptr<StreamChannel> acceptedRequest;
    std::unique_ptr<StreamChannel> acceptedResponse;
    std::thread t1([&] { acceptedRequest = requestServer.accept(); });
    std::thread t2([&] { acceptedResponse = responseServer.accept(); });

    StreamChannelPair pair(StreamParams::getDefault(StreamType::CONTROL),
                           StreamParams::getDefault(StreamType::EVENT));
    EXPECT_TRUE(pair.connect("127.0.0.1", requestPort, responsePort));
    EXPECT_TRUE(pair.isConnected());
    ASSERT_NE(pair.getRequestChannel(), nullptr);
    ASSERT_NE(pair.getResponseChannel(), nullptr);

    t1.join();
    t2.join();

    DataSink responseSink;
    acceptedResponse->startReceiving([&](const uint8_t* data, size_t size) {
        responseSink.append(data, size);
    });

    EXPECT_TRUE(pair.getRequestChannel()->send("req"));
    EXPECT_TRUE(pair.getResponseChannel()->send("resp"));
    ASSERT_TRUE(responseSink.waitForSize(4));
    EXPECT_EQ(responseSink.data(), std::vector<uint8_t>({'r', 'e', 's', 'p'}));

    pair.disconnect();
    EXPECT_FALSE(pair.isConnected());
    acceptedRequest->disconnect();
    acceptedResponse->disconnect();
    requestServer.disconnect();
    responseServer.disconnect();
}

TEST(StreamChannelPairTest, PairConnectSecondPortFailure) {
    int requestPort = findFreePort();
    int refusedPort = findFreePort();  // 无监听 -> 连接拒绝

    StreamChannel requestServer(StreamType::CONTROL, StreamParams::getDefault(StreamType::CONTROL));
    ASSERT_TRUE(requestServer.listen("127.0.0.1", requestPort));

    StreamChannelPair pair(StreamParams::getDefault(StreamType::CONTROL),
                           StreamParams::getDefault(StreamType::EVENT));
    // 第二个端口连接失败，整体失败且回滚第一个连接
    EXPECT_FALSE(pair.connect("127.0.0.1", requestPort, refusedPort));
    EXPECT_FALSE(pair.isConnected());

    requestServer.disconnect();
}

TEST(StreamChannelPairTest, PairConnectFirstPortFailure) {
    int refusedPort = findFreePort();  // 无监听 -> 连接拒绝
    int otherPort = findFreePort();

    StreamChannelPair pair(StreamParams::getDefault(StreamType::CONTROL),
                           StreamParams::getDefault(StreamType::EVENT));
    // 第一个端口连接失败：直接返回 false，不尝试第二个端口
    EXPECT_FALSE(pair.connect("127.0.0.1", refusedPort, otherPort));
    EXPECT_FALSE(pair.isConnected());
}
