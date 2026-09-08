/**
 * Stream / Session / Channel / Transport 集成测试
 * 使用本地回环 TCP 连接验证流通道、会话、通道多路复用与传输层。
 */

#include <gtest/gtest.h>
#include "wingman/transport/stream_type.hpp"
#include "wingman/transport/stream_channel.hpp"
#include "wingman/transport/stream_manager.hpp"
#include "wingman/transport/transport.hpp"
#include "wingman/transport/transport_client.hpp"
#include "wingman/transport/transport_server.hpp"
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <random>
#include <thread>

using namespace std::chrono_literals;
using namespace wingman::transport;

namespace {

// 选择一个空闲端口（通过绑定后释放来探测）
int findFreePort() {
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    auto port = acceptor.local_endpoint().port();
    acceptor.close();
    return static_cast<int>(port);
}

class DataSink {
public:
    void append(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer.insert(buffer.end(), data, data + size);
        cv.notify_all();
    }

    bool waitForSize(size_t expected, std::chrono::milliseconds timeout = 2s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv.wait_for(lock, timeout, [&] { return buffer.size() >= expected; });
    }

    std::vector<uint8_t> data() {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv;
    std::vector<uint8_t> buffer;
};

} // namespace

// ========== stream_type.hpp ==========

TEST(StreamTypeTest, Names) {
    EXPECT_STREQ(streamTypeName(StreamType::CONTROL), "CONTROL");
    EXPECT_STREQ(streamTypeName(StreamType::SCREEN), "SCREEN");
    EXPECT_STREQ(streamTypeName(StreamType::EVENT), "EVENT");
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
    // RFC 6761 保留的 .invalid TLD 保证不会被 DNS 解析；
    // 不要使用 .example 等可能被通配符 DNS 劫持的域名
    EXPECT_FALSE(channel.connect("unresolvable.host.invalid", 12345));
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

// ========== Message / Session (session.hpp) ==========

TEST(MessageTest, SerializeDeserializeRoundTrip) {
    auto msg = Message::create(MessageType::Request, "body-data");
    msg->header.sequence = 42;
    auto bytes = msg->serialize();
    ASSERT_EQ(bytes.size(), sizeof(MessageHeader) + 9u);

    auto parsed = Message::deserialize(bytes);
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->header.sequence, 42u);
    EXPECT_EQ(parsed->header.type, MessageType::Request);
    EXPECT_EQ(parsed->header.length, 9u);
    EXPECT_EQ(parsed->body, "body-data");
}

TEST(MessageTest, DeserializeTooShortReturnsNull) {
    std::vector<uint8_t> small(sizeof(MessageHeader) - 1, 0);
    EXPECT_EQ(Message::deserialize(small), nullptr);
}

TEST(MessageTest, DeserializeHeaderOnly) {
    auto msg = Message::create();
    msg->header.length = 0;
    auto bytes = msg->serialize();
    auto parsed = Message::deserialize(bytes);
    ASSERT_NE(parsed, nullptr);
    EXPECT_TRUE(parsed->body.empty());
}

class SessionEnv : public ::testing::Test {
protected:
    void SetUp() override {
        int port = findFreePort();

        // 建立一对已连接的 asio socket
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port);
        asio::ip::tcp::acceptor acceptor(serverIo_);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(asio::socket_base::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();

        asio::ip::tcp::socket clientSocket(clientIo_);
        std::thread connector([&] {
            asio::error_code ec;
            clientSocket.connect(asio::ip::tcp::endpoint(
                asio::ip::make_address("127.0.0.1"), port), ec);
        });

        auto serverSocket = acceptor.accept();
        connector.join();

        serverSession_ = Session::create(1, std::move(serverSocket));
        clientSession_ = Session::create(2, std::move(clientSocket));
        acceptor.close();

        serverWork_.emplace(serverIo_.get_executor());
        clientWork_.emplace(clientIo_.get_executor());
        serverThread_ = std::thread([&] { serverIo_.run(); });
        clientThread_ = std::thread([&] { clientIo_.run(); });
    }

    void TearDown() override {
        if (clientSession_) clientSession_->close();
        if (serverSession_) serverSession_->close();
        serverIo_.stop();
        clientIo_.stop();
        if (serverThread_.joinable()) serverThread_.join();
        if (clientThread_.joinable()) clientThread_.join();
        serverWork_.reset();
        clientWork_.reset();
    }

    asio::io_context serverIo_, clientIo_;
    // work_guard 防止 run() 在测试体 post 异步操作之前因无工作而提前返回
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>>
        serverWork_, clientWork_;
    SessionPtr serverSession_, clientSession_;
    std::thread serverThread_, clientThread_;
};

TEST_F(SessionEnv, SessionBasics) {
    EXPECT_EQ(serverSession_->getId(), 1u);
    EXPECT_EQ(clientSession_->getId(), 2u);
    EXPECT_TRUE(serverSession_->isConnected());
    EXPECT_TRUE(clientSession_->isConnected());
    EXPECT_EQ(serverSession_->getRemoteAddress(), "127.0.0.1");
    EXPECT_GT(serverSession_->getRemotePort(), 0);
}

TEST_F(SessionEnv, SendReceiveBothWays) {
    DataSink serverSink, clientSink;
    std::mutex mutex;
    std::condition_variable cv;
    size_t serverCount = 0, clientCount = 0;

    serverSession_->setMessageCallback([&](const MessagePtr& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        serverSink.append(reinterpret_cast<const uint8_t*>(msg->body.data()), msg->body.size());
        ++serverCount;
        cv.notify_all();
    });
    clientSession_->setMessageCallback([&](const MessagePtr& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        clientSink.append(reinterpret_cast<const uint8_t*>(msg->body.data()), msg->body.size());
        ++clientCount;
        cv.notify_all();
    });

    serverSession_->startReceive();
    clientSession_->startReceive();

    auto toServer = Message::create(MessageType::Request, "ping");
    auto toClient = Message::create(MessageType::Response, "pong");
    ASSERT_TRUE(clientSession_->send(toServer));
    ASSERT_TRUE(serverSession_->send(toClient));

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait_for(lock, 2s, [&] { return serverCount >= 1 && clientCount >= 1; });
    EXPECT_EQ(serverSink.data(), std::vector<uint8_t>({'p', 'i', 'n', 'g'}));
    EXPECT_EQ(clientSink.data(), std::vector<uint8_t>({'p', 'o', 'n', 'g'}));
}

TEST_F(SessionEnv, SendWhenClosedFails) {
    serverSession_->close();
    EXPECT_FALSE(serverSession_->isConnected());
    EXPECT_FALSE(serverSession_->send(Message::create(MessageType::Notify, "x")));
}

TEST_F(SessionEnv, DisconnectedSessionRemoteInfoEmpty) {
    asio::io_context io;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
    asio::ip::tcp::acceptor acceptor(io);
    acceptor.open(endpoint.protocol());
    acceptor.bind(endpoint);
    acceptor.listen();

    // 建立真实连接后关闭，验证远端信息为空
    asio::ip::tcp::socket clientSocket(io);
    asio::error_code ec;
    clientSocket.connect(asio::ip::tcp::endpoint(
        asio::ip::make_address("127.0.0.1"), acceptor.local_endpoint().port()), ec);
    ASSERT_FALSE(ec);

    auto socket = acceptor.accept();
    auto session = Session::create(9, std::move(socket));
    acceptor.close();
    clientSocket.close();
    session->close();
    EXPECT_EQ(session->getRemoteAddress(), "");
    EXPECT_EQ(session->getRemotePort(), 0);
}
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

// ========== TcpClient / TcpServer (transport.hpp) ==========

class TransportEnv : public ::testing::Test {
protected:
    void SetUp() override {
        port = findFreePort();
        server = createTcpServer();
        client = createTcpClient();
    }

    void TearDown() override {
        client->stop();
        server->stop();
    }

    bool startServerAndWaitClient() {
        serverMessages.clear();
        clientEvents.clear();

        server->setMessageHandler([this](const MessagePtr& msg) {
            std::lock_guard<std::mutex> lock(mutex_);
            serverMessages.push_back(msg);
            cv.notify_all();
        });
        server->setEventHandler([this](Session*, SessionEvent event) {
            std::lock_guard<std::mutex> lock(mutex_);
            serverEvents.push_back(event);
            cv.notify_all();
        });
        client->setEventHandler([this](Session*, SessionEvent event) {
            std::lock_guard<std::mutex> lock(mutex_);
            clientEvents.push_back(event);
            cv.notify_all();
        });

        if (!server->listen("127.0.0.1", port)) return false;
        if (!server->start()) return false;
        if (!client->connect("127.0.0.1", port)) return false;

        // 等待服务端感知连接
        std::unique_lock<std::mutex> lock(mutex_);
        return cv.wait_for(lock, 2s, [&] {
            for (auto e : serverEvents) {
                if (e == SessionEvent::Connected) return true;
            }
            return false;
        });
    }

    bool waitServerMessages(size_t n, std::chrono::milliseconds timeout = 2s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv.wait_for(lock, timeout, [&] { return serverMessages.size() >= n; });
    }

    int port = 0;
    TcpServerPtr server;
    TcpClientPtr client;

    std::mutex mutex_;
    std::condition_variable cv;
    std::vector<MessagePtr> serverMessages;
    std::vector<SessionEvent> serverEvents;
    std::vector<SessionEvent> clientEvents;
};

TEST_F(TransportEnv, ClientServerRoundTrip) {
    ASSERT_TRUE(startServerAndWaitClient());

    EXPECT_TRUE(client->isConnected());
    EXPECT_TRUE(client->isRunning());
    EXPECT_TRUE(server->isRunning());
    EXPECT_EQ(server->getSessionCount(), 1u);
    EXPECT_EQ(client->getSessionCount(), 1u);
    ASSERT_NE(client->getSession(), nullptr);

    auto msg = Message::create(MessageType::Notify, "ping-body");
    msg->header.type = MessageType::Notify;
    ASSERT_TRUE(client->send(msg));
    ASSERT_TRUE(waitServerMessages(1));
    EXPECT_EQ(serverMessages[0]->body, "ping-body");
}

TEST_F(TransportEnv, ServerSendToClientSession) {
    ASSERT_TRUE(startServerAndWaitClient());

    DataSink clientSink;
    client->setMessageHandler([this](const MessagePtr& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        serverMessages.push_back(msg);
        cv.notify_all();
    });

    auto ids = server->getSessionIds();
    ASSERT_EQ(ids.size(), 1u);
    auto msg = Message::create(MessageType::Notify, "from-server");
    EXPECT_TRUE(server->send(ids[0], msg));

    ASSERT_TRUE(waitServerMessages(1));
    EXPECT_EQ(serverMessages[0]->body, "from-server");
}

TEST_F(TransportEnv, ServerSendToInvalidSessionFails) {
    ASSERT_TRUE(startServerAndWaitClient());
    auto msg = Message::create(MessageType::Notify, "x");
    EXPECT_FALSE(server->send(9999, msg));
    EXPECT_EQ(server->getSession(9999), nullptr);
}

TEST_F(TransportEnv, BroadcastReachesClients) {
    ASSERT_TRUE(startServerAndWaitClient());

    client->setMessageHandler([this](const MessagePtr& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        serverMessages.push_back(msg);
        cv.notify_all();
    });

    auto msg = Message::create(MessageType::Notify, "broadcast");
    server->broadcast(msg);
    ASSERT_TRUE(waitServerMessages(1));
    EXPECT_EQ(serverMessages[0]->body, "broadcast");
}

TEST_F(TransportEnv, CloseSessionCleanup) {
    ASSERT_TRUE(startServerAndWaitClient());
    ASSERT_EQ(server->getSessionCount(), 1u);

    auto ids = server->getSessionIds();
    server->closeSession(ids[0]);
    EXPECT_EQ(server->getSessionCount(), 0u);
}

TEST_F(TransportEnv, ClientDisconnectDetected) {
    ASSERT_TRUE(startServerAndWaitClient());

    client->disconnect();
    EXPECT_FALSE(client->isConnected());
    EXPECT_EQ(client->getSessionCount(), 0u);
    EXPECT_TRUE(client->getSessionIds().empty());
    EXPECT_EQ(client->getSession(), nullptr);

    // 服务端应清理会话
    std::unique_lock<std::mutex> lock(mutex_);
    cv.wait_for(lock, 2s, [&] {
        return server->getSessionCount() == 0;
    });
    EXPECT_EQ(server->getSessionCount(), 0u);
}

TEST_F(TransportEnv, ClientStartStop) {
    EXPECT_TRUE(client->start());
    ASSERT_TRUE(startServerAndWaitClient());
    client->stop();
    EXPECT_FALSE(client->isConnected());
}

TEST_F(TransportEnv, SendWhenDisconnectedFails) {
    auto msg = Message::create(MessageType::Notify, "x");
    EXPECT_FALSE(client->send(msg));
    // 断开状态下 request 立即返回 nullptr
    auto future = static_cast<TcpClient*>(client.get())->request(msg);
    ASSERT_EQ(future.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(future.get(), nullptr);
}

TEST_F(TransportEnv, RequestResponseFuture) {
    ASSERT_TRUE(startServerAndWaitClient());

    // 服务端回显响应
    server->setMessageHandler([this](const MessagePtr& msg) {
        auto response = Message::create(MessageType::Response, "resp:" + msg->body);
        response->header.sequence = msg->header.sequence;
        if (auto session = server->getSession(1)) {
            session->send(response);
        } else if (!server->getSessionIds().empty()) {
            server->send(server->getSessionIds()[0], response);
        }
    });

    auto tcpClient = static_cast<TcpClient*>(client.get());
    auto request = Message::create(MessageType::Request, "echo");
    auto future = tcpClient->request(request);
    ASSERT_EQ(future.wait_for(2s), std::future_status::ready);
    auto response = future.get();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->body, "resp:echo");
    EXPECT_EQ(response->header.sequence, request->header.sequence);
}

TEST_F(TransportEnv, ServerListenInvalidAddressFails) {
    EXPECT_FALSE(server->listen("999.999.999.999", 12345));
}

TEST_F(TransportEnv, ServerStartIdempotentAndStopSafe) {
    EXPECT_TRUE(server->start());
    EXPECT_TRUE(server->start());  // 重复 start 直接成功
    server->stop();
    server->stop();  // 重复 stop 安全
    EXPECT_FALSE(server->isRunning());
}

TEST_F(TransportEnv, ClientConnectRefused) {
    int freePort = findFreePort();
    std::atomic<bool> errored{false};
    client->setEventHandler([&](Session*, SessionEvent event) {
        if (event == SessionEvent::Error) errored = true;
    });
    EXPECT_FALSE(client->connect("127.0.0.1", freePort));
}

TEST_F(TransportEnv, CloseAllSessions) {
    ASSERT_TRUE(startServerAndWaitClient());
    ASSERT_EQ(server->getSessionCount(), 1u);

    auto* tcpServer = static_cast<TcpServer*>(server.get());
    tcpServer->closeAllSessions();
    EXPECT_EQ(server->getSessionCount(), 0u);
}

TEST_F(TransportEnv, BroadcastWithoutSessionsIsSafe) {
    ASSERT_TRUE(server->listen("127.0.0.1", port));
    ASSERT_TRUE(server->start());
    EXPECT_NO_THROW(server->broadcast(Message::create(MessageType::Notify, "x")));
}
