#include "agentcore_test_common.hpp"

// ========== RemoteClient（loopback TcpServer） ==========

TEST_F(RemoteClientTest, StartAgainstUnreachablePortStartsBackgroundReconnect) {
    // 占住端口再释放：保证「连接被拒」而非未路由等待
    {
        asio::io_context io;
        asio::ip::tcp::acceptor a(io);
        a.open(asio::ip::tcp::v4());
        a.bind(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
        a.close();
    }

    std::vector<ConnectionState> states;
    RemoteClient client(makeConfig());
    client.setEventCallback([&](const ConnectionEvent& e) {
        states.push_back(e.state);
    });

    EXPECT_FALSE(client.start());
    EXPECT_TRUE(client.isRunning());
    EXPECT_FALSE(client.isConnected());

    // 后台重连循环会发出 Error/Reconnecting 事件
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline && states.empty()) {
        std::this_thread::sleep_for(10ms);
    }
    EXPECT_FALSE(states.empty());

    client.stop();
    EXPECT_FALSE(client.isRunning());
}

TEST_F(RemoteClientTest, StopWithoutStartIsSafe) {
    RemoteClient client(makeConfig());
    client.stop();
    EXPECT_FALSE(client.isRunning());
    EXPECT_FALSE(client.isConnected());
}

TEST_F(RemoteClientTest, RegisterCarriesIdentityMetadataAndToken) {
    ASSERT_TRUE(startServer());
    RemoteClient client(makeConfig());
    client.setIdentity("agent-x", "host-x");
    client.setRegisterMetadata(nlohmann::json{{"platform", "unittest"}});
    client.setAuthToken("tok-1");
    ASSERT_TRUE(client.start());
    EXPECT_TRUE(client.isRunning());
    ASSERT_TRUE(waitServerMessages(1));

    auto registerMsg = nlohmann::json::parse(serverMessages_[0]->body);
    EXPECT_EQ(registerMsg["type"], "agent.register");
    EXPECT_EQ(registerMsg["agentId"], "agent-x");
    EXPECT_EQ(registerMsg["hostname"], "host-x");
    EXPECT_EQ(registerMsg["platform"], "unittest");
    EXPECT_EQ(registerMsg["token"], "tok-1");

    client.stop();
}

TEST_F(RemoteClientTest, RegisterAckSuccessReportsRegisteredAndPushesEvent) {
    ASSERT_TRUE(connectAndRegister(true, "srv-42"));
    ASSERT_NE(client_, nullptr);

    EXPECT_TRUE(waitClientState(*client_, "connected"));

    // 注册确认推入 EventBuffer（GUI 连接指示器数据源）
    bool sawConnected = false;
    for (const auto& evt : EventBuffer::instance().drain(EventBuffer::kMaxEvents)) {
        if (evt.method == "connection.state_changed" && evt.payload["state"] == "connected") {
            sawConnected = true;
        }
    }
    EXPECT_TRUE(sawConnected);

    client_->stop();
}

TEST_F(RemoteClientTest, RegisterAckFailureReportsErrorState) {
    ASSERT_TRUE(connectAndRegister(false));
    ASSERT_NE(client_, nullptr);

    EXPECT_TRUE(waitClientState(*client_, "error"));
    client_->stop();
}

TEST_F(RemoteClientTest, RegisterAckMalformedJsonDoesNotCrash) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    auto msg = Message::create(MessageType::Notify, "not-json-at-all");
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], msg));
    std::this_thread::sleep_for(200ms);

    // 解析失败被吞掉，连接本身不受影响
    EXPECT_TRUE(client_->isConnected());
    client_->stop();
}

TEST_F(RemoteClientTest, SendNotifyDeliversTypedFrameWithMergedFields) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    // 团队协同桥出站面：type + 逐键合并字段，Notify 帧（server 侧 sendNotify 对称面）
    client_->sendNotify("team.join", {
        {"teamId", "team_1"},
        {"memberId", "m-1"},
    });
    ASSERT_TRUE(waitServerMessages(2));
    const MessagePtr& sent = serverMessages_[1];
    EXPECT_EQ(sent->header.type, MessageType::Notify);
    const nlohmann::json body = nlohmann::json::parse(sent->body);
    EXPECT_EQ(body.value("type", ""), "team.join");
    EXPECT_EQ(body.value("teamId", ""), "team_1");
    EXPECT_EQ(body.value("memberId", ""), "m-1");
    client_->stop();
}

TEST_F(RemoteClientTest, NotifyHandlerReceivesUnhandledTypes) {
    ASSERT_TRUE(startServer());
    client_ = std::make_unique<RemoteClient>(makeConfig());

    // 内建类型（agent.register_ack / proxy.*）之外全部交宿主装配层分派
    std::mutex cbMutex;
    std::condition_variable cbCv;
    std::vector<std::pair<std::string, nlohmann::json>> received;
    client_->setNotifyHandler([&](const std::string& type, const nlohmann::json& body) {
        std::lock_guard<std::mutex> lock(cbMutex);
        received.emplace_back(type, body);
        cbCv.notify_all();
    });
    ASSERT_TRUE(client_->start());
    ASSERT_TRUE(waitServerMessages(1));

    nlohmann::json frame = {
        {"type", "inbox.message"},
        {"msgId", "msg_1"},
        {"messageType", "team.joined"},
        {"payload", {{"type", "team.joined"}, {"teamId", "team_1"}}},
        {"timestamp", 1715299200000},
    };
    auto msg = Message::create(MessageType::Notify, frame.dump());
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], msg));
    std::unique_lock<std::mutex> lock(cbMutex);
    ASSERT_TRUE(cbCv.wait_for(lock, 3000ms, [&] { return received.size() >= 1; }));
    EXPECT_EQ(received[0].first, "inbox.message");
    EXPECT_EQ(received[0].second.value("msgId", ""), "msg_1");
    EXPECT_EQ(received[0].second.value("messageType", ""), "team.joined");
    client_->stop();
}

TEST_F(RemoteClientTest, DoubleStartReturnsTrueWithoutRestart) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    EXPECT_TRUE(client_->start()); // 已运行：直接返回 true
    EXPECT_TRUE(client_->isRunning());
    client_->stop();
}

TEST_F(RemoteClientTest, CommandRequestRoundTripReturnsCallbackResult) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    client_->setCommandCallback([](const std::string& method, const CommandData&) {
        EXPECT_EQ(method, "test.echo");
        return CommandResult::okData(R"({"k":"v"})", "done");
    });

    auto request = Message::create(MessageType::Request, R"({"type":"test.echo","arg":"x"})");
    request->header.sequence = 7;
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], request));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr response = [this]() -> MessagePtr {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type == MessageType::Response) return msg;
        }
        return nullptr;
    }();
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->header.sequence, 7u);

    auto body = nlohmann::json::parse(response->body);
    EXPECT_TRUE(body["success"].get<bool>());
    EXPECT_EQ(body["method"], "test.echo");
    EXPECT_EQ(body["message"], "done");
    EXPECT_EQ(body["data"]["k"], "v");
    client_->stop();
}

TEST_F(RemoteClientTest, CommandRequestWithoutCallbackGetsErrorResponse) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    auto request = Message::create(MessageType::Request, R"({"type":"no.handler"})");
    request->header.sequence = 9;
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], request));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr response = [this]() -> MessagePtr {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type == MessageType::Response) return msg;
        }
        return nullptr;
    }();
    ASSERT_NE(response, nullptr);
    auto body = nlohmann::json::parse(response->body);
    EXPECT_FALSE(body["success"].get<bool>());
    EXPECT_EQ(body["error"], "command callback not bound");
    client_->stop();
}

TEST_F(RemoteClientTest, CommandCallbackFailureCarriesErrorMessage) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    client_->setCommandCallback([](const std::string&, const CommandData&) {
        return CommandResult::error("boom");
    });

    auto request = Message::create(MessageType::Request, R"({"type":"failing"})");
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], request));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr response = [this]() -> MessagePtr {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type == MessageType::Response) return msg;
        }
        return nullptr;
    }();
    ASSERT_NE(response, nullptr);
    auto body = nlohmann::json::parse(response->body);
    EXPECT_FALSE(body["success"].get<bool>());
    EXPECT_EQ(body["error"], "boom");
    client_->stop();
}

TEST_F(RemoteClientTest, CommandDataInvalidJsonFallsBackToString) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    client_->setCommandCallback([](const std::string&, const CommandData&) {
        return CommandResult::okData("raw-not-json");
    });

    auto request = Message::create(MessageType::Request, R"({"type":"raw.data"})");
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], request));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr response = [this]() -> MessagePtr {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type == MessageType::Response) return msg;
        }
        return nullptr;
    }();
    ASSERT_NE(response, nullptr);
    auto body = nlohmann::json::parse(response->body);
    EXPECT_TRUE(body["success"].get<bool>());
    EXPECT_EQ(body["data"], "raw-not-json");
    client_->stop();
}

TEST_F(RemoteClientTest, MalformedRequestBodyGetsErrorResponse) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    client_->setCommandCallback([](const std::string&, const CommandData&) {
        return CommandResult::ok();
    });

    auto request = Message::create(MessageType::Request, "{{{not-json");
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], request));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr response = [this]() -> MessagePtr {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type == MessageType::Response) return msg;
        }
        return nullptr;
    }();
    ASSERT_NE(response, nullptr);
    auto body = nlohmann::json::parse(response->body);
    EXPECT_FALSE(body["success"].get<bool>());
    EXPECT_TRUE(body.contains("error"));
    client_->stop();
}

TEST_F(RemoteClientTest, HeartbeatCarriesLinkStats) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    // makeConfig 心跳 30s 不适用本用例：重启一个 1s 心跳的客户端
    client_->stop();
    serverMessages_.clear();
    client_ = std::make_unique<RemoteClient>(makeConfig(/*heartbeatSeconds=*/1));
    ASSERT_TRUE(client_->start());
    ASSERT_TRUE(waitServerMessages(1)); // register

    // 等第一条 agent.heartbeat（窗口 5s）
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    nlohmann::json heartbeat;
    bool found = false;
    while (std::chrono::steady_clock::now() < deadline && !found) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& msg : serverMessages_) {
                if (msg->header.type != MessageType::Notify) continue;
                auto body = nlohmann::json::parse(msg->body);
                if (body.value("type", "") == "agent.heartbeat") {
                    heartbeat = body;
                    found = true;
                    break;
                }
            }
        }
        if (!found) std::this_thread::sleep_for(50ms);
    }
    ASSERT_TRUE(found) << "no agent.heartbeat within 5s";

    EXPECT_EQ(heartbeat["status"], "online");
    ASSERT_TRUE(heartbeat.contains("link"));
    EXPECT_EQ(heartbeat["link"]["reconnects"], 0); // 初始连接不计重连
    EXPECT_TRUE(heartbeat["link"].contains("dropped"));
    EXPECT_TRUE(heartbeat["link"].contains("outboxPending"));
    EXPECT_TRUE(heartbeat["link"].contains("lastDisconnectReason"));
    EXPECT_GE(heartbeat["link"]["sessionUptimeMs"].get<int64_t>(), 0);
    client_->stop();
}

TEST_F(RemoteClientTest, SendAgentEventDeliveredImmediatelyWhenConnected) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);

    client_->sendAgentEvent("trigger.fired", {{"trigger", "t1"}});
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr event = [this]() -> MessagePtr {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& msg : serverMessages_) {
            if (msg->header.type != MessageType::Notify) continue;
            auto body = nlohmann::json::parse(msg->body);
            if (body.value("type", "") == "agent.event") return msg;
        }
        return nullptr;
    }();
    ASSERT_NE(event, nullptr);
    auto body = nlohmann::json::parse(event->body);
    EXPECT_EQ(body["event"], "trigger.fired");
    EXPECT_EQ(body["data"]["trigger"], "t1");
    client_->stop();
}

TEST_F(RemoteClientTest, DisconnectedEventsQueueAndFlushOnReconnect) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);
    ASSERT_GE(server_->getSessionCount(), 1u);

    // 服务端主动断开 → 客户端进入重连循环（interval=1s）
    server_->closeSession(server_->getSessionIds()[0]);
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline && client_->isConnected()) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_FALSE(client_->isConnected());

    // 断线期间发送的 agent.event 应入 outbox
    client_->sendAgentEvent("queued.one", {{"i", 1}});
    client_->sendAgentEvent("queued.two", {{"i", 2}});
    client_->sendAgentEvent("queued.three", {{"i", 3}});

    // 重连成功后冲刷：服务端应收到新 register + 3 条 queued 事件
    std::size_t queuedReceived = 0;
    const auto flushDeadline = std::chrono::steady_clock::now() + 15s;
    while (std::chrono::steady_clock::now() < flushDeadline && queuedReceived < 3) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& msg : serverMessages_) {
                if (msg->header.type != MessageType::Notify) continue;
                auto body = nlohmann::json::parse(msg->body);
                if (body.value("type", "") == "agent.event" &&
                    body.value("event", "").rfind("queued.", 0) == 0) {
                    ++queuedReceived;
                }
            }
        }
        if (queuedReceived < 3) std::this_thread::sleep_for(50ms);
    }
    EXPECT_EQ(queuedReceived, 3u);
    client_->stop();
}

TEST_F(RemoteClientTest, OutboxCapDropsExcessBeyondLimit) {
    ASSERT_TRUE(connectAndRegister(true));
    ASSERT_NE(client_, nullptr);
    ASSERT_GE(server_->getSessionCount(), 1u);

    server_->closeSession(server_->getSessionIds()[0]);
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline && client_->isConnected()) {
        std::this_thread::sleep_for(10ms);
    }
    ASSERT_FALSE(client_->isConnected());

    // 超过 outbox 容量（kMaxOutbox=100）10 条：重连后只应冲刷出 100 条
    for (int i = 0; i < 110; ++i) {
        client_->sendAgentEvent("bulk.event", {{"i", i}});
    }

    std::size_t bulkReceived = 0;
    const auto flushDeadline = std::chrono::steady_clock::now() + 20s;
    while (std::chrono::steady_clock::now() < flushDeadline && bulkReceived < 100) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            bulkReceived = 0;
            for (const auto& msg : serverMessages_) {
                if (msg->header.type != MessageType::Notify) continue;
                auto body = nlohmann::json::parse(msg->body);
                if (body.value("type", "") == "agent.event" &&
                    body.value("event", "") == "bulk.event") {
                    ++bulkReceived;
                }
            }
        }
        if (bulkReceived < 100) std::this_thread::sleep_for(50ms);
    }
    EXPECT_EQ(bulkReceived, 100u);

    // 静默窗口后不应再变多：溢出的 10 条已被丢弃而非排队
    std::this_thread::sleep_for(500ms);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t total = 0;
        for (const auto& msg : serverMessages_) {
            if (msg->header.type != MessageType::Notify) continue;
            auto body = nlohmann::json::parse(msg->body);
            if (body.value("type", "") == "agent.event" &&
                body.value("event", "") == "bulk.event") {
                ++total;
            }
        }
        EXPECT_EQ(total, 100u);
    }
    client_->stop();
}

TEST_F(RemoteClientTest, ConfigRoundTrip) {
    RemoteClient client(makeConfig());
    EXPECT_EQ(client.getConfig().serverIp, "127.0.0.1");
    EXPECT_EQ(client.getConfig().serverPort, port_);
    EXPECT_EQ(client.getConfig().heartbeatInterval, 30);
    EXPECT_EQ(RemoteClient::stateName(ConnectionState::Connected), "connected");
    EXPECT_EQ(RemoteClient::stateName(ConnectionState::Connecting), "connecting");
    EXPECT_EQ(RemoteClient::stateName(ConnectionState::Reconnecting), "reconnecting");
    EXPECT_EQ(RemoteClient::stateName(ConnectionState::Disconnected), "disconnected");
    EXPECT_EQ(RemoteClient::stateName(ConnectionState::Error), "error");
}

