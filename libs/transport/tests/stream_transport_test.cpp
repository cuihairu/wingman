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

#ifndef _WIN32
    #include <fcntl.h>
    #include <sys/resource.h>
#endif

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

#ifndef _WIN32
// 将 RLIMIT_NOFILE 软上限压到当前已打开的 fd 数量，使下一个 ::socket()
// 以 EMFILE 失败；析构时恢复原值。RLIMIT 是进程级设置，gtest 串行执行
// 下安全（各 fixture 的后台线程均已在 teardown 中 join）。
class FdExhaustionGuard {
public:
    FdExhaustionGuard() {
        ::getrlimit(RLIMIT_NOFILE, &original_);

        // 用 fcntl 而非 /proc/self/fd 统计：后者自身会打开新的 fd
        int openCount = 0;
        for (rlim_t fd = 0; fd < original_.rlim_cur; ++fd) {
            if (::fcntl(static_cast<int>(fd), F_GETFD) != -1) {
                ++openCount;
            }
        }

        rlimit exhausted{static_cast<rlim_t>(openCount), original_.rlim_max};
        ::setrlimit(RLIMIT_NOFILE, &exhausted);
    }

    ~FdExhaustionGuard() {
        ::setrlimit(RLIMIT_NOFILE, &original_);
    }

    FdExhaustionGuard(const FdExhaustionGuard&) = delete;
    FdExhaustionGuard& operator=(const FdExhaustionGuard&) = delete;

private:
    rlimit original_{};
};
#endif

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

// ========== Session 异常路径（原生 socket 对端） ==========

// 会话一端使用 Session + IO 线程，另一端使用原生 asio socket
// 便于注入畸形帧、截断数据与 RST
class RawPeerSessionEnv : public ::testing::Test {
protected:
    void SetUp() override {
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 0);
        asio::ip::tcp::acceptor acceptor(io_);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(asio::socket_base::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();

        peer_ = std::make_unique<asio::ip::tcp::socket>(peerIo_);
        std::thread connector([&] {
            asio::error_code ec;
            peer_->connect(acceptor.local_endpoint(), ec);
        });
        auto sessionSocket = acceptor.accept();
        connector.join();
        acceptor.close();

        session_ = Session::create(1, std::move(sessionSocket));
        work_.emplace(io_.get_executor());
        thread_ = std::thread([&] { io_.run(); });
    }

    void TearDown() override {
        if (session_) session_->close();
        io_.stop();
        if (thread_.joinable()) thread_.join();
        work_.reset();
        if (peer_) {
            asio::error_code ec;
            peer_->close(ec);
        }
    }

    void writeRaw(const uint8_t* data, size_t size) {
        asio::error_code ec;
        asio::write(*peer_, asio::buffer(data, size), ec);
    }

    void writeHeader(uint32_t length) {
        MessageHeader h{};
        h.length = length;
        h.type = MessageType::Notify;
        writeRaw(reinterpret_cast<const uint8_t*>(&h), sizeof(h));
    }

    static bool waitFor(const std::atomic<bool>& flag, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (flag.load()) return true;
            std::this_thread::sleep_for(10ms);
        }
        return flag.load();
    }

    asio::io_context io_;
    asio::io_context peerIo_;
    std::unique_ptr<asio::ip::tcp::socket> peer_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_;
    SessionPtr session_;
    std::thread thread_;
};

TEST_F(RawPeerSessionEnv, OversizedHeaderFiresErrorEvent) {
    std::atomic<bool> errored{false};
    session_->setEventCallback([&](SessionEvent event, const std::string&) {
        if (event == SessionEvent::Error) errored = true;
    });
    session_->startReceive();

    // 声明超过 16 MiB 上限的负载长度，读取端应立即报 message_size 错误
    writeHeader(17u * 1024 * 1024);

    ASSERT_TRUE(waitFor(errored, 2s));
    EXPECT_FALSE(session_->isConnected());
}

TEST_F(RawPeerSessionEnv, TruncatedBodyFiresErrorEvent) {
    std::atomic<bool> errored{false};
    session_->setEventCallback([&](SessionEvent event, const std::string&) {
        if (event == SessionEvent::Error) errored = true;
    });
    session_->startReceive();

    // 声明 8 字节负载，仅发送 3 字节后正常关闭（负载体阶段 EOF）
    writeHeader(8);
    writeRaw(reinterpret_cast<const uint8_t*>("abc"), 3);
    asio::error_code ec;
    peer_->close(ec);

    ASSERT_TRUE(waitFor(errored, 2s));
    EXPECT_FALSE(session_->isConnected());
}

TEST_F(RawPeerSessionEnv, WriteFailureFiresErrorEvent) {
    std::atomic<bool> errored{false};
    session_->setEventCallback([&](SessionEvent event, const std::string&) {
        if (event == SessionEvent::Error) errored = true;
    });

    // 对端以 RST 方式关闭，随后的异步写入失败
    asio::error_code ec;
    peer_->set_option(asio::socket_base::linger(true, 0), ec);
    peer_->close(ec);

    const auto payload = std::string(64, 'W');
    for (int i = 0; i < 200 && !errored.load(); ++i) {
        session_->send(Message::create(MessageType::Notify, payload));
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_TRUE(errored.load());
    EXPECT_FALSE(session_->isConnected());
}

TEST_F(RawPeerSessionEnv, SendQueueLimitReached) {
    // 对端保持连接但从不读取：内核缓冲区填满后，发送队列达到
    // kMaxSendQueueSize 上限，send 应开始返回 false
    const auto payload = std::string(64 * 1024, 'Q');
    bool queueFull = false;
    for (int i = 0; i < 1100; ++i) {
        if (!session_->send(Message::create(MessageType::Notify, payload))) {
            queueFull = true;
            break;
        }
    }
    EXPECT_TRUE(queueFull);
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

TEST_F(TransportEnv, ClientConnectInvalidAddressFails) {
    // make_address 对非法 IP 抛出异常，connect 应捕获并返回 false
    EXPECT_FALSE(client->connect("999.999.999.999", 12345));
    EXPECT_FALSE(client->isConnected());
}

TEST_F(TransportEnv, ClientSessionAccessorsAndBroadcast) {
    ASSERT_TRUE(startServerAndWaitClient());

    // 已连接状态下会话访问器
    auto ids = client->getSessionIds();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], 0u);
    EXPECT_NE(client->getSession(ids[0]), nullptr);

    // 客户端广播等价于向唯一会话发送
    auto msg = Message::create(MessageType::Notify, "client-broadcast");
    client->broadcast(msg);
    ASSERT_TRUE(waitServerMessages(1));
    EXPECT_EQ(serverMessages[0]->body, "client-broadcast");

    // closeSession 应断开连接
    client->closeSession(ids[0]);
    EXPECT_FALSE(client->isConnected());
    EXPECT_EQ(client->getSessionCount(), 0u);
}

TEST(TransportFactoryTest, WebSocketTypeUnsupported) {
    EXPECT_EQ(TransportClient::create(TransportType::WebSocket), nullptr);
    EXPECT_EQ(TransportServer::create(TransportType::WebSocket), nullptr);
}

TEST_F(TransportEnv, BroadcastWithoutSessionsIsSafe) {
    ASSERT_TRUE(server->listen("127.0.0.1", port));
    ASSERT_TRUE(server->start());
    EXPECT_NO_THROW(server->broadcast(Message::create(MessageType::Notify, "x")));
}
