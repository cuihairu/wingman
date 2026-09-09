// Unix domain socket 通道测试：依赖 POSIX socket API（sys/un.h、::getpid 等），
// 仅在非 Windows 平台编译。CMake 侧同样按 NOT WIN32 排除，此处守卫为双保险。
#if !defined(_WIN32)

#include <gtest/gtest.h>
#include "wingman/ipc/unix_socket_channel.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace wingman::ipc;

namespace {

std::string makeSocketPath() {
    static std::atomic<uint64_t> counter{0};
    const char* tmpdir = std::getenv("TMPDIR");
    std::string base = (tmpdir && *tmpdir) ? tmpdir : "/tmp";
    return base + "/wingman_unix_test_" + std::to_string(::getpid()) + "_"
         + std::to_string(++counter) + ".sock";
}

class MessageCollector {
public:
    void record(const IpcMessage& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        messages.push_back(msg);
        cv_.notify_all();
    }

    bool waitFor(size_t expected, std::chrono::milliseconds timeout = 2s) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return messages.size() >= expected; });
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages.size();
    }

    IpcMessage at(size_t idx) {
        std::lock_guard<std::mutex> lock(mutex_);
        return messages.at(idx);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        messages.clear();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<IpcMessage> messages;
};

} // namespace

// ========== 构造与状态 ==========

TEST(UnixSocketChannelTest, ConstructServerMode) {
    UnixSocketChannel channel(true, "/tmp/test_server.sock");
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
    EXPECT_EQ(channel.getTransport(), IpcTransport::UnixSocket);
    EXPECT_EQ(channel.getBackendName(), "UnixSocket");
    EXPECT_EQ(channel.getEndpoint(), "/tmp/test_server.sock");
}

TEST(UnixSocketChannelTest, ConstructClientMode) {
    UnixSocketChannel channel(false, "/tmp/test_client.sock");
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getBackendName(), "UnixSocket");
}

TEST(UnixSocketChannelTest, DisconnectWhenDisconnectedIsNoOp) {
    UnixSocketChannel channel(false, "/tmp/nowhere.sock");
    EXPECT_NO_THROW(channel.disconnect());
    EXPECT_EQ(channel.getState(), IpcState::Disconnected);
}

TEST(UnixSocketChannelTest, SendWhenDisconnectedReturnsFalse) {
    UnixSocketChannel channel(false, "/tmp/nowhere.sock");
    IpcMessage msg;
    msg.type = IpcMessageType::Request;
    msg.method = "test";
    EXPECT_FALSE(channel.send(msg));
    EXPECT_EQ(channel.sendRequest("method", "{}"), 0u);
    EXPECT_FALSE(channel.sendEvent("event", "{}"));
}

TEST(UnixSocketChannelTest, ClientConnectToMissingSocketFails) {
    UnixSocketChannel channel(false, "/tmp/wingman_missing_" + std::to_string(::getpid()) + ".sock");
    EXPECT_FALSE(channel.connect(""));
    EXPECT_EQ(channel.getState(), IpcState::Error);
}

TEST(UnixSocketChannelTest, ClientConnectOverridesEndpoint) {
    std::string path = makeSocketPath();
    UnixSocketChannel server(true, path);
    ASSERT_TRUE(server.connect(""));

    UnixSocketChannel client(false, "/tmp/somewhere_else.sock");
    // 通过 endpoint 参数覆盖路径
    EXPECT_TRUE(client.connect(path));
    EXPECT_EQ(client.getEndpoint(), path);

    client.disconnect();
    server.disconnect();
}

TEST(UnixSocketChannelTest, ServerBindFailureSetsError) {
    // 绑定到一个位于不存在目录里的路径 -> bind 失败
    UnixSocketChannel server(true, "/tmp/wingman_no_such_dir_12345/x.sock");
    EXPECT_FALSE(server.connect(""));
    EXPECT_EQ(server.getState(), IpcState::Error);
}

TEST(UnixSocketChannelTest, ServerConnectWhenAlreadyConnectedReturnsTrue) {
    std::string path = makeSocketPath();
    UnixSocketChannel server(true, path);
    ASSERT_TRUE(server.connect(""));
    // 已连接时再次 connect 直接返回 true
    EXPECT_TRUE(server.connect(""));
    server.disconnect();
}

// ========== 双向通信 ==========

class UnixSocketChannelTestEnv : public ::testing::Test {
protected:
    void SetUp() override {
        path = makeSocketPath();
        server = std::make_unique<UnixSocketChannel>(true, path);
        client = std::make_unique<UnixSocketChannel>(false, path);

        serverCollector = std::make_unique<MessageCollector>();
        clientCollector = std::make_unique<MessageCollector>();

        server->setMessageCallback([this](const IpcMessage& msg) {
            serverCollector->record(msg);
        });
        client->setMessageCallback([this](const IpcMessage& msg) {
            clientCollector->record(msg);
        });
    }

    void TearDown() override {
        if (client) client->disconnect();
        if (server) server->disconnect();
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }

    bool connectPair() {
        std::atomic<bool> serverOk{false};
        std::thread serverThread([&] {
            serverOk = server->connect("");
        });
        // 等服务器监听起来
        std::this_thread::sleep_for(50ms);
        if (!client->connect("")) {
            serverThread.join();
            return false;
        }
        serverThread.join();
        if (!serverOk.load()) return false;

        server->startReceiving();  // 触发 accept
        client->startReceiving();
        std::this_thread::sleep_for(100ms);
        return true;
    }

    std::string path;
    std::unique_ptr<UnixSocketChannel> server;
    std::unique_ptr<UnixSocketChannel> client;
    std::unique_ptr<MessageCollector> serverCollector;
    std::unique_ptr<MessageCollector> clientCollector;
};

TEST_F(UnixSocketChannelTestEnv, ConnectAndStates) {
    ASSERT_TRUE(connectPair());
    EXPECT_TRUE(server->isConnected());
    EXPECT_TRUE(client->isConnected());
}

TEST_F(UnixSocketChannelTestEnv, ClientToServerRequest) {
    ASSERT_TRUE(connectPair());

    uint64_t id = client->sendRequest("test.method", R"({"key":"value"})");
    EXPECT_GT(id, 0u);

    ASSERT_TRUE(serverCollector->waitFor(1));
    auto msg = serverCollector->at(0);
    EXPECT_EQ(msg.type, IpcMessageType::Request);
    EXPECT_EQ(msg.method, "test.method");
    EXPECT_EQ(msg.id, id);
    EXPECT_NE(msg.payload.find("value"), std::string::npos);
}

TEST_F(UnixSocketChannelTestEnv, ClientToServerEvent) {
    ASSERT_TRUE(connectPair());

    EXPECT_TRUE(client->sendEvent("some.event", R"({"n":1})"));
    ASSERT_TRUE(serverCollector->waitFor(1));
    auto msg = serverCollector->at(0);
    EXPECT_EQ(msg.type, IpcMessageType::Event);
    EXPECT_EQ(msg.method, "some.event");
    EXPECT_EQ(msg.id, 0u);
}

TEST_F(UnixSocketChannelTestEnv, ServerToClientResponse) {
    ASSERT_TRUE(connectPair());

    client->sendRequest("ping", "{}");

    IpcMessage response;
    response.type = IpcMessageType::Response;
    response.method = "ping";
    response.payload = R"({"ok":true})";
    response.id = 1;
    ASSERT_TRUE(server->send(response));

    ASSERT_TRUE(clientCollector->waitFor(1));
    auto msg = clientCollector->at(0);
    EXPECT_EQ(msg.type, IpcMessageType::Response);
    EXPECT_EQ(msg.method, "ping");
    EXPECT_NE(msg.payload.find("ok"), std::string::npos);
}

TEST_F(UnixSocketChannelTestEnv, MultipleMessagesInOrder) {
    ASSERT_TRUE(connectPair());

    for (int i = 0; i < 10; ++i) {
        client->sendRequest("m" + std::to_string(i), "{}");
    }
    ASSERT_TRUE(serverCollector->waitFor(10));
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(serverCollector->at(i).method, "m" + std::to_string(i));
    }
}

TEST_F(UnixSocketChannelTestEnv, SequentialRequestIds) {
    ASSERT_TRUE(connectPair());

    uint64_t id1 = client->sendRequest("a", "{}");
    uint64_t id2 = client->sendRequest("b", "{}");
    EXPECT_EQ(id2, id1 + 1);
}

TEST_F(UnixSocketChannelTestEnv, PayloadWithStringFallback) {
    ASSERT_TRUE(connectPair());

    // 非 JSON payload 应原样序列化传输
    client->sendRequest("raw", "not-a-json-payload");
    ASSERT_TRUE(serverCollector->waitFor(1));
    auto msg = serverCollector->at(0);
    EXPECT_EQ(msg.method, "raw");
    EXPECT_EQ(msg.payload, "not-a-json-payload");
}

TEST_F(UnixSocketChannelTestEnv, DisconnectPropagatesToPeer) {
    ASSERT_TRUE(connectPair());

    client->disconnect();
    EXPECT_FALSE(client->isConnected());

    // 服务端接收线程应感知到连接关闭并退出
    std::this_thread::sleep_for(200ms);
    server->disconnect();
    EXPECT_FALSE(server->isConnected());
}

TEST_F(UnixSocketChannelTestEnv, StartReceivingIdempotent) {
    ASSERT_TRUE(connectPair());

    // 重复 startReceiving 不应启动第二个线程
    server->startReceiving();
    client->startReceiving();
    std::this_thread::sleep_for(50ms);

    client->sendRequest("x", "{}");
    EXPECT_TRUE(serverCollector->waitFor(1));
}

TEST_F(UnixSocketChannelTestEnv, StopReceivingWithoutStartIsSafe) {
    ASSERT_TRUE(connectPair());
    EXPECT_NO_THROW(client->stopReceiving());
    // 断开后仍能安全析构
    client->disconnect();
    server->disconnect();
}

TEST_F(UnixSocketChannelTestEnv, SocketFileRemovedOnServerDisconnect) {
    ASSERT_TRUE(connectPair());
    client->disconnect();
    server->disconnect();
    EXPECT_FALSE(std::filesystem::exists(path));
}

#endif // !defined(_WIN32)
