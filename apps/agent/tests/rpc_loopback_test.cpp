#include "rpc_ipc_test_common.hpp"


// LocalIpcServer 回环用的极简客户端：connect 成功后再启动接收线程
// （通道的 receiveLoop 以 isConnected 为循环条件，先收后连会立即退出）。
class TestIpcClient {
public:
    explicit TestIpcClient(const std::string& endpoint) : endpoint_(endpoint) {
        wingman::ipc::IpcConfig config;
        config.serverName = endpoint;
        channel_ = wingman::ipc::IpcFactory::createClient(config);
    }

    bool connect() {
        if (!channel_) {
            return false;
        }
        if (!channel_->connect(endpoint_)) {
            return false;
        }
        channel_->setMessageCallback([this](const wingman::ipc::IpcMessage& message) {
            if (message.type != wingman::ipc::IpcMessageType::Response) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                responses_.push_back(message);
            }
            cv_.notify_all();
        });
        channel_->startReceiving();
        return true;
    }

    uint64_t call(const std::string& method, const std::string& payload = "{}") {
        return channel_ ? channel_->sendRequest(method, payload) : 0;
    }

    bool send(const wingman::ipc::IpcMessage& message) {
        return channel_ && channel_->send(message);
    }

    std::optional<wingman::ipc::IpcMessage> awaitResponse(
        uint64_t id, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [&] {
                return std::any_of(responses_.begin(), responses_.end(),
                    [id](const auto& m) { return m.id == id; });
            })) {
            return std::nullopt;
        }
        for (auto it = responses_.begin(); it != responses_.end(); ++it) {
            if (it->id == id) {
                wingman::ipc::IpcMessage matched = *it;
                responses_.erase(it);
                return matched;
            }
        }
        return std::nullopt;
    }

    void disconnect() {
        if (channel_) {
            channel_->disconnect();
        }
    }

private:
    std::string endpoint_;
    std::unique_ptr<wingman::ipc::IIpcChannel> channel_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<wingman::ipc::IpcMessage> responses_;
};

// ========== LocalIpcServer 回环（Unix：Linux/macOS） ==========

#ifndef _WIN32

class LocalIpcServerLoopbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        clearEventBuffer();
        wingman::lua::registerLuaEngine();
        endpoint_ = tempPath("ipc");
        standalone_ = std::make_unique<StandaloneMode>(StandaloneModeConfig{});
    }

    void TearDown() override {
        client_.reset(); // 先客户端后服务端：模拟 GUI 先行退出的顺序
        server_.reset();
        standalone_.reset();
        std::error_code ec;
        fs::remove(endpoint_, ec);
    }

    void startServer() {
        server_ = std::make_unique<wingman::runtime::LocalIpcServer>(
            *standalone_, endpoint_, nullptr);

        // access/providers 的 std::function 只捕获 this，成员生命周期覆盖整测
        wingman::rpc::RemoteConfigAccess access;
        access.get = [this] { return remoteConfig_; };
        access.apply = [this](const json& request) -> std::string {
            remoteConfig_ = request.value("config", json::object());
            return {};
        };
        server_->setRemoteConfigAccess(access);

        wingman::rpc::RuntimeStatusProviders providers;
        providers.remoteConnected = [] { return true; };
        providers.remoteStateName = [] { return std::string("connected"); };
        server_->setStatusProviders(providers);

        ASSERT_TRUE(server_->start());
    }

    void connectClient() {
        client_ = std::make_unique<TestIpcClient>(endpoint_);
        ASSERT_TRUE(client_->connect());
    }

    std::optional<json> rpc(const std::string& method,
                            const json& params = json::object()) {
        const uint64_t id = client_->call(method, params.dump());
        EXPECT_NE(id, 0u);
        const auto message = client_->awaitResponse(id, std::chrono::seconds{5});
        if (!message) {
            return std::nullopt;
        }
        try {
            return json::parse(message->payload);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // 轮询等待出现指定 state 的 connection.ipc_client 事件
    bool waitForIpcConnectionEvent(const std::string& state) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
        while (std::chrono::steady_clock::now() < deadline) {
            for (const auto& event : drainEventBuffer()) {
                if (event.method == "connection.ipc_client" &&
                    event.payload.value("state", "") == state) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        }
        return false;
    }

    std::string endpoint_;
    json remoteConfig_ = json{{"serverIp", "10.0.0.1"}};
    std::unique_ptr<StandaloneMode> standalone_;
    std::unique_ptr<wingman::runtime::LocalIpcServer> server_;
    std::unique_ptr<TestIpcClient> client_;
};

TEST_F(LocalIpcServerLoopbackTest, StartStopLifecycleIsIdempotent) {
    startServer();
    EXPECT_TRUE(server_->start()); // 已运行时立即返回 true
    client_.reset();
    server_->stop();
    EXPECT_NO_THROW(server_->stop()); // 二次 stop 幂等
}

TEST_F(LocalIpcServerLoopbackTest, RoundTripSystemGetVersion) {
    startServer();
    connectClient();

    const auto response = rpc("system.getVersion");
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->at("type").get<std::string>(), "response");
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("result").at("server").get<std::string>(), "wingman");
}

TEST_F(LocalIpcServerLoopbackTest, GetStatusExposesInjectedProviders) {
    startServer();
    connectClient();

    const auto response = rpc("system.getStatus");
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    ASSERT_TRUE(data.at("success").get<bool>());
    EXPECT_TRUE(data.at("result").at("remoteConnected").get<bool>());
    EXPECT_EQ(data.at("result").at("remoteState").get<std::string>(), "connected");
}

TEST_F(LocalIpcServerLoopbackTest, UnknownMethodReturnsErrorEnvelope) {
    startServer();
    connectClient();

    const auto response = rpc("no.such.method");
    ASSERT_TRUE(response.has_value());
    const auto& data = response->at("data");
    EXPECT_FALSE(data.at("success").get<bool>());
    EXPECT_NE(data.at("error").get<std::string>().find("Unknown method"),
        std::string::npos);
}

TEST_F(LocalIpcServerLoopbackTest, MalformedPayloadReturnsParseErrorEnvelope) {
    startServer();
    connectClient();

    wingman::ipc::IpcMessage message;
    message.type = wingman::ipc::IpcMessageType::Request;
    message.method = "system.getVersion";
    message.payload = "{definitely-not-json";
    message.id = 77;
    ASSERT_TRUE(client_->send(message));

    const auto raw = client_->awaitResponse(77, std::chrono::seconds{5});
    ASSERT_TRUE(raw.has_value());
    const auto response = json::parse(raw->payload);
    const auto& data = response.at("data");
    EXPECT_FALSE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("error").get<std::string>().substr(0, 21),
        "Invalid params JSON: ");
}

TEST_F(LocalIpcServerLoopbackTest, MissingMethodIsRejected) {
    startServer();
    connectClient();

    wingman::ipc::IpcMessage message;
    message.type = wingman::ipc::IpcMessageType::Request;
    message.method = "";
    message.payload = "{}";
    message.id = 78;
    ASSERT_TRUE(client_->send(message));

    const auto raw = client_->awaitResponse(78, std::chrono::seconds{5});
    ASSERT_TRUE(raw.has_value());
    const auto response = json::parse(raw->payload);
    EXPECT_FALSE(response.at("data").at("success").get<bool>());
    EXPECT_EQ(response.at("data").at("error").get<std::string>(),
        "Missing IPC method");
}

TEST_F(LocalIpcServerLoopbackTest, ErrorTypeMessageProducesErrorEnvelope) {
    startServer();
    connectClient();

    wingman::ipc::IpcMessage message;
    message.type = wingman::ipc::IpcMessageType::Error;
    message.method = "";
    message.payload = R"({"error":"boom"})";
    message.id = 79;
    ASSERT_TRUE(client_->send(message));

    const auto raw = client_->awaitResponse(79, std::chrono::seconds{5});
    ASSERT_TRUE(raw.has_value());
    const auto response = json::parse(raw->payload);
    const auto& data = response.at("data");
    EXPECT_FALSE(data.at("success").get<bool>());
    EXPECT_EQ(data.at("error").get<std::string>(), "boom");
}

TEST_F(LocalIpcServerLoopbackTest, ConfigRoundTripThroughLoopback) {
    startServer();
    connectClient();

    const auto fetched = rpc("config.getRemote");
    ASSERT_TRUE(fetched.has_value());
    ASSERT_TRUE(fetched->at("data").at("success").get<bool>());
    EXPECT_EQ(fetched->at("data").at("result").at("serverIp").get<std::string>(),
        "10.0.0.1");

    const auto applied = rpc("config.setRemote",
        json{{"config", {{"serverIp", "9.9.9.9"}}}});
    ASSERT_TRUE(applied.has_value());
    ASSERT_TRUE(applied->at("data").at("success").get<bool>()) << applied->dump();
    EXPECT_EQ(applied->at("data").at("result").at("serverIp").get<std::string>(),
        "9.9.9.9");

    // 二次 dispatch 必须重新调用 get（handler 持有的是 access 拷贝而非配置快照）
    const auto refetched = rpc("config.getRemote");
    ASSERT_TRUE(refetched.has_value());
    EXPECT_EQ(refetched->at("data").at("result").at("serverIp").get<std::string>(),
        "9.9.9.9");
}

TEST_F(LocalIpcServerLoopbackTest, EventsDrainThroughLoopback) {
    startServer();
    connectClient();

    wingman::runtime::EventBuffer::instance().push("t.loopback", {{"k", true}});
    const auto response = rpc("events.drain", json{{"max", 10}});
    ASSERT_TRUE(response.has_value());
    const auto& result = response->at("data").at("result");
    bool found = false;
    for (const auto& event : result.at("events")) {
        if (event.at("method").get<std::string>() == "t.loopback") {
            found = true;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_EQ(result.at("remaining").get<std::size_t>(), 0u);
}

TEST_F(LocalIpcServerLoopbackTest, ClientDisconnectTriggersEventAndReconnect) {
    startServer();
    connectClient();

    const auto first = rpc("system.getVersion");
    ASSERT_TRUE(first.has_value());

    client_->disconnect();
    EXPECT_TRUE(waitForIpcConnectionEvent("disconnected"));

    connectClient(); // 服务端重听后第二个客户端可用
    const auto second = rpc("system.getVersion");
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(second->at("data").at("success").get<bool>());
}

TEST_F(LocalIpcServerLoopbackTest, StopWhileClientConnectedJoinsCleanly) {
    startServer();
    connectClient();

    const auto response = rpc("system.getVersion");
    ASSERT_TRUE(response.has_value());

    server_->stop(); // 客户端仍在线：服务端必须能收尾 join 而不悬挂
    client_->disconnect();
}

#endif // _WIN32
