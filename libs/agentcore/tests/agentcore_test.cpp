// wingman::agentcore 单元测试
//
// 覆盖两块此前 0% 行覆盖的核心：
//   - EventBuffer   本地 UI 事件缓冲（容量驱逐 / sink 转发 / drain pull 模型）
//   - RemoteClient  出站远程链路（注册 / 心跳 / 命令分发 / 断线 outbox / 重连）
//
// RemoteClient 经 loopback TcpServer 真连真收发（同 libs/transport 的
// TransportEnv 模式）；全部等待都有界，整体 suite 无外部依赖、headless 可跑。

#include <gtest/gtest.h>

#include "proxy_tunnel.hpp"
#include "wingman/agentcore/event_buffer.hpp"
#include "wingman/agentcore/hmac_sha256.hpp"
#include "wingman/agentcore/remote_client.hpp"
#include "wingman/agentcore/remote_client_config.hpp"
#include "wingman/transport/transport.hpp"

#include <nlohmann/json.hpp>
#include <asio.hpp>

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <optional>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace wingman::runtime;
using namespace wingman::transport;
using namespace std::chrono_literals;

namespace {

// 测试内联 hex 小写（独立参考实现，核对 hmac_sha256 的向量一致性）
std::string testHexEncode(const std::string& in) {
    static const char* tab = "0123456789abcdef";
    std::string out;
    out.reserve(in.size() * 2);
    for (unsigned char c : in) {
        out += tab[c >> 4];
        out += tab[c & 0x0f];
    }
    return out;
}

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

} // namespace

// ========== EventBuffer ==========

// 单例跨用例共享：每个用例前后清空并摘除 sink；
// dropped 计数器不清零，断言一律用「用例内增量」。
class EventBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& buffer = EventBuffer::instance();
        buffer.setRemoteSink(nullptr);
        buffer.clear();
        droppedBase_ = buffer.dropped();
    }
    void TearDown() override {
        auto& buffer = EventBuffer::instance();
        buffer.setRemoteSink(nullptr);
        buffer.clear();
    }

    std::size_t droppedBase_ = 0;
};

TEST_F(EventBufferTest, PushDrainRoundTripPreservesOrderAndFields) {
    auto& buffer = EventBuffer::instance();

    buffer.push("log.line", {{"text", "one"}});
    buffer.push("trigger.fired", {{"id", 7}});
    buffer.push("screenshot.frame", {{"w", 1280}});

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_EQ(buffer.dropped(), droppedBase_);

    auto events = buffer.drain(10);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].method, "log.line");
    EXPECT_EQ(events[0].payload, nlohmann::json({{"text", "one"}}));
    EXPECT_EQ(events[1].method, "trigger.fired");
    EXPECT_EQ(events[2].method, "screenshot.frame");
    for (const auto& evt : events) {
        EXPECT_GT(evt.timestamp, 0u);
    }
    // 时间戳毫秒级且不回退
    EXPECT_LE(events[0].timestamp, events[2].timestamp);

    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.drain(10).empty());
}

TEST_F(EventBufferTest, DrainRespectsMaxAndLeavesRest) {
    auto& buffer = EventBuffer::instance();
    for (int i = 0; i < 5; ++i) {
        buffer.push("m", {{"i", i}});
    }

    auto first = buffer.drain(2);
    ASSERT_EQ(first.size(), 2u);
    EXPECT_EQ(first[0].payload["i"], 0);
    EXPECT_EQ(first[1].payload["i"], 1);
    EXPECT_EQ(buffer.size(), 3u);

    EXPECT_TRUE(buffer.drain(0).empty());
    EXPECT_EQ(buffer.size(), 3u);

    auto rest = buffer.drain(10);
    ASSERT_EQ(rest.size(), 3u);
    EXPECT_EQ(rest[0].payload["i"], 2);
}

TEST_F(EventBufferTest, DrainEmptyReturnsEmpty) {
    EXPECT_TRUE(EventBuffer::instance().drain(5).empty());
}

TEST_F(EventBufferTest, ClearEmptiesBuffer) {
    auto& buffer = EventBuffer::instance();
    buffer.push("a", {});
    buffer.push("b", {});
    EXPECT_EQ(buffer.size(), 2u);

    buffer.clear();
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.drain(5).empty());
}

TEST_F(EventBufferTest, OverflowDropsOldestWhenNoLogLine) {
    auto& buffer = EventBuffer::instance();
    for (std::size_t i = 0; i < EventBuffer::kMaxEvents + 3; ++i) {
        buffer.push("keep.a", {{"i", i}});
    }

    EXPECT_EQ(buffer.size(), EventBuffer::kMaxEvents);
    EXPECT_EQ(buffer.dropped(), droppedBase_ + 3);

    // 无 log.line 时按 FIFO 逐出：最早的 3 条被丢，队首是第 4 条
    auto front = buffer.drain(1);
    ASSERT_EQ(front.size(), 1u);
    EXPECT_EQ(front[0].payload["i"], 3);
}

TEST_F(EventBufferTest, OverflowPrefersEvictingLogLine) {
    auto& buffer = EventBuffer::instance();
    for (int i = 0; i < 997; ++i) {
        buffer.push("keep.a", {{"i", i}});
    }
    for (int i = 0; i < 3; ++i) {
        buffer.push("log.line", {{"i", 1000 + i}});
    }
    ASSERT_EQ(buffer.size(), EventBuffer::kMaxEvents);

    // 再进一条非 log 事件触发容量超限：优先逐出 log.line 而非最旧的 keep.a
    buffer.push("keep.a", {{"i", 9999}});
    EXPECT_EQ(buffer.size(), EventBuffer::kMaxEvents);
    EXPECT_EQ(buffer.dropped(), droppedBase_ + 1);

    int keep = 0, logs = 0;
    bool oldestKept = true;
    for (const auto& evt : buffer.drain(EventBuffer::kMaxEvents)) {
        if (evt.method == "keep.a") {
            ++keep;
            if (evt.payload["i"] == 0) oldestKept = true;
        } else {
            ++logs;
        }
    }
    EXPECT_EQ(keep, 998);
    EXPECT_EQ(logs, 2);
    EXPECT_TRUE(oldestKept);
}

TEST_F(EventBufferTest, RemoteSinkReceivesEachPushedEvent) {
    auto& buffer = EventBuffer::instance();

    std::string sinkMethod;
    nlohmann::json sinkPayload;
    buffer.setRemoteSink([&](const std::string& method, const nlohmann::json& payload) {
        sinkMethod = method;
        sinkPayload = payload;
    });

    buffer.push("trigger.fired", {{"id", 42}});
    EXPECT_EQ(sinkMethod, "trigger.fired");
    EXPECT_EQ(sinkPayload, nlohmann::json({{"id", 42}}));

    // sink 转发不影响本地缓冲
    EXPECT_EQ(buffer.size(), 1u);
}

TEST_F(EventBufferTest, SinkMayQueryBufferWithoutDeadlock) {
    auto& buffer = EventBuffer::instance();

    // sink 在锁外回调：回调里允许再查 size / 再 push（重入），不应死锁
    buffer.setRemoteSink([&buffer](const std::string&, const nlohmann::json&) {
        (void)buffer.size();
    });
    buffer.push("m", {});

    int depth = 0;
    buffer.setRemoteSink([&](const std::string& method, const nlohmann::json&) {
        if (method == "outer" && depth == 0) {
            depth = 1;
            EventBuffer::instance().push("inner", {});
        }
    });
    buffer.push("outer", {});

    EXPECT_EQ(depth, 1);
    EXPECT_EQ(buffer.size(), 3u); // outer + inner（两次 push 各缓冲一条）+ 首条 "m"
}

TEST_F(EventBufferTest, NullSinkStillBuffers) {
    auto& buffer = EventBuffer::instance();
    buffer.setRemoteSink(nullptr);
    buffer.push("m", {{"x", 1}});
    EXPECT_EQ(buffer.size(), 1u);
}

TEST_F(EventBufferTest, IpcEventToJsonHasThreeFields) {
    const IpcEvent evt{"log.line", {{"text", "hi"}}, 12345u};
    const auto json = evt.toJson();
    ASSERT_TRUE(json.is_object());
    EXPECT_EQ(json.size(), 3u);
    EXPECT_EQ(json["method"], "log.line");
    EXPECT_EQ(json["payload"], nlohmann::json({{"text", "hi"}}));
    EXPECT_EQ(json["timestamp"], 12345u);
}

TEST_F(EventBufferTest, InstanceReturnsSameObject) {
    EXPECT_EQ(&EventBuffer::instance(), &EventBuffer::instance());
}

// ========== RemoteClient（loopback TcpServer） ==========

class RemoteClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        port_ = findFreePort();
        server_ = createTcpServer();
        server_->setMessageHandler([this](const MessagePtr& msg) {
            std::lock_guard<std::mutex> lock(mutex_);
            serverMessages_.push_back(msg);
            cv_.notify_all();
        });
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
    }

    void TearDown() override {
        client_.reset();
        server_->stop();
        EventBuffer::instance().setRemoteSink(nullptr);
        EventBuffer::instance().clear();
    }

    RemoteClientConfig makeConfig(int heartbeatSeconds = 30) {
        RemoteClientConfig config;
        config.serverIp = "127.0.0.1";
        config.serverPort = port_;
        config.heartbeatInterval = heartbeatSeconds;
        config.reconnectInterval = 1;
        config.maxReconnectInterval = 1;
        return config;
    }

    bool startServer() {
        if (!server_->listen("127.0.0.1", port_)) return false;
        return server_->start();
    }

    bool waitServerMessages(std::size_t n, std::chrono::milliseconds timeout = 3000ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return serverMessages_.size() >= n; });
    }

    // 启动服务端与客户端，等 agent.register 到达并回注册确认（可选失败确认）。
    bool connectAndRegister(bool ackSuccess = true, const std::string& ackId = "srv-42") {
        if (!startServer()) return false;
        client_ = std::make_unique<RemoteClient>(makeConfig());
        if (!client_->start()) return false;
        if (!waitServerMessages(1)) return false;
        nlohmann::json ack = {
            {"type", "agent.register_ack"},
            {"success", ackSuccess},
            {"agentId", ackId},
        };
        auto msg = Message::create(MessageType::Notify, ack.dump());
        return server_->send(server_->getSessionIds()[0], msg);
    }

    bool waitClientState(RemoteClient& client, const std::string& stateName,
                         std::chrono::milliseconds timeout = 3000ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (client.connectionStateName() == stateName) return true;
            std::this_thread::sleep_for(10ms);
        }
        return client.connectionStateName() == stateName;
    }

    int port_ = 0;
    TcpServerPtr server_;
    std::unique_ptr<RemoteClient> client_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<MessagePtr> serverMessages_;
};

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

// ========== challenge-response（A3-P2 §6.1，hmac_sha256 + RemoteClient） ==========

TEST(HmacSha256Test, Sha256KnownVectors) {
    // FIPS 180 标准向量
    EXPECT_EQ(sha256Hex(""),
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256Hex("abc"),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // 跨块长度（> 64 字节）验证多块压缩与填充
    EXPECT_EQ(sha256Hex(std::string(200, 'x')),
        "aa20c23e3201834050679e1d88941b9a6fed0557c9a705cb2c315e2e63fd486d");
}

TEST(HmacSha256Test, HmacRfc4231Vectors) {
    // RFC 4231 Test Case 1/2（key/data 原始字节；本层返回原始 32 字节，经
    // 内联 hex 参考实现转 hex 后比对）
    EXPECT_EQ(testHexEncode(hmacSha256(std::string(20, '\x0b'), "Hi There")),
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    EXPECT_EQ(testHexEncode(hmacSha256("Jefe", "what do ya want for nothing?")),
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    // key 超过块长（64 字节）走 key 先哈希分支（RFC 4231 Test Case 7）
    EXPECT_EQ(testHexEncode(hmacSha256(std::string(131, '\xaa'),
                      "Test Using Larger Than Block-Size Key - Hash Key First")),
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
}

TEST(HmacSha256Test, HmacHexKeyMatchesServerTokenMac) {
    // 与 Go server tokenMAC 字节语义对齐：key/message 均为 hex 串的 ASCII 字节。
    // 向量独立预计算：key = sha256Hex("tok-1")，message = nonce。
    const std::string key = "65dcf16ea3dfa49069628089eb4a75483070f5584b2a21ee64912b5f621f12da";
    const std::string nonce = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
    EXPECT_EQ(hmacSha256HexKey(key, nonce),
        "aa1b9280bd896237986b9cc680446c0833f6cedeaffe1b2e47b0bcd337f1f4d6");
    // 与「先 sha256Hex(token) 再 HMAC」的组合用法一致
    EXPECT_EQ(hmacSha256HexKey(sha256Hex("tok-1"), nonce),
        "aa1b9280bd896237986b9cc680446c0833f6cedeaffe1b2e47b0bcd337f1f4d6");
}

TEST_F(RemoteClientTest, RegisterChallengeModeCarriesFlagNotToken) {
    ASSERT_TRUE(startServer());
    auto config = makeConfig();
    config.useChallengeAuth = true;
    RemoteClient client(config);
    client.setIdentity("agent-ch", "host-ch");
    client.setAuthToken("tok-1");
    ASSERT_TRUE(client.start());
    ASSERT_TRUE(waitServerMessages(1));

    auto registerMsg = nlohmann::json::parse(serverMessages_[0]->body);
    EXPECT_EQ(registerMsg["challenge"], true);
    EXPECT_FALSE(registerMsg.contains("token"));

    client.stop();
}

TEST_F(RemoteClientTest, RegisterDefaultModeNeverCarriesChallengeFlag) {
    ASSERT_TRUE(startServer());
    RemoteClient client(makeConfig());
    client.setAuthToken("tok-1");
    ASSERT_TRUE(client.start());
    ASSERT_TRUE(waitServerMessages(1));

    auto registerMsg = nlohmann::json::parse(serverMessages_[0]->body);
    EXPECT_EQ(registerMsg["token"], "tok-1");
    EXPECT_FALSE(registerMsg.contains("challenge"));

    client.stop();
}

TEST_F(RemoteClientTest, AuthChallengeRequestAnsweredWithHmac) {
    ASSERT_TRUE(startServer());
    auto config = makeConfig();
    config.useChallengeAuth = true;
    client_ = std::make_unique<RemoteClient>(config);
    client_->setAuthToken("tok-1");
    ASSERT_TRUE(client_->start());
    ASSERT_TRUE(waitServerMessages(1)); // agent.register

    // server 下发 auth.challenge Request；client 应回 Response（同 seq）携 hmac
    const std::string nonce = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
    auto req = Message::create(MessageType::Request,
        nlohmann::json{{"type", "auth.challenge"}, {"nonce", nonce}}.dump());
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], req));
    ASSERT_TRUE(waitServerMessages(2));

    const MessagePtr& resp = serverMessages_[1];
    EXPECT_EQ(resp->header.type, MessageType::Response);
    auto body = nlohmann::json::parse(resp->body);
    EXPECT_EQ(body["hmac"],
        "aa1b9280bd896237986b9cc680446c0833f6cedeaffe1b2e47b0bcd337f1f4d6");

    client_->stop();
}

TEST_F(RemoteClientTest, AuthChallengeWithoutTokenAnswersEmptyHmac) {
    ASSERT_TRUE(startServer());
    auto config = makeConfig();
    config.useChallengeAuth = true;
    client_ = std::make_unique<RemoteClient>(config);
    // 不设 token
    ASSERT_TRUE(client_->start());
    ASSERT_TRUE(waitServerMessages(1));

    const std::string nonce = "aabbccdd";
    auto req = Message::create(MessageType::Request,
        nlohmann::json{{"type", "auth.challenge"}, {"nonce", nonce}}.dump());
    ASSERT_TRUE(server_->send(server_->getSessionIds()[0], req));
    ASSERT_TRUE(waitServerMessages(2));

    auto body = nlohmann::json::parse(serverMessages_[1]->body);
    EXPECT_EQ(body["hmac"], "");

    client_->stop();
}

// ========== ProxyTunnel ==========

// 代理数据面（proxy.* Notify 的裸 TCP 拨号分支）回环真连测试：
// 测试侧 acceptor 扮演 target，收集器扮演 server 侧中继。全部等待有界。

namespace {

// 测试内联 base64（独立参考实现，顺带核对内建实现的向量一致性）
std::string testBase64Encode(const std::string& in) {
    static const char* tab =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::uint32_t buf = 0;
    int bits = 0;
    for (unsigned char c : in) {
        buf = (buf << 8) | c;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += tab[(buf >> bits) & 0x3F];
        }
    }
    if (bits > 0) out += tab[(buf << (6 - bits)) & 0x3F];
    out += std::string((4 - out.size() % 4) % 4, '=');
    return out;
}

// 收集 Tunnel 发出的 proxy.* 帧；waitFor 轮询等待命中谓词的帧。
class SendCollector {
public:
    bool send(const std::string& type, const nlohmann::json& fields) {
        std::lock_guard<std::mutex> lock(mu_);
        frames_.emplace_back(type, fields);
        return true;
    }

    // 返回命中的首个帧（type, fields），无命中返回 { "", null }。
    std::pair<std::string, nlohmann::json> waitFor(
        const std::function<bool(const std::string&, const nlohmann::json&)>& hit,
        std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            {
                std::lock_guard<std::mutex> lock(mu_);
                for (const auto& [type, fields] : frames_) {
                    if (hit(type, fields)) return {type, fields};
                }
            }
            if (std::chrono::steady_clock::now() > deadline) {
                return {"", nullptr};
            }
            std::this_thread::sleep_for(10ms);
        }
    }

private:
    std::mutex mu_;
    std::vector<std::pair<std::string, nlohmann::json>> frames_;
};

// target 端 acceptor：接受 Tunnel 的拨入并持有连接。
// 成员声明顺序即初始化顺序：io_ 必须先于 acceptor_（在其上开监听）。
class TargetServer {
public:
    TargetServer() {
        asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
        acceptor_.open(ep.protocol());
        acceptor_.bind(ep);
        acceptor_.listen();
        port_ = acceptor_.local_endpoint().port();
        acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket sock) {
            if (!ec) {
                sock_ = std::move(sock);
                connected_.store(true);
            }
        });
        thread_ = std::thread([this] { io_.run(); });
    }

    ~TargetServer() {
        asio::error_code ignored;
        acceptor_.close(ignored);
        if (sock_) {
            sock_->close(ignored);
        }
        if (thread_.joinable()) thread_.join();
    }

    uint16_t port() const { return port_; }
    bool waitConnected(std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!connected_.load()) {
            if (std::chrono::steady_clock::now() > deadline) return false;
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }

    // 关闭 target 侧连接（模拟目标服务断开 → Tunnel 读泵 EOF）
    void closePeer() {
        asio::error_code ignored;
        if (sock_) sock_->close(ignored);
        closed_ = true;
    }

    // target → tunnel 方向写（Tunnel 读泵收进 proxy.data）
    bool writeSome(const std::string& data) {
        if (!sock_ || closed_) return false;
        asio::error_code ec;
        asio::write(*sock_, asio::buffer(data), ec);
        return !ec;
    }

    // tunnel → target 方向读（handleData 的写入落点）
    std::string readSome(std::chrono::milliseconds timeout = 5000ms) {
        if (!sock_ || closed_) return "";
        asio::error_code ec;
        sock_->non_blocking(true, ec);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::string got;
        char buf[4096];
        while (std::chrono::steady_clock::now() < deadline) {
            std::size_t n = sock_->read_some(asio::buffer(buf), ec);
            if (n > 0) {
                got.append(buf, n);
                break;
            }
            if (ec && ec != asio::error::would_block) break;
            std::this_thread::sleep_for(10ms);
        }
        sock_->non_blocking(false, ec);
        return got;
    }

    // 对端读 EOF/错误（Tunnel 关闭后成立）
    bool waitEof(std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            if (closed_) return true;
            if (!sock_) return false;
            asio::error_code ec;
            sock_->non_blocking(true, ec);
            char buf[16];
            std::size_t n = sock_->read_some(asio::buffer(buf), ec);
            if (n == 0 && ec == asio::error::eof) return true;
            if (ec && ec != asio::error::would_block) return true; // reset/bad_descriptor 皆算收口
            if (std::chrono::steady_clock::now() > deadline) return false;
            std::this_thread::sleep_for(10ms);
        }
    }

private:
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_{io_};
    std::optional<asio::ip::tcp::socket> sock_;
    std::thread thread_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> closed_{false};
    uint16_t port_ = 0;
};

} // namespace

TEST(ProxyTunnelTest, LoopbackRoundTripBothDirections) {
    TargetServer target;
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-1"},
        {"connId", "guac:sess-1-1"},
        {"target", "127.0.0.1:" + std::to_string(target.port())},
    });
    ASSERT_TRUE(target.waitConnected());

    // target → tunnel 读泵 → proxy.data（base64）
    const std::string payload = "hello-tunnel-payload";
    ASSERT_TRUE(target.writeSome(payload));
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.data";
    });
    EXPECT_EQ(type, "proxy.data");
    if (type == "proxy.data") {
        EXPECT_EQ(fields.value("proxyId", ""), "guac:sess-1");
        EXPECT_EQ(fields.value("connId", ""), "guac:sess-1-1");
        EXPECT_EQ(fields.value("data", ""), testBase64Encode(payload));
    }

    // server → tunnel handleData → 落到 target socket
    tunnel->handleNotify("proxy.data", {
        {"proxyId", "guac:sess-1"},
        {"connId", "guac:sess-1-1"},
        {"data", testBase64Encode("reply-frame")},
    });
    EXPECT_EQ(target.readSome(), "reply-frame");

    // server 侧关闭 → tunnel 收口（对端读到 EOF），不回发
    tunnel->handleNotify("proxy.close", {
        {"proxyId", "guac:sess-1"},
        {"connId", "guac:sess-1-1"},
        {"reason", "test"},
    });
    EXPECT_TRUE(target.waitEof());
    tunnel->stop();
}

TEST(ProxyTunnelTest, TargetEofNotifiesServerSideClose) {
    TargetServer target;
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-2"},
        {"connId", "guac:sess-2-1"},
        {"target", "127.0.0.1:" + std::to_string(target.port())},
    });
    ASSERT_TRUE(target.waitConnected());
    // target 主动断开（模拟目标服务关闭）→ tunnel 读泵 EOF → proxy.close 回发
    {
        asio::error_code ignored;
        target.closePeer();
    }
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.close";
    });
    EXPECT_EQ(type, "proxy.close");
    if (type == "proxy.close") {
        EXPECT_EQ(fields.value("connId", ""), "guac:sess-2-1");
        EXPECT_FALSE(fields.value("reason", "").empty());
    }
    tunnel->stop();
}

TEST(ProxyTunnelTest, DialFailureReportsProxyError) {
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-3"},
        {"connId", "guac:sess-3-1"},
        {"target", "127.0.0.1:1"}, // 回环保留端口，必然拒绝
    });
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.error";
    });
    EXPECT_EQ(type, "proxy.error");
    if (type == "proxy.error") {
        EXPECT_EQ(fields.value("connId", ""), "guac:sess-3-1");
        EXPECT_FALSE(fields.value("error", "").empty());
    }

    // 对已失败连接的 proxy.data 静默丢弃，不崩
    tunnel->handleNotify("proxy.data", {
        {"proxyId", "guac:sess-3"},
        {"connId", "guac:sess-3-1"},
        {"data", testBase64Encode("stray")},
    });
    tunnel->stop();
}

TEST(ProxyTunnelTest, StopSendsProxyErrorForLateNew) {
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });
    tunnel->stop();
    tunnel->stop(); // 幂等

    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-4"},
        {"connId", "guac:sess-4-1"},
        {"target", "127.0.0.1:1"},
    });
    auto [type, fields] = collector.waitFor([](const std::string& t, const nlohmann::json&) {
        return t == "proxy.error";
    });
    EXPECT_EQ(type, "proxy.error");
    if (type == "proxy.error") {
        EXPECT_EQ(fields.value("error", ""), "shutting down");
    }
}

TEST(ProxyTunnelTest, BadTargetAndBadBase64AreRejectedGracefully) {
    SendCollector collector;
    auto tunnel = std::make_shared<ProxyTunnel>(
        [&collector](const std::string& type, const nlohmann::json& fields) {
            return collector.send(type, fields);
        });

    // 缺字段
    tunnel->handleNotify("proxy.new", {{"proxyId", "guac:x"}});
    // 非法 target
    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-5"}, {"connId", "guac:sess-5-1"}, {"target", "no-port"},
    });
    // 非法 base64
    tunnel->handleNotify("proxy.new", {
        {"proxyId", "guac:sess-6"}, {"connId", "guac:sess-6-1"}, {"target", "127.0.0.1:1"},
    });
    tunnel->handleNotify("proxy.data", {
        {"proxyId", "guac:sess-6"}, {"connId", "guac:sess-6-1"}, {"data", "!!not-base64!!"},
    });
    // 未知 type 忽略；proxy.error 是 runtime→server 方向，收到即忽略
    tunnel->handleNotify("proxy.error", {{"proxyId", "guac:sess-6"}, {"connId", "guac:sess-6-1"}});

    // 无拨号成功的连接、无崩溃即为通过（有界收尾）
    tunnel->stop();
}
