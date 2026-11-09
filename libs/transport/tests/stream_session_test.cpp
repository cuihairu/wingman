#include "stream_transport_test_common.hpp"

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
