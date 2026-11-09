#include "stream_transport_test_common.hpp"

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
